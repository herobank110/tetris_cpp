// Tetris game with SDL2 for desktop and emscripten.
// Controls:
//    Down Arrow - Move Down
//    Left Arrow - Move Left
//    Right Arrow - Move Right

#include <SDL2/SDL.h>
#include <array>
#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <vector>
#ifdef EMSCRIPTEN
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif
#include "tetris.hpp"

static const int fps = 20;
static const Location window_size{800, 600};
static const Color background_color{255};
static const Color foreground_color{113, 150, 107};
static const std::array pieces{Piece{{{0, 0}, {0, 1}, {0, 2}, {0, 3}}},
                               Piece{{{0, 0}, {1, 0}, {1, 1}, {2, 1}}}};

SDL_Window *window;
SDL_Renderer *renderer;
std::unique_ptr<class Board> board;
std::unique_ptr<class GameMode> game_mode;

class Actor
{
public:
  void tick(float delta_time);
};

class Board : Actor
{
public:
  Board()
  {
    data.reserve(width * height);
    data.assign(width * height, false);
  }

  void tick(float delta_time)
  {

    SDL_SetRenderDrawColor(renderer, foreground_color.r, foreground_color.g,
                           foreground_color.b, foreground_color.a);

    int screen_w;
    int screen_h;
    SDL_GetWindowSize(window, &screen_w, &screen_h);
    const int right_edge = (screen_w - grid_size * width) / 2;
    const int bottom_edge = screen_h - right_edge / 2 + 40;
    SDL_Rect rect{right_edge, bottom_edge, grid_size - padding,
                  -(grid_size - padding * 2)};

    for (std::size_t i = 0; i < data.size(); i++)
    {
      if (i != 0)
      {
        rect.x += grid_size;
        if (i % width == 0)
        {
          // New row above.
          rect.y -= grid_size;
          rect.x = right_edge;
        }
      }

      if (data[i])
      {
        rect.h--;
        SDL_RenderFillRect(renderer, &rect);
        rect.h++;
      }
      else
      {
        SDL_RenderDrawRect(renderer, &rect);
      }
    }

    // Draw a border around the grid.
    rect.x = right_edge - margin;
    rect.y = bottom_edge + margin;
    rect.w = grid_size * Board::width + margin * 2 - padding;
    rect.h = -(grid_size * Board::height + margin * 2 - padding * 2);
    SDL_RenderDrawRect(renderer, &rect);
  }

  void stamp_values(const Piece &piece, const bool new_value)
  {
    for (const auto &p : piece.get_world_parts())
    {
      set_value_at(p, new_value);
    }
  };

  [[nodiscard]] static auto location_to_index(const Location &loc)
      -> std::size_t
  {
    return loc.y * width + loc.x;
  }

  [[nodiscard]] auto get_value_at(const Location &world_location) const
      -> std::optional<bool>
  {
    if (auto index = location_to_index(world_location); index < data.size())
    {
      return data[index];
    }
    return {};
  }

  auto set_value_at(const Location &world_location, const bool new_value)
      -> bool
  {
    auto index = location_to_index(world_location);
    if (index < data.size())
    {
      data[index] = new_value;
      return true;
    }
    return false;
  }

  constexpr static const int width = 10;
  constexpr static const int height = 21;
  constexpr static const int grid_size = 22;
  constexpr static const int padding = 2;
  constexpr static const int margin = 4;
  std::vector<bool> data;
};

class GameMode : Actor
{
public:
  void choose_new_piece()
  {
    // Makes a copy to adjust world location on the member variable.
    // player_piece = pieces[rand() % pieces.size()];
    player_piece = pieces[0];

    // Move to the top center of the board.
    player_piece.value().location.x = Board::width / 2;
    player_piece.value().location.y = Board::height;

    // Shift down until not colliding.
    while (player_piece.value().has_collision())
    {
      player_piece.value().location.y--;
    }
  }

  void tick_new_piece(float delta_time)
  {
    current_new_piece_time += delta_time;
    if (current_new_piece_time > new_piece_delay)
    {
      current_new_piece_time = 0.F;
      choose_new_piece();
    }
  }

