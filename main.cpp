#include <iostream>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_main.h>

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture* texture;
SDL_Event event;
bool isRunning = true;

int playerX = 320; // Initial X position
int playerY = 240; // Initial Y position

void handleInput() {
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            isRunning = false;
        }
        if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
                case SDLK_w:
                    // Move up
                    playerY -= 10;
                    break;
                case SDLK_a:
                    // Move left
                    playerX -= 10;
                    break;
                case SDLK_s:
                    // Move down
                    playerY += 10;
                    break;
                case SDLK_d:
                    // Move right
                    playerX += 10;
                    break;
                default:
                    break;
            }
        }
    }
}

void render() {
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderClear(renderer);

    // just playing with moving things around on the screen
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawLine(renderer, playerX, playerY, playerX + 50, playerY);

    //rendering a tile
    if (texture != nullptr) {
      SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    }

    // Draw other elements here.
    SDL_RenderPresent(renderer);

}

int main(int argc, char* argv[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    // Create a window
    SDL_Window* window = SDL_CreateWindow("hexagonal tesselation",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, 0);

    if (!window) {
        SDL_Log("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    // initialize the renderer variable (already declared globally)
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    // Load the test-tile PNG image
    SDL_Surface* imageSurface = IMG_Load("assets/test-tile.png");
    if (imageSurface == nullptr) {
        // Handle error
        SDL_Quit();
        return 1;
    }

    // Create a texture from the loaded image
    texture = SDL_CreateTextureFromSurface(renderer, imageSurface);
    SDL_FreeSurface(imageSurface);

    if (!renderer) {
        SDL_Log("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    while (isRunning) {
        handleInput();
        render();
    }

    // Cleanup and quit
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
