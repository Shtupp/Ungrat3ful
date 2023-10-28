#include <iostream>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_main.h>

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture* testTile;
SDL_Event event;
bool isRunning = true;

int playerX = 320; // Initial X position
int playerY = 240; // Initial Y position

int winInitWidth = 800;
int winInitHeight = 600;

int** mapArr;
int numCols;
int numRows;

//prototype functions here for scoping 
void RenderTileMap(SDL_Renderer* renderer, SDL_Texture* tileset, int tileWidth, int** tilemap); 

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

    //rendering the map. I suspect this is inefficient. need to find how to just render "statically" 
    if (testTile != nullptr) {
      //SDL_RenderCopy(renderer, testTile, nullptr, nullptr);
      RenderTileMap(renderer, testTile, 100, mapArr);
    }

    // just playing with moving things around on the screen
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawLine(renderer, playerX, playerY, playerX + 50, playerY);

    // Draw other elements here.
    SDL_RenderPresent(renderer);
}

void RenderTileMap(SDL_Renderer* renderer, SDL_Texture* tileset, int tileWidth, int** tileMap) {
    int mapHeight = sizeof(tileMap) / sizeof(tileMap[0]);
    int mapWidth = sizeof(tileMap[0]) / sizeof(tileMap[0][0]);

    // Iterate through the tileMap
    for (int y = 0; y < numCols; ++y) {
        for (int x = 0; x < numRows; ++x) {
            // Calculate the source and destination rectangles for the current tile
            SDL_Rect destRect = { x * tileWidth, y * tileWidth, tileWidth, tileWidth };

            // Render the tile
            SDL_RenderCopy(renderer, tileset, nullptr, &destRect);
        }
    }
}

//here we initialize the mapArr var using dynamic memory allocation
//POTENTIAL FOR MEMORY LEAKS BE CAREFUL
int** initMapArr(int winWidth, int winHeight, int tileDem) {
    int** tessa;
    numRows = winWidth/tileDem;
    numCols = winHeight/tileDem;

    std::cout << "Number of rows: " << numRows << std::endl;
    std::cout << "Number of columns: " << numCols << std::endl;

    // Allocate memory for the array (my baby tessa).
    tessa = new int*[numRows];
    for (int i = 0; i < numRows; i++) {
        tessa[i] = new int[numCols];
    }

    // Initialize the array.
    for (int i = 0; i < numRows; i++) {
        for (int j = 0; j < numCols; j++) {
            tessa[i][j] = i * numCols + j;  // piece of work. needs to be understood
        }
    }

    return tessa;
}

int main(int argc, char* argv[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    // Create a window
    SDL_Window* window = SDL_CreateWindow("hexagonal tesselation",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, winInitWidth, winInitHeight, 0);

    if (!window) {
        SDL_Log("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    // initialize the renderer variable (already declared globally)
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    //the mapArr var is initialized here so it can be used to draw the map
    mapArr = initMapArr(winInitWidth, winInitHeight, 100);

    // Load the tile-test PNG image
    SDL_Surface* imageSurface = IMG_Load("assets/tile-test.png");
    if (imageSurface == nullptr) {
        throw std::runtime_error("Failed to load image (check ur file names & paths): " + std::string(IMG_GetError()));
        SDL_Quit();
        return 1;
    }

    // Create a texture named testTile from the loaded image
    testTile = SDL_CreateTextureFromSurface(renderer, imageSurface);
    SDL_FreeSurface(imageSurface);

    if (!renderer) {
        SDL_Log("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    while (isRunning) {
        handleInput();
        render();
    }
    std::cout << "checkpoint";

    // Cleanup and quit
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    // Don't forget to deallocate the memory when you're done.
    for (int i = 0; i < winInitHeight/100; i++) {
        delete[] mapArr[i];
    }
    delete[] mapArr;

    return 0;
}
