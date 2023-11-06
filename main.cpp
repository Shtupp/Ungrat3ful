#include <iostream>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_main.h>
#include <vector>
#include <array>
#include <unordered_set>
#include <boost/functional/hash.hpp>
#include <random>

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Event event;
bool isRunning = true;
int cursorX;
int cursorY;
std::array<int, 2> activePos;

int plaOneX = 30;
int plaOneY = 3;
int playerX = 320; // Initial X position
int playerY = 240; // Initial Y position

int numCols;
int numRows;

//prototype functions here for scoping 
void RenderTileMap(SDL_Renderer* renderer, SDL_Texture* tileset, int tileWidth, int** tilemap);

// Function to modify an object's x and y coordinates
void transformObj(int x, int y, int& objX, int& objY) {
    objX += x;
    objY += y;
}

// https://www.redblobgames.com/grids/hexagons/implementation.html#hex
// the int w parameter is used to distinguish between positions and vectors. Not useful yet but worth keeping around
template <typename Number, int w>
struct _Hex {
    union {
        const Number v[3];
        struct { const Number q, r, s; };
    };

    std::string decoration;

    bool operator==(const _Hex& other) const {
        return q == other.q && r == other.r && s == other.s;
    }
    _Hex(Number q_, Number r_) : v{q_, r_, -q_ - r_} {
        decoration = "";
    }
    _Hex(Number q_, Number r_, Number s_) : v{q_, r_, s_} {
        decoration = "";
    }
};

using Hex = _Hex<int, 1>;
using FracHex = _Hex<double, 1>;

struct HexHash {
    std::size_t operator()(const Hex& hex) const {
        std::size_t seed = 0;
        // Combine the hashes of q, r, and s using a hash combining function
        boost::hash_combine(seed, hex.q);
        boost::hash_combine(seed, hex.r);
        boost::hash_combine(seed, hex.s);
        return seed;
    }
};

// should be in the file's header but oh well
std::unordered_set<Hex, HexHash> mapSet;

// https://www.redblobgames.com/grids/hexagons/implementation.html#hex-arithmetic
Hex hex_subtract(Hex a, Hex b) {
    return Hex(a.q - b.q, a.r - b.r, a.s - b.s);
}

// https://www.redblobgames.com/grids/hexagons/implementation.html#hex-distance
int hex_length(Hex hex) {
    return int((abs(hex.q) + abs(hex.r) + abs(hex.s)) / 2);
}

int hex_distance(Hex a, Hex b) {
    return hex_length(hex_subtract(a, b));
}

// https://www.redblobgames.com/grids/hexagons/implementation.html#layout
struct Orientation {
    const double f0, f1, f2, f3;
    const double b0, b1, b2, b3;
    const double start_angle; // in multiples of 60°
    Orientation(double f0_, double f1_, double f2_, double f3_,
                double b0_, double b1_, double b2_, double b3_,
                double start_angle_)
    : f0(f0_), f1(f1_), f2(f2_), f3(f3_),
      b0(b0_), b1(b1_), b2(b2_), b3(b3_),
      start_angle(start_angle_) {}
};

// layout_flat makes sense for our art rendering on top of tiles but might as well include both
const Orientation layout_pointy
  = Orientation(sqrt(3.0), sqrt(3.0) / 2.0, 0.0, 3.0 / 2.0,
                sqrt(3.0) / 3.0, -1.0 / 3.0, 0.0, 2.0 / 3.0,
                0.5);
const Orientation layout_flat
  = Orientation(3.0 / 2.0, 0.0, sqrt(3.0) / 2.0, sqrt(3.0),
                2.0 / 3.0, 0.0, -1.0 / 3.0, sqrt(3.0) / 3.0,
                0.0);

struct Point {
    const double x, y;
    Point(double x_, double y_): x(x_), y(y_) {}
};

struct Layout {
    const Orientation orientation;
    const Point size;
    const Point origin;
    Layout(Orientation orientation_, Point size_, Point origin_)
    : orientation(orientation_), size(size_), origin(origin_) {}
};

