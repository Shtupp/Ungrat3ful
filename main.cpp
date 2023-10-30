#include <iostream>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_main.h>

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture* testTile;
SDL_Texture* plaOnePNG;
SDL_Event event;
bool isRunning = true;

int plaOneX = 30;
int plaOneY = 3;
int playerX = 320; // Initial X position
int playerY = 240; // Initial Y position

int winInitWidth = 850;
int winInitHeight = 600;

int** mapArr;
int numCols;
int numRows;

//prototype functions here for scoping 
void RenderTileMap(SDL_Renderer* renderer, SDL_Texture* tileset, int tileWidth, int** tilemap);

// Function to modify the player's x and y coordinates
// In the future, could be improved by passing a player object as one of the paramater variables and modifying that directly
void movePlayer(int x, int y) {
    plaOneX += x;
    plaOneY += y;
}

void HandleMouseClick(SDL_Event& event) {
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);

        // Move the character to the adjacent tile in the clicked direction.
        if ((mouseY > plaOneY + 47) && (abs(mouseX - plaOneX) < 50)) {
            movePlayer(0, 100);
        }
        else if ((mouseY < plaOneY + 47) && (abs(mouseX - plaOneX) < 50)) {
            movePlayer(0, -100);
        }
        else if ((mouseY > plaOneY + 47) && (mouseX > plaOneX + 20))
            movePlayer(75, 50);
        else if ((mouseY < plaOneY + 47) && (mouseX > plaOneX + 20))
            movePlayer(75, -50);
        else if ((mouseY < plaOneY + 47) && (mouseX < plaOneX + 20))
            movePlayer(-75, -50);
        else if ((mouseY > plaOneY + 47) && (mouseX < plaOneX + 20))
            movePlayer(-75, 50);

    }
}

void handleInput() {
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            isRunning = false;
        }
        HandleMouseClick(event);
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

SDL_Texture* LoadImageAsTexture(const char* imagePath) {
    // Load the image from the provided file path
    SDL_Surface* imageSurface = IMG_Load(imagePath);

    if (imageSurface == nullptr) {
        SDL_Log("Failed to load image from path. Check spelling. SDL_Error: %s\n", IMG_GetError());
        throw 1;
    }

    // Create a texture from the loaded image
    SDL_Texture* imageTexture = SDL_CreateTextureFromSurface(renderer, imageSurface);
    SDL_FreeSurface(imageSurface);

    if (imageTexture == nullptr) {
        SDL_Log("Failed to create texture from image. SDL_Error: %s\n", SDL_GetError());
        throw 1;
    }

    return imageTexture;
}

void render() {
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderClear(renderer);

    //rendering the map. I suspect this is inefficient. need to find how to just render "statically" 
    if (testTile != nullptr) {
        //SDL_RenderCopy(renderer, testTile, nullptr, nullptr);
        RenderTileMap(renderer, testTile, 100, mapArr);
    }

    SDL_Rect plaOneDest = { plaOneX, plaOneY, 41, 94 };
    SDL_RenderCopy(renderer, plaOnePNG, nullptr, &plaOneDest);

    // just playing with moving things around on the screen
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawLine(renderer, playerX, playerY, playerX + 50, playerY);

    // Draw other elements here.
    SDL_RenderPresent(renderer);
}

void RenderTileMap(SDL_Renderer* renderer, SDL_Texture* tileset, int tileWidth, int** tileMap) {
    SDL_Rect destRect;
    int mapHeight = sizeof(tileMap) / sizeof(tileMap[0]);
    int mapWidth = sizeof(tileMap[0]) / sizeof(tileMap[0][0]);

    // Iterate through the tileMap
    for (int y = 0; y < numCols; ++y) {
        int rowLen = (y % 2 == 1) ? numRows - 2 : numRows;
        for (int x = 0; x < rowLen; ++x) {
            // Calculate the source and destination rectangles for the current tile
            if (y % 2 == 0) {
                destRect = { x * tileWidth + (x * (tileWidth / 2)), y * (tileWidth / 2), tileWidth, tileWidth };
            }
            else {
                destRect = { x * tileWidth + (x * (tileWidth / 2)) + ((tileWidth / 4) * 3), y * tileWidth - (y * (tileWidth / 2)), tileWidth, tileWidth };
            }
            // Render the tile
            SDL_RenderCopy(renderer, tileset, nullptr, &destRect);
        }
    }
}

//here we initialize the mapArr var using dynamic memory allocation
//POTENTIAL FOR MEMORY LEAKS BE CAREFUL
int** initMapArr(int winWidth, int winHeight, int tileDem) {
    int** tessa;
    numRows = winWidth / tileDem;
    numCols = (winHeight / tileDem) * 2;

    std::cout << "Number of rows: " << numRows << std::endl;
    std::cout << "Number of columns: " << numCols << std::endl;

    // Allocate memory for the array (my baby tessa).
    tessa = new int* [numRows];
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

    testTile = LoadImageAsTexture("assets/tile-test.png");
    plaOnePNG = LoadImageAsTexture("assets/ancp-male-test.png");


    if (!renderer) {
        SDL_Log("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    while (isRunning) {
        handleInput();
        render();
    }
    //std::cout << "checkpoint";

    // Cleanup and quit
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    // Don't forget to deallocate the memory when you're done.
    for (int i = 0; i < winInitHeight / 100; i++) {
        delete[] mapArr[i];
    }
    delete[] mapArr;

    return 0;
}
