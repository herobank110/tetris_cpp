// Tetris game with SDL2 for desktop and emscripten.
// Controls:
//    Down Arrow - Move Down
//    Left Arrow - Rotate Left
//    Right Arrow - Rotate Right

#include <array>
#include <chrono>
#include <iostream>
#include <memory>
#include <SDL2/SDL.h>
#include <vector>
#ifdef EMSCRIPTEN
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

SDL_Window* window;
SDL_Renderer* renderer;
std::unique_ptr<class Board> board;
std::unique_ptr<class GameMode> game_mode;

struct Point { float x, y; };
std::array pieces{1, 2, 3};

class Actor
{
public:
	void tick(float delta_time);
};

class Board : Actor
{
	constexpr static const int width = 10;
	constexpr static const int height = 21;
	std::vector<bool> data;

public:
	Board()
	{
		data.reserve(width * height);
		data.assign(width * height, false);

		data[0] = true;
		data[11] = true;
		data[22] = true;
		data[33] = true;
	}

	void tick(float delta_time)
	{
		const int grid_size = 20, padding = 2;

		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

		int w, h;
		SDL_GetWindowSize(window, &w, &h);
		const int right_edge = (w - grid_size * width) / 2;
		SDL_Rect rect{ right_edge, h - 2 * (grid_size - padding), grid_size - padding, grid_size - padding };

		for (int i = 0; i < data.size(); i++)
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
};

class GameMode : Actor
{
public:
	void tick(float delta_time)
	{
		const Uint8* state = SDL_GetKeyboardState(nullptr);
		if (state[SDL_SCANCODE_RETURN] == 1)
		{
			std::cout << "<RETURN> is pressed. delta time: " << delta_time << std::endl;
		}
		if (state[SDL_SCANCODE_RIGHT] == 1 && state[SDL_SCANCODE_UP] == 1)
		{
			std::cout << "Right and Up Keys Pressed." << std::endl;
		}
	}
};

void render_func()
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
		SDL_CreateWindowAndRenderer(800, 600, 0, &window, &renderer);

		board = std::make_unique<Board>();
		game_mode = std::make_unique<GameMode>();

#ifdef EMSCRIPTEN
		emscripten_set_main_loop(render_func, 0, true);
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
			render_func();
			SDL_Delay(1000 / 60); // assume 60fps
		}
#endif

		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
	}

	return 0;
}