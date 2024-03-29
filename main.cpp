#include <iostream>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_main.h>
#include <vector>
#include <array>
#include <unordered_set>
#include <boost/functional/hash.hpp>
#include <random>
#include <SDL_ttf.h>

//setup the random number gen
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<> dis(0.0, 1.0);

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Event event;
bool isRunning = true;
bool mapMode = true;
enum shotStates {
    HIT,
    MISS,
    PRESHOT 
};
shotStates shotState = PRESHOT;
int cursorX;
int cursorY;
std::array<int, 2> activePos;
TTF_Font* fontReg;
TTF_Font* fontBold;
int hitChance;
int plaOneX = 30;
int plaOneY = 3;
int playerX = 320; // Initial X position
int playerY = 240; // Initial Y position
bool fullscreen = false;
int numCols;
int numRows;

//prototype functions here for scoping 
void RenderTileMap(SDL_Renderer* renderer, SDL_Texture* tileset, int tileWidth, int** tilemap);

// Function to modify an object's x and y coordinates
void transformObj(int x, int y, int& objX, int& objY) {
    objX += x;
    objY += y;
}

// Function to properly shift an object's row/col coordinates in the hex grid after a transformation
// Currently configured to function properly in an odd-q layout, will update if my understanding of our hex layout was incorrect
void updateRowsAndCols(int& objRow, int& objCol, bool up, bool right, bool upDir) {
    if (up == true && right == false && upDir == false)
        objCol++;
    else if (up == false && right == false && upDir == false)
        objCol--;
    else if (right == true && upDir == true) {
        if (objCol % 2 != 0) {
            objCol++;
        }
        else {
            objCol++;
            objRow--;
        }
    }
    else if (right == true && upDir == false) {
        if (objCol % 2 != 0) {
            objCol++;
            objRow++;
        }
        else {
            objCol++;
        }
    }
    else if (right == false && upDir == false) {
        if (objCol % 2 != 0) {
            objCol--;
            objRow++;
        }
        else {
            objCol--;
        }
    }
    else if (right == false && upDir == true) {
        if (objCol % 2 != 0) {
            objCol--;
        }
        else {
            objCol--;
            objRow--;
        }
    }
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
    //double x = (M.f0 * h.q + M.f1 * h.r) * layout.size.x;
    double x = layout.size.x * ( 3./2 * h.q);
    double y = layout.size.y * (sqrt(3)/2 * (h.q%2) + sqrt(3) * h.r);

    //double y = (M.f2 * h.q + M.f3 * h.r) * layout.size.y;
    return Point(x + layout.origin.x, y + layout.origin.y);
}