  void tick_current_piece(float delta_time)
  {
    auto &piece = player_piece.value();

    // Reset previous piece state.
    board->stamp_values(piece, false);

    bool has_landed = false;
    // Fall automatically after a repeat delay.
    current_fall_time += delta_time;
    if (current_fall_time > fall_delay)
    {
      current_fall_time = 0.F;
      if (!piece.add_offset({0, -1}))
      {
        has_landed = true;
      }
    }

    const Uint8 *state = SDL_GetKeyboardState(nullptr);
    if (state[SDL_SCANCODE_DOWN] == 1)
    {
      // Drop lower if user holding down key but don't consider
      // as 'landed' from user down input.
      piece.add_offset({0, -1});
    }
    if (piece.location.x > 0 && state[SDL_SCANCODE_LEFT] == 1)
    {
      piece.add_offset({-1, 0});
    }
    else if (piece.location.x < Board::width - 1 &&
             state[SDL_SCANCODE_RIGHT] == 1)
    {
      piece.add_offset({1, 0});
    }
    // TODO: Piece rotation

    if (has_landed)
    {
      // This time the player landed on an occupied spot.

      // Set final state on board.
      board->stamp_values(piece, true);
      player_piece.reset();
    }

    // Set new state on board.
    board->stamp_values(piece, true);
  }

  auto can_end_match() const -> bool
  {
    // End game if anything on the 4th row down is occupied.
    // I don't know if this is part of the official rules.

    Location loc{0, Board::height - 4};
    for (int i = 0; i < Board::width; i++)
    {
      if (board->get_value_at(loc).value_or(false))
      {
        // A spot is occupied, so we can end the game.
        return true;
      }
    }
    // Nothing on this row was occupied, so continue play.
    return false;
  }

  void tick(float delta_time)
  {
    if (is_match_over)
    {
      return;
    }
    if (can_end_match())
    {
      is_match_over = true;
      return;
    }

    if (player_piece.has_value())
    {
      // Otherwise move the current piece.
      tick_current_piece(delta_time);
    }
    else
    {
      // Try to give the player a new piece.
      tick_new_piece(delta_time);
    }
  }

  constexpr const static float fall_delay = 1.F;
  constexpr const static float new_piece_delay = 1.5F;
  float current_fall_time = 0.F;
  float current_new_piece_time = 0.F;
  std::optional<Piece> player_piece;
  bool is_match_over = false;
};

[[nodiscard]] auto Piece::has_collision() const -> bool
{
  auto parts = get_world_parts();
  return !std::all_of(parts.begin(), parts.end(), [](const Location &loc) {
    return
        // Ensure the piece is within the board bounds.
        0 <= loc.x && loc.x < Board::width && 0 <= loc.y &&
        loc.y < Board::height

        // Ensure the spot isn't occupied.
        && !(board->get_value_at(loc).value_or(false));
  });
}

void main_loop()
{
  SDL_SetRenderDrawColor(renderer, background_color.r, background_color.g,
                         background_color.b, background_color.a);
  SDL_RenderClear(renderer);

  float delta_time = 0.F;
#ifdef EMSCRIPTEN
  static double last_time = emscripten_performance_now();
  const double time_now = emscripten_performance_now();
  delta_time = static_cast<float>(time_now - last_time);
  // Convert milliseconds to seconds.
  delta_time *= 0.001;
  last_time = time_now;
#else
  static auto last_time = std::chrono::high_resolution_clock::now();
  const auto time_now = std::chrono::high_resolution_clock::now();
  delta_time = std::chrono::duration<float>(time_now - last_time).count();
  last_time = time_now;
#endif

  board->tick(delta_time);
  game_mode->tick(delta_time);

  SDL_RenderPresent(renderer);
}

#undef main // from SDL2
auto main(int argc, char *argv[]) -> int
{
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0)
  {
    SDL_CreateWindowAndRenderer(window_size.x, window_size.y, 0, &window,
                                &renderer);

    board = std::make_unique<Board>();
    game_mode = std::make_unique<GameMode>();
    game_mode->choose_new_piece();

#ifdef EMSCRIPTEN
    emscripten_set_main_loop(main_loop, fps, true);
#else // normal SDL
    SDL_Event event;
    bool wants_to_quit = false;
    while (!wants_to_quit)
    {
      while (SDL_PollEvent(&event) == 1)
      {
        if (event.type == SDL_QUIT)
        {
          wants_to_quit = true;
        }
      }
      main_loop();
      static_assert(fps > 0, "fps must be greater than zero");
      const int milliseconds_per_second = 1000;
      SDL_Delay(milliseconds_per_second / fps); // assume 60fps
    }
#endif

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
  }

  return 0;
}
