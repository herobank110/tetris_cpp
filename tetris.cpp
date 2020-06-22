#include <vector>
#include <memory>
#include <SDL2/SDL.h>
#include <assert.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

SDL_Window *window;
SDL_Renderer *renderer;
std::unique_ptr<class Board> board;

class Board
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

    void print()
    {
        const int grid_size = 20, padding = 2;

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        const int right_edge = (w - grid_size * width) / 2;
        SDL_Rect rect{right_edge, h - 2 * (grid_size - padding), grid_size - padding, grid_size - padding};

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

void render_func(void)
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    board->print();

    SDL_RenderPresent(renderer);

#ifdef EMSCRIPTEN
    emscripten_cancel_main_loop();
#endif
}

int main()
{
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    SDL_CreateWindowAndRenderer(800, 600, 0, &window, &renderer);

    board = std::make_unique<Board>();

#ifdef EMSCRIPTEN
    emscripten_set_main_loop(render_func, 0, 1);
#else
    render_func();
#endif

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}