// https://www.redblobgames.com/grids/hexagons/implementation.html#hex-to-pixel
Point hex_to_pixel(Layout layout, Hex h) {
    const Orientation& M = layout.orientation;
    double x = (M.f0 * h.q + M.f1 * h.r) * layout.size.x;
    double y = (M.f2 * h.q + M.f3 * h.r) * layout.size.y;
    return Point(x + layout.origin.x, y + layout.origin.y);
}

// https://www.redblobgames.com/grids/hexagons/implementation.html#pixel-to-hex
FracHex pixel_to_hex(Layout layout, Point p) {
    const Orientation& M = layout.orientation;
    Point pt = Point((p.x - layout.origin.x) / layout.size.x,
                     (p.y - layout.origin.y) / layout.size.y);
    double q = M.b0 * pt.x + M.b1 * pt.y;
    double r = M.b2 * pt.x + M.b3 * pt.y;
    return FracHex(q, r, -q - r);
}

// https://www.redblobgames.com/grids/hexagons/implementation.html#rounding
Hex hex_round(FracHex h) {
    int q = int(round(h.q));
    int r = int(round(h.r));
    int s = int(round(h.s));
    double q_diff = abs(q - h.q);
    double r_diff = abs(r - h.r);
    double s_diff = abs(s - h.s);
    if (q_diff > r_diff and q_diff > s_diff) {
        q = -r - s;
    } else if (r_diff > s_diff) {
        r = -q - s;
    } else {
        s = -q - r;
    }
    return Hex(q, r, s);
}

//map storage www.redblobgames.com/grids/hexagons/implementation.html#map-storage
namespace std {
    template <> struct hash<Hex> {
        size_t operator()(const Hex& h) const {
            hash<int> int_hash;
            size_t hq = int_hash(h.q);
            size_t hr = int_hash(h.r);
            return hq ^ (hr + 0x9e3779b9 + (hq << 6) + (hq >> 2));
        }
    };
}

// Function for efficiently moving an npc object a given amount of tiles toward a specific point
void moveNPC(int objX, int objY, int tgtX, int tgtY, int tiles) {

    for (int i = 0; i <= tiles; i++) {
        if ((abs(tgtX - objX) < 75) && (abs(objY - tgtY) < 50)) {
            //Exit the loop early if the object has already reached the destination
            i += tiles;
        }
        else {
            //This detection for direction is currently using the same method for determining the center of the hexagon as the player movement in HandleMousClick
            //(adding 47 to the y value of the object and 20 to the x value of the object)
            //This is because of the manner in which the standard human png is positioned on the hexagon, and may need be updated later to account for other objects
            if ((tgtY > objY + 47) && (abs(tgtX - objX) < 50))
                transformObj(0, 100, objX, objY);
            else if ((tgtY < objY + 47) && (abs(tgtX - objX) < 50))
                transformObj(0, -100, objX, objY);
            else if ((tgtY > objY + 47) && (tgtX > objX + 20))
                transformObj(75, 50, objX, objY);
            else if ((tgtY < objY + 47) && (tgtX > objX + 20))
                transformObj(75, -50, objX, objY);
            else if ((tgtY < objY + 47) && (tgtX < objX + 20))
                transformObj(-75, -50, objX, objY);
            else if ((tgtY > objY + 47) && (tgtX < objX + 20))
                transformObj(-75, 50, objX, objY);
        }
    }

}

void HandleMouseClick() {
    // Move the character to the adjacent tile in the clicked direction.
    if ((cursorY > plaOneY + 47) && (abs(cursorX - plaOneX) < 50))
        transformObj(0, 100, plaOneX, plaOneY);
    else if ((cursorY < plaOneY + 47) && (abs(cursorX - plaOneX) < 50))
        transformObj(0, -100, plaOneX, plaOneY);
    else if ((cursorY > plaOneY + 47) && (cursorX > plaOneX + 20))
        transformObj(75, 50, plaOneX, plaOneY);
    else if ((cursorY < plaOneY + 47) && (cursorX > plaOneX + 20))
        transformObj(75, -50, plaOneX, plaOneY);
    else if ((cursorY < plaOneY + 47) && (cursorX < plaOneX + 20))
        transformObj(-75, -50, plaOneX, plaOneY);
    else if ((cursorY > plaOneY + 47) && (cursorX < plaOneX + 20))
        transformObj(-75, 50, plaOneX, plaOneY);
}

