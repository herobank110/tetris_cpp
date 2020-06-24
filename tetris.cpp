// Tetris game with SDL2 for desktop and emscripten.
// Controls:
//    Down Arrow - Move Down
//    Left Arrow - Move Left
//    Right Arrow - Move Right
//    Up Arrow - Rotate

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
static const SDL_Point window_size{ 800, 600 };
static const SDL_Color background_color{ 255, 255, 255, 255 };
static const SDL_Color foreground_color{ 113, 150, 107, 255 };
static const SDL_Color foreground_color_alt{ 150, 113, 97, 255 };
static const std::array pieces{
  Piece{ { { 0, 0 }, { 0, 1 }, { 1, 1 }, { 0, 2 } } },
  Piece{ { { 0, 0 }, { 1, 0 }, { 1, -1 }, { 2, -1 } } },
  Piece{ { { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 } } },
  Piece{ { { 0, 0 }, { 0, 1 }, { 0, 2 }, { 0, 3 } } },
  Piece{ { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 2, 1 } } }
};

static SDL_Window* window;
static SDL_Renderer* renderer;
static std::unique_ptr<class Board> board;
static std::unique_ptr<class GameMode> game_mode;

Piece::Piece(std::initializer_list<SDL_Point>&& t) noexcept
  : parts{ std::make_shared<std::vector<SDL_Point>>(std::move(t)) }
{}

[[nodiscard]] auto Piece::get_world_parts() const
{
  std::vector<SDL_Point> vec{};
  vec.reserve(parts->size());
  std::transform(
    parts->begin(),
    parts->end(),
    std::back_inserter(vec),
    [this](const SDL_Point& p) {
      // Apply rotation to reference piece.
      SDL_Point loc;
      switch (rotation)
      {
      case 0: loc = { p.x, p.y }; break;
      case 1: loc = { p.y, -p.x }; break;
      case 2: loc = { -p.x, -p.y }; break;
      case 3: loc = { -p.y, p.x }; break;
      default: throw std::runtime_error("Rotation must be between 0 and 3");
      };

      // Add world origin offset.
      loc.x += location.x;
      loc.y += location.y;
      return loc;
    });
  return vec;
}

auto Piece::add_offset(SDL_Point offset) -> bool
{
  const SDL_Point sweep{ offset.x < 0 ? -1 : offset.x > 0 ? 1 : 0,
                         offset.y < 0 ? -1 : offset.y > 0 ? 1 : 0 };

  while (!(offset.x == 0 && offset.y == 0))
  {
    if (sweep.x != 0)
    {
      location.x += sweep.x;
      if (has_collision())
      {
        location.x -= sweep.x;
        return false;
      }
      offset.x -= sweep.x;
    }

    if (sweep.y != 0)
    {
      location.y += sweep.y;
      if (has_collision())
      {
        location.y -= sweep.y;
        return false;
      }
      offset.y -= sweep.y;
    }
  }
  return true;
};

[[nodiscard]] auto Piece::has_collision() const -> bool
{
  auto parts = get_world_parts();
  return !std::all_of(parts.begin(), parts.end(), [](const SDL_Point& loc) {
    return
      // Ensure the piece is within the board bounds.
      0 <= loc.x && loc.x < Board::width && 0 <= loc.y &&
      loc.y < Board::height

      // Ensure the spot isn't occupied.
      && !(board->get_value_at(loc).value_or(false));
  });
}

Board::Board()
{
  data.reserve(width * height);
  data.assign(width * height, false);
}

auto Board::try_eliminate_rows() -> int
{
  std::vector<int> complete_rows;
  for (SDL_Point loc{ 0, 0 }; loc.y < Board::height; loc.y++)
  {
    bool row_has_empty_space = false;
    for (loc.x = 0; loc.x < Board::width; loc.x++)
    {
      if (!get_value_at(loc).value_or(false))
      {
        row_has_empty_space = true;
        break;
      }
    }
    if (!row_has_empty_space)
    {
      // Mark this row to be eliminated.
      complete_rows.push_back(loc.y);
    }
  }

  // Eliminate the rows.
  if (!complete_rows.empty())
  {
    int shift_down = 0;
    SDL_Point loc{ 0, 0 };
    for (const int& row_index : complete_rows)
    {
      loc.y = row_index - shift_down;
      const auto& row_start = data.begin() + location_to_index(loc);
      data.erase(row_start, row_start + Board::width);
      // Shift completed row indexes down considering this row was just removed.
      shift_down++;
    }
    // Fill in erased rows at the top to match the board dimensions.
    data.resize(Board::width * Board::height, false);
  }
  // Return how many rows were eliminated.
  return complete_rows.size();
}

