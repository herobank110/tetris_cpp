// Tetris game with SDL2 for desktop and emscripten.
// Controls:
//    Down Arrow - Move Down
//    Left Arrow - Move Left
//    Right Arrow - Move Right

#include <array>
#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <SDL2/SDL.h>
#include <vector>
#ifdef EMSCRIPTEN
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif
#include <optional>

SDL_Window* window;
SDL_Renderer* renderer;
std::unique_ptr<class Board> board;
std::unique_ptr<class GameMode> game_mode;
constexpr const int fps = 12;

struct Location {
	int x = 0, y = 0;
	Location() = default;
	Location(int x, int y) : x(x), y(y) {};
};

struct Piece
{
	Piece(std::initializer_list<Location>&& t)
		: parts{ std::move(t) }

	{
		location = Location{ 0, 0 };
	}

	[[nodiscard]] auto get_world_parts() const {
		std::vector<Location> vec;
		for (const auto& p : parts)
		{
			vec.emplace_back(p.x + location.x, p.y + location.y);
		}
		return vec;
	}

	std::vector<Location> parts;
	Location location;
};

const std::array pieces{
	Piece{{ { 0, 0 }, { 0, 1 }, { 0, 2 }, {0, 3} }},
	Piece{{ { 0, 0 }, { 1, 0 }, { 1, 1 }, { 2, 1 } }}
};

class Actor
{
public:
	void tick(float delta_time);
};

class Board : Actor
{
public:
	constexpr static const int width = 10;
	constexpr static const int height = 21;
	std::vector<bool> data;

	Board()
	{
		data.reserve(width * height);
		data.assign(width * height, false);
	}

	void tick(float delta_time)
	{
		const int grid_size = 20, padding = 2;

		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

		int w, h;
		SDL_GetWindowSize(window, &w, &h);
		const int right_edge = (w - grid_size * width) / 2;
		SDL_Rect rect{ right_edge, h - 2 * (grid_size - padding), grid_size - padding, grid_size - padding };

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
				SDL_RenderFillRect(renderer, &rect);
			}
			else
			{
				SDL_RenderDrawRect(renderer, &rect);
			}
		}
	}

	void stamp_values(const Piece& piece, const bool new_value)
	{
		for (const auto& p : piece.get_world_parts())
		{
			set_value_at(p, new_value);
		}
	};

	std::size_t location_to_index(const Location& loc) const { return loc.y * width + loc.x; }

	std::optional<bool> get_value_at(const Location& world_location) const
	{
		if (auto index = location_to_index(world_location);
			index < data.size())
		{
			return data[index];
		}
		return {};
	}

	bool set_value_at(const Location& world_location, const bool new_value)
	{
		auto index = location_to_index(world_location);
		if (index < data.size())
		{
			data[index] = new_value;
			return true;
		}
		return false;
	}
};

class GameMode : Actor
{
public:
	void choose_new_piece()
	{
		// Makes a copy to adjust world location on the member variable.
		//player_piece = pieces[rand() % pieces.size()];
		player_piece = pieces[0];

		// Move to the top of the board.
		player_piece.value().location.y = Board::height;

		// Shift down until not colliding.
		while (has_collision(player_piece.value()))
		{
			player_piece.value().location.y--;
		}
	}

	bool has_collision(const Piece& piece)
	{
		auto parts = piece.get_world_parts();
		return !std::all_of(parts.begin(), parts.end(),
			[](const Location& loc)
			{
				return
					// Ensure the piece is within the board bounds.
					0 <= loc.x && loc.x < Board::width
					&& 0 <= loc.y && loc.y < Board::height

					// Ensure the spot isn't occupied.
					&& !(board->get_value_at(loc).value_or(false));
			}
		);
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
		auto& piece = player_piece.value();

		// Adjust location and revert if collision would occur.
		auto add_offset = [&](int dx, int dy)
		{
			const int sweep_x = dx < 0 ? -1 : dx > 0 ? 1 : 0;
			const int sweep_y = dy < 0 ? -1 : dy > 0 ? 1 : 0;

			// Sweep to the ending location.
			while (!(dx == 0 && dy == 0))
			{
				if (sweep_x != 0)
				{
					piece.location.x += sweep_x;
					if (has_collision(piece))
					{
						piece.location.x -= sweep_x;
						return false;
					}
					dx -= sweep_x;
				}

				if (sweep_y != 0)
				{
					piece.location.y += sweep_y;
					if (has_collision(piece))
					{
						piece.location.y -= sweep_y;
						return false;
					}
					dy -= sweep_y;
				}
			}
			return true;
		};

		// Reset previous piece state.
		board->stamp_values(piece, false);

		bool has_landed = false;
		// Fall automatically after a repeat delay.
		current_fall_time += delta_time;
		if (current_fall_time > fall_delay)
		{
			current_fall_time = 0.F;
			has_landed = has_landed || !add_offset(0, -1);
		}

		const Uint8* state = SDL_GetKeyboardState(nullptr);
		if (state[SDL_SCANCODE_DOWN] == 1)
		{
			// Drop lower if user holding down key.
			has_landed = has_landed || !add_offset(0, -1);
		}
		if (piece.location.x > 0 && state[SDL_SCANCODE_LEFT] == 1)
		{
			add_offset(-1, 0);
		}
		else if (piece.location.x < Board::width - 1 && state[SDL_SCANCODE_RIGHT] == 1)
		{
			add_offset(1, 0);
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

	void tick(float delta_time)
	{
		if (!player_piece.has_value())
		{
			// Try to give the player a new piece.
			tick_new_piece(delta_time);
		}
		else
		{
			// Otherwise move the current piece.
			tick_current_piece(delta_time);
		}
	}

	std::optional<Piece> player_piece;
	float current_fall_time = 0.F;
	float current_new_piece_time = 0.F;
	constexpr const static float fall_delay = 0.2;
	constexpr const static float new_piece_delay = 0.5;
};

void main_loop()
{
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	SDL_RenderClear(renderer);

	float delta_time = 0.F;
#ifdef EMSCRIPTEN
	static double last_time = emscripten_performance_now();
	const double time_now = emscripten_performance_now();
	delta_time = static_cast<float>(time_now - last_time);
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

#undef main  // from SDL2
int main(int argc, char* argv[])
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0)
	{
		static const Location window_size{ 800, 600 };
		SDL_CreateWindowAndRenderer(window_size.x, window_size.y, 0, &window, &renderer);

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
			SDL_Delay(1000 / fps); // assume 60fps
		}
#endif

		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
	}

	return 0;
}