void handleInput() {
    int newMouseX, newMouseY;
    SDL_GetMouseState(&newMouseX, &newMouseY);
    if (cursorX != newMouseX || cursorY != newMouseY) {
        cursorX = newMouseX;
        cursorY = newMouseY;
    }
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            isRunning = false;
        }
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            HandleMouseClick();
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
void RenderTileMap(SDL_Renderer* renderer, const std::vector<SDL_Texture*>& textures, int tileWidth, std::unordered_set<Hex, HexHash> tileMap) {
    SDL_Rect destRect;


    // notice the values 50,57 for the tile size. I found these values through trial and error. We need to find a way to calculate these values based on the tile .png dimensions, or etc.
    Layout flatLayout(layout_flat, Point(50,57), Point(0,0));

    for (Hex tile : tileMap) {
        Point p = hex_to_pixel(flatLayout, tile);
        int x = p.x; //converting these doubles to ints
        int y = p.y;
        destRect = {x, y, 100, 100};
        SDL_RenderCopy(renderer, textures[0], nullptr, &destRect);
        if (tile == hex_round(pixel_to_hex(flatLayout, Point(cursorX-50, cursorY-50)))) {
            SDL_RenderCopy(renderer, textures[2], nullptr, &destRect);
        }
        if (tile.decoration == "birch") {
            SDL_RenderCopy(renderer, textures[3], nullptr, &destRect);
        } else if (tile.decoration == "tree") {
            SDL_RenderCopy(renderer, textures[4], nullptr, &destRect);
        }
    }
}

void render(const std::vector<SDL_Texture*>& textures) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    if (textures[0] != nullptr) {
        RenderTileMap(renderer, textures, 100, mapSet);
    }

    SDL_Rect plaOneDest = { plaOneX, plaOneY, 41, 94 };
    SDL_RenderCopy(renderer, textures[1], nullptr, &plaOneDest);

    SDL_RenderPresent(renderer);
}

std::unordered_set<Hex, HexHash> initMapSet(int winWidth, int winHeight, int tileDem) {
    std::unordered_set<Hex, HexHash> map;
    //numRows = winHeight / tileDem;
    //numCols = (winWidth / tileDem);
    numRows = 22;
    numCols = 22;
    std::cout << "Number of rows: " << numRows << std::endl;
    std::cout << "Number of columns: " << numCols << std::endl;

    //setup the random number gen
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);

    // Initialize the array.
    for (int i = 0; i < numRows; i++) {
        //int oddRowOffset = floor(i/2.0);
        //std::cout << "oddRowOffset: " << oddRowOffset << std::endl;
        for (int j = 0; j < numCols; j++) {
            int s = -i-j;
            Hex tempHex(i, j, s);
            double ranVal = dis(gen);
            if (ranVal > 0.9) tempHex.decoration = "birch";
            else if (ranVal > 0.7) tempHex.decoration = "tree";
            map.insert(tempHex);
        }
    }
    return map;
}

int main(int argc, char* argv[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    // Create a window
    SDL_Window* window = SDL_CreateWindow("Based aspect ratio (4:3)", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600, SDL_WINDOW_SHOWN);
    //SDL_Window* window = SDL_CreateWindow("local man forced to fight robots on a hex grid",
    //    SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600, 0);

    if (!window) {
        SDL_Log("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
    //SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);

    // initialize the renderer variable (already declared globally)
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        SDL_Log("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    //the mapSet var is initialized here so it can be used to draw the map
    mapSet = initMapSet(800, 600, 100);

    //loading textures into the texture vector :)
    std::vector<std::string> imagePaths = {
        "assets/tile-test.png",
        "assets/ancp-male-std-one.png",
        "assets/active-tile-test.png",
        "assets/cvr-birch-test.png",
        "assets/cvr-tree-test.png"
    };
    std::vector<SDL_Texture*> textures;
    for (const std::string& path : imagePaths) {
        SDL_Texture* texture = LoadImageAsTexture(path.c_str());
        textures.push_back(texture);
    }

    while (isRunning) {
        handleInput();
        render(textures);
    }

    // Cleanup and quit
    for (SDL_Texture* texture : textures) {
        SDL_DestroyTexture(texture);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