// https://www.redblobgames.com/grids/hexagons/implementation.html#pixel-to-hex
FracHex pixel_to_hex(Layout layout, Point p) {
    const Orientation& M = layout.orientation;
    Point pt = Point((p.x - layout.origin.x) / layout.size.x,
                     (p.y - layout.origin.y) / layout.size.y);
    //double q = M.b0 * pt.x + M.b1 * pt.y;
    //double r = M.b2 * pt.x + M.b3 * pt.y;
    double q = ( 2./3 * pt.x);
    double r = (-1./3 * (static_cast<int>(pt.x)%2) + sqrt(3)/3 * pt.y);

    //double r = M.b2 * pt.x + (static_cast<int>(M.b3)% 2) * pt.y;
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

class Enemy {
public:
    // Constructor to initialize an enemy with a specified health and position
    Enemy(int initialHealth, int inQ, int inR)
        : health(initialHealth), q(inQ), r(inR) {}

    // Function to reduce the enemy's health
    void takeDamage(int damage) {
        health -= damage;
    }

    // Function to move the enemy to a new position
    void move(int newQ, int newR) {
        q = newQ;
        r = newR;
    }

    // Function to check if the enemy is still alive
    bool isAlive() const {
        return health > 0;
    }
private:
    int health;
    int q;
    int r;
};

// Function for efficiently moving an npc object a given amount of tiles toward a specific point
void moveNPC(int objX, int objY, int tgtX, int tgtY, int tiles, int objCol, int objRow) {

    for (int i = 0; i <= tiles; i++) {
        if ((abs(tgtX - objX) < 75) && (abs(objY - tgtY) < 50)) {
            //Exit the loop early if the object has already reached the destination
            i += tiles;
        }
        else {
            //This detection for direction is currently using the same method for determining the center of the hexagon as the player movement in HandleMousClick
            //(adding 47 to the y value of the object and 20 to the x value of the object)
            //This is because of the manner in which the standard human png is positioned on the hexagon, and may need be updated later to account for other objects
            if ((tgtY > objY + 47) && (abs(tgtX - objX) < 50)) {
                transformObj(0, 100, objX, objY);
                updateRowsAndCols(objRow, objCol, true, false, false);
            }
            else if ((tgtY < objY + 47) && (abs(tgtX - objX) < 50)) {
                transformObj(0, -100, objX, objY);
                updateRowsAndCols(objRow, objCol, false, false, false);
            }
            else if ((tgtY > objY + 47) && (tgtX > objX + 20)) {
                transformObj(75, 50, objX, objY);
                updateRowsAndCols(objRow, objCol, true, true, true);
            }
            else if ((tgtY < objY + 47) && (tgtX > objX + 20)) {
                transformObj(75, -50, objX, objY);
                updateRowsAndCols(objRow, objCol, false, true, false);
            }
            else if ((tgtY < objY + 47) && (tgtX < objX + 20)) {
                transformObj(-75, -50, objX, objY);
                updateRowsAndCols(objRow, objCol, false, false, false);
            }
            else if ((tgtY > objY + 47) && (tgtX < objX + 20)) {
                transformObj(-75, 50, objX, objY);
                updateRowsAndCols(objRow, objCol, true, false, true);
            }
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

bool attRes() {
    double luckyDouble = dis(gen);
    if (hitChance > static_cast<int>(luckyDouble * 100)) return false;
    else return true;
}

void handleInput(SDL_Window* window) {
    int newMouseX, newMouseY;
    SDL_GetMouseState(&newMouseX, &newMouseY);
    if (cursorX != newMouseX || cursorY != newMouseY) {
        cursorX = newMouseX;
        cursorY = newMouseY;
    }
    std::cout << "mouse x: " << cursorX << std::endl;
    std::cout << "mouse y: " << cursorY << std::endl;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            isRunning = false;
        } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            //HandleMouseClick();
        } else if (event.key.keysym.sym == SDLK_g) {
            mapMode = !mapMode;
            double luckyDouble = dis(gen);
            hitChance = static_cast<int>(luckyDouble * 100);
            shotState = PRESHOT;
        } else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) {
            bool success = attRes();
            if (success) {
                shotState = HIT;
            } else {
                shotState = MISS;
            }
        } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            if (fullscreen) {
                SDL_SetWindowFullscreen(window, 0); // Switch to windowed mode
                fullscreen = false;
            } else {
                SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
                fullscreen = true;
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
void RenderTileMap(SDL_Renderer* renderer, const std::vector<SDL_Texture*>& textures, int tileWidth, std::unordered_set<Hex, HexHash> tileMap) {
    SDL_Rect destRect;
    SDL_Rect textRect;


    // notice the values 50,57 for the tile size. I found these values through trial and error. We need to find a way to calculate these values based on the tile .png dimensions, or etc.
    Layout flatLayout(layout_flat, Point(50,57), Point(0,0));

    for (Hex tile : tileMap) {
        Point p = hex_to_pixel(flatLayout, tile);
        int x = p.x; //converting these doubles to ints
        int y = p.y;
        destRect = {x, y, 100, 100};
        textRect = {x+10, y+35, 80, 30};

        SDL_RenderCopy(renderer, textures[0], nullptr, &destRect);

        if (tile == hex_round(pixel_to_hex(flatLayout, Point(cursorX-50, cursorY-50)))) {
            SDL_RenderCopy(renderer, textures[2], nullptr, &destRect);
        }

        if (tile.decoration == "birch") {
            SDL_RenderCopy(renderer, textures[3], nullptr, &destRect);
        } else if (tile.decoration == "tree") {
            SDL_RenderCopy(renderer, textures[4], nullptr, &destRect);
        }

        std::string tileCoords= std::to_string(tile.q) + "," + std::to_string(tile.r) + "," + std::to_string(tile.s);
        SDL_Color black;
        black.r = 255;  // Red component (0 to 255)
        black.g = 255;    // Green component (0 to 255)
        black.b = 255;    // Blue component (0 to 255)
        black.a = 255;  // Alpha (transparency) component (0 to 255, 255 is fully opaque)
                        
        SDL_Surface* textSurface = TTF_RenderText_Solid(fontBold, tileCoords.c_str(), black); // textColor is an SDL_Color SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface); // 'renderer' is your SDL_Renderer

        if (textSurface != nullptr) {
            SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface); // 'renderer' is your SDL_Renderer
                                                                                            
            if (textTexture != nullptr) {
                SDL_RenderCopy(renderer, textTexture, NULL, &textRect); // 'destinationRect' is where you want to render the text
                SDL_DestroyTexture(textTexture);
            }

            SDL_FreeSurface(textSurface); // Free the surface, we don't need it anymore
        }
    }
}

SDL_Rect createRect(int row, int col, int textureWidth, int textureHeight) {
    SDL_Rect rect;
    rect.x = col * textureWidth;
    rect.y = row * textureHeight;
    rect.w = textureWidth;
    rect.h = textureHeight;
    return rect;
}

void renderFightUI(const std::vector<SDL_Texture*>& textures) {
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderClear(renderer);

    SDL_Rect attacker = { 20, 280, 380, 300};
    SDL_RenderCopy(renderer, textures[5], nullptr, &attacker);

    SDL_Rect target = { 450, 50, 350, 250 };
    SDL_RenderCopy(renderer, textures[6], nullptr, &target);

    switch (shotState) {
        case PRESHOT: {
            SDL_Rect aa = { 400, 500, 50, 50 };
            SDL_RenderCopy(renderer, textures[7], nullptr, &aa);
            SDL_Rect ab = { 450, 500, 50, 50 };
            SDL_RenderCopy(renderer, textures[8], nullptr, &ab);
            SDL_Rect ac = { 500, 500, 50, 50 };
            SDL_RenderCopy(renderer, textures[9], nullptr, &ac);
            SDL_Rect ad = { 550, 500, 50, 50 };
            SDL_RenderCopy(renderer, textures[10], nullptr, &ad);
            SDL_Rect ae = { 600, 500, 50, 50 };
            SDL_RenderCopy(renderer, textures[11], nullptr, &ae);
            SDL_Rect af = { 650, 500, 50, 50 };
            SDL_RenderCopy(renderer, textures[12], nullptr, &af);

            int red = dis(gen) * 255;
            int blu = dis(gen) * 255;

            std::string hitMsg = "*** Chance to hit: CODE_PLACEHOLDE% ****";
            SDL_Color blue;
            blue.r = red;  // Red component (0 to 255)
            blue.g = 0;    // Green component (0 to 255)
            blue.b = blu;    // Blue component (0 to 255)
            blue.a = 255;  // Alpha (transparency) component (0 to 255, 255 is fully opaque)
            size_t found = hitMsg.find("CODE_PLACEHOLDE");
            if (found != std::string::npos) {
                hitMsg.replace(found, 15, std::to_string(hitChance));
            }

            SDL_Surface* textSurface = TTF_RenderText_Solid(fontBold, hitMsg.c_str(), blue); // textColor is an SDL_Color SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface); // 'renderer' is your SDL_Renderer
            SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface); // 'renderer' is your SDL_Renderer
            SDL_FreeSurface(textSurface); // Free the surface, we don't need it anymore
            SDL_Rect text1 = { 50, 0, 300, 50 };
            SDL_Rect text2 = { 50, 50, 300, 50 };
            SDL_Rect text3 = { 50, 100, 300, 50 };
            SDL_Rect text4 = { 200, 150, 300, 50 };
            SDL_Rect text5 = { 200, 200, 300, 50 };
            SDL_Rect text6 = { 200, 250, 300, 50 };
            SDL_Rect text7 = { 200, 300, 300, 50 };

            SDL_RenderCopy(renderer, textTexture, NULL, &text1); // 'destinationRect' is where you want to render the text
            SDL_RenderCopy(renderer, textTexture, NULL, &text2);
            SDL_RenderCopy(renderer, textTexture, NULL, &text3);
            SDL_RenderCopy(renderer, textTexture, NULL, &text4);
            SDL_RenderCopy(renderer, textTexture, NULL, &text5);
            SDL_RenderCopy(renderer, textTexture, NULL, &text6);
            SDL_RenderCopy(renderer, textTexture, NULL, &text7);
            SDL_DestroyTexture(textTexture);
            break;
        } case HIT: {
            int blu = 255;

            std::string hitMsg = "*** SUCCESS :) ****";
            SDL_Color blue;
            blue.r = 0;  // Red component (0 to 255)
            blue.g = 0;    // Green component (0 to 255)
            blue.b = blu;    // Blue component (0 to 255)
            blue.a = 255;  // Alpha (transparency) component (0 to 255, 255 is fully opaque)

            SDL_Surface* textSurface = TTF_RenderText_Solid(fontBold, hitMsg.c_str(), blue); // textColor is an SDL_Color SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
            SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface); // 'renderer' is your SDL_Renderer
            SDL_FreeSurface(textSurface); // Free the surface, we don't need it anymore
            int textureWidth = 200;
            int textureHeight = 50;
            // Loop to render the grid of textures
            for (int row = 0; row < numRows; ++row) {
                for (int col = 0; col < numCols; ++col) {
                    // Create SDL_Rect for the current position in the grid
                    SDL_Rect currentRect = createRect(row, col, textureWidth, textureHeight);

                    // Render the texture at the current position
                    SDL_RenderCopy(renderer, textTexture, NULL, &currentRect);
                }
            }

            SDL_DestroyTexture(textTexture);
            break;
        } case MISS: {
            int redd = 255;

            std::string hitMsg = "*** MISS :( ****";
            SDL_Color red;
            red.r = redd;  // Red component (0 to 255)
            red.g = 0;    // Green component (0 to 255)
            red.b = 0;    // Blue component (0 to 255)
            red.a = 255;  // Alpha (transparency) component (0 to 255, 255 is fully opaque)

            SDL_Surface* textSurface = TTF_RenderText_Solid(fontBold, hitMsg.c_str(), red); // textColor is an SDL_Color SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
            SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface); // 'renderer' is your SDL_Renderer
            SDL_FreeSurface(textSurface); // Free the surface, we don't need it anymore
            int textureWidth = 200;
            int textureHeight = 50;
            // Loop to render the grid of textures
            for (int row = 0; row < numRows; ++row) {
                for (int col = 0; col < numCols; ++col) {
                    // Create SDL_Rect for the current position in the grid
                    SDL_Rect currentRect = createRect(row, col, textureWidth, textureHeight);

                    // Render the texture at the current position
                    SDL_RenderCopy(renderer, textTexture, NULL, &currentRect);
                }
            }

            SDL_DestroyTexture(textTexture);
            break;
        }
    }
}