void Board::tick(float delta_time)
{
  const auto& color =
    game_mode->is_match_over ? foreground_color_alt : foreground_color;
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

  int screen_w;
  int screen_h;
  SDL_GetWindowSize(window, &screen_w, &screen_h);
  const int right_edge = (screen_w - grid_size * width) / 2;
  const int bottom_edge = screen_h - right_edge / 2 + 40;
  SDL_Rect rect{
    right_edge, bottom_edge, grid_size - padding, -(grid_size - padding * 2)
  };

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

void Board::stamp_values(const Piece& piece, const bool& new_value)
{
  for (const auto& p : piece.get_world_parts())
  {
    set_value_at(p, new_value);
  }
};

[[nodiscard]] auto Board::location_to_index(const SDL_Point& loc) -> std::size_t
{
  return static_cast<size_t>(loc.y * width + loc.x);
}

auto Board::get_value_at(const SDL_Point& world_location) const
  -> std::optional<bool>
{
  if (auto index = location_to_index(world_location); index < data.size())
  {
    return data[index];
  }
  return {};
}

auto Board::set_value_at(const SDL_Point& world_location, const bool& new_value)
  -> bool
{
  if (auto index = location_to_index(world_location); index < data.size())
  {
    data[index] = new_value;
    return true;
  }
  return false;
}

void GameMode::choose_new_piece()
{
  // Makes a copy to adjust world location on the member variable.
  player_piece = pieces[rand() % pieces.size()];

  // Move to the top center of the board.
  player_piece.value().location.x = Board::width / 2;
  player_piece.value().location.y = Board::height;

  // Shift down until not colliding.
  while (player_piece.value().has_collision())
  {
    player_piece.value().location.y--;
  }
}

void GameMode::tick_new_piece(float delta_time)
{
  current_new_piece_time += delta_time;
  if (current_new_piece_time > new_piece_delay)
  {
    current_new_piece_time = 0.F;
    choose_new_piece();
  }
}

void GameMode::handle_input()
{
  // Collision testing is handled in `piece.add_offset` which will sweep until
  // it hits something.
  auto& piece = player_piece.value();
  const Uint8* keyboard_state = SDL_GetKeyboardState(nullptr);
  if (keyboard_state[SDL_SCANCODE_DOWN] == 1)
  {
    // Drop lower if user holding down key but don't consider
    // as 'landed' from user down input.
    piece.add_offset({ 0, -1 });
  }
  if (keyboard_state[SDL_SCANCODE_LEFT] == 1)
  {
    piece.add_offset({ -1, 0 });
  }
  else if (keyboard_state[SDL_SCANCODE_RIGHT] == 1)
  {
    piece.add_offset({ 1, 0 });
  }
  else if (keyboard_state[SDL_SCANCODE_UP] == 1)
  {
    auto try_loop_rotation = [&] {
      if (piece.rotation < 0)
      {
        piece.rotation = 3;
      }
      else if (piece.rotation > 3)
      {
        // Loop back round as 4 * 90 degrees is a full turn.
        piece.rotation = 0;
      }
    };

    piece.rotation += 1;
    try_loop_rotation();
    if (piece.has_collision())
    {
      piece.rotation -= 1;
    }
    // Loop back round if collided and lowered rotations.
    try_loop_rotation();
  }
}

void GameMode::tick_current_piece(float delta_time)
{
  auto& piece = player_piece.value();

  // Reset previous piece state.
  board->stamp_values(piece, false);

  bool has_landed = false;
  // Fall automatically after a repeat delay.
  current_fall_time += delta_time;
  if (current_fall_time > fall_delay)
  {
    current_fall_time = 0.F;
    if (!piece.add_offset({ 0, -1 }))
    {
      has_landed = true;
    }
  }

  handle_input();

  if (has_landed)
  {
    // This time the player landed on an occupied spot.

    // Set final state on board.
    board->stamp_values(piece, true);
    player_piece.reset();
    board->tick(delta_time);
  }
  else
  {
    // Set new state on board.
    board->stamp_values(piece, true);
    // Draw board then clear temporary values.
    board->tick(delta_time);
    board->stamp_values(piece, false);
  }
}

[[nodiscard]] auto GameMode::can_end_match() const -> bool
{
  // End game if anything on the 4th row down is occupied.
  // I don't know if this is part of the official rules.

  SDL_Point loc{ 0, Board::height - 4 };
  for (int i = 0; i < Board::width; i++)
  {
    loc.x = i;
    if (board->get_value_at(loc).value_or(false))
    {
      // A spot is occupied, so we can end the game.
      return true;
    }
  }
  // Nothing on this row was occupied, so continue play.
  return false;
}

void GameMode::tick(float delta_time)
{
  if (is_match_over)
  {
    board->tick(delta_time);

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
    board->try_eliminate_rows();
    tick_current_piece(delta_time);
  }
  else
  {
    // Try to give the player a new piece.
    tick_new_piece(delta_time);
    // TODO: Fix temporary player piece marker on board so board state
    // doesn't have actually be adjusted for game logic.
    board->tick(delta_time);
  }
}

void main_loop()
{
  SDL_SetRenderDrawColor(
    renderer,
    background_color.r,
    background_color.g,
    background_color.b,
    background_color.a);
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

  game_mode->tick(delta_time);

  SDL_RenderPresent(renderer);
}

#undef main // from SDL2
auto main() -> int
{
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0)
  {
    SDL_CreateWindowAndRenderer(
      window_size.x, window_size.y, 0, &window, &renderer);

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