void render(const std::vector<SDL_Texture*>& textures) {
    if (mapMode && textures[0] != nullptr) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        RenderTileMap(renderer, textures, 100, mapSet);
        //SDL_Rect plaOneDest = { plaOneX, plaOneY, 41, 94 };
        //SDL_RenderCopy(renderer, textures[1], nullptr, &plaOneDest);
    } else {
        renderFightUI(textures);
    }

    SDL_RenderPresent(renderer);
}

std::unordered_set<Hex, HexHash> initMapSet(int winWidth, int winHeight, int tileDem) {
    std::unordered_set<Hex, HexHash> map;
    //numRows = winHeight / tileDem;
    //numCols = (winWidth / tileDem);
    numCols = 10;
    numRows = 5;
    std::cout << "Number of rows: " << numRows << std::endl;
    std::cout << "Number of columns: " << numCols << std::endl;

    // Initialize the array.
    for (int i = 0; i < numRows; i++) {
        //int oddRowOffset = floor(i/2.0);
        //std::cout << "oddRowOffset: " << oddRowOffset << std::endl;
        for (int j = 0; j < numCols; j++) {
            int s = -i-j;
            Hex tempHex(i, j, s);
            double ranVal = dis(gen);
            //if (ranVal > 0.9) tempHex.decoration = "birch";
            //else if (ranVal > 0.7) tempHex.decoration = "tree";
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

    if (!window) {
        SDL_Log("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    if (TTF_Init() == -1) {
        throw std::runtime_error("TrueType font support (TTF) failed to initialize");
    }
    fontReg = TTF_OpenFont("./ttf/Hack-Regular.tff", 24);
    fontBold = TTF_OpenFont("./ttf/Hack-Bold.ttf", 24);

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
        "assets/tile-test-blue.png",
        "assets/ancp-male-std-one.png",
        "assets/active-tile-test.png",
        "assets/cvr-birch-test.png",
        "assets/cvr-tree-test.png",
        "assets/ancp-fight-test.png",
        "assets/dog-bomb-test.png",
        "assets/pri-butt.png",
        "assets/side-butt.png",
        "assets/sword-butt.png",
        "assets/lsd-butt.png",
        "assets/hack-butt.png",
        "assets/hack2-butt.png"
    };
    std::vector<SDL_Texture*> textures;
    for (const std::string& path : imagePaths) {
        SDL_Texture* texture = LoadImageAsTexture(path.c_str());
        textures.push_back(texture);
    }

    while (isRunning) {
        handleInput(window);
        render(textures);
    }

    // Cleanup and quit
    for (SDL_Texture* texture : textures) {
        SDL_DestroyTexture(texture);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(fontReg);
    TTF_CloseFont(fontBold);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
