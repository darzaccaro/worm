// TODO: fix 1px sprite border disconnections
// TODO: tail is sometimes mis-rotated when growing
// TODO: audio
// TODO: make sure spawned apples don't already collide with snake or other apples

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

typedef const char* cstring;

#define nil 0
#define global static
#define persist static

#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>

#define TILE_SIZE     64
#define TILES_WIDE    16
#define TILES_TALL    9
#define WINDOW_WIDTH  TILES_WIDE * TILE_SIZE
#define WINDOW_HEIGHT TILES_TALL * TILE_SIZE
#define FRAMES_PER_SECOND 60
#define MS_PER_FRAME ((1.f/60.f) * 1000.f)
#define MS_PER_UPDATE MS_PER_FRAME * 5.f
#define MAX_APPLES 16
#define MAX_INPUT_QUEUE_SIZE 3
#define MAX_SCORE_TEXT 128
#define MS_PER_APPLE_SPAWN 4 * 1000
#define GROWTH_FACTOR 2
#define DEBUG_MODE true
#define MAX_SNAKE_LENGTH TILES_WIDE * TILES_TALL

typedef struct {
    f32 x;
    f32 y;
} V2f;

V2f V2fAddV2f(V2f a, V2f b) {
    return (V2f) { a.x + b.x, a.y + b.y };
}

V2f V2fMul(V2f a, f32 b) {
    return (V2f) { a.x * b, a.y * b };
}

bool V2fEqV2f(V2f a, V2f b) {
    return a.x == b.x && a.y == b.y;
}

V2f V2fSubV2f(V2f a, V2f b) {
    return (V2f) { a.x - b.x, a.y - b.y };
}

f32 V2fMagnitude(V2f v) {
    return sqrtf(v.x * v.x + v.y * v.y);
}

V2f V2fNormalize(V2f v) {
    f32 magnitude = V2fMagnitude(v);
    return (V2f) {v.x / magnitude, v.y / magnitude};
}

typedef struct {
    V2f position;
    bool isActive;
} Apple;

typedef struct {
    V2f positions[MAX_SNAKE_LENGTH];
    V2f directions[MAX_SNAKE_LENGTH];
    u64 length;
} Snake;

typedef struct {
    SDL_KeyCode inputs[MAX_INPUT_QUEUE_SIZE];
} InputQueue;

typedef enum {
    SN_TRANSPARENT,
    SN_HEAD,
    SN_BODY_STRAIGHT,
    SN_BODY_CURVED,
    SN_TAIL,
    SN_APPLE,
} SpriteName;

typedef struct {
    SDL_Texture* texture;
    i32 width, height;
    i32 tileSize;
} SpriteSheet;

typedef enum {
    GM_START,
    GM_PLAY,
    GM_GAME_OVER,
} GameMode;

global SDL_Renderer* renderer;
global SpriteSheet spriteSheet;
global TTF_Font* font;
global Apple apples[MAX_APPLES];
global Snake snake;
global SDL_KeyCode inputs[MAX_INPUT_QUEUE_SIZE];
global char* scoreText[MAX_SCORE_TEXT];
global GameMode gameMode;
global u32 startTime;
global u32 updateTime;
global u32 appleTime;
global u64 score;
global u64 prevScore;
global bool ateLastFrame;
global bool wasKeyPressed;
global bool isRunning;

SDL_KeyCode PushInput(SDL_KeyCode key) {
    for (u64 i = 0; i < MAX_INPUT_QUEUE_SIZE; i++) {
        if (inputs[i] == 0) {
            inputs[i] = key;
            break;
        }
    }
}

SDL_KeyCode PopInput() {
    SDL_KeyCode key = inputs[0];
    for (u64 i = 0; i < MAX_INPUT_QUEUE_SIZE; i++) {
        if (i == MAX_INPUT_QUEUE_SIZE - 1) {
            inputs[i] = 0;
        }
        else {
            inputs[i] = inputs[i + 1];
        }
    }
    return key;
}

void DrawSprite(SpriteName sprite, u64 x, u64 y, f64 angle) {
    i32 tilesWide = spriteSheet.width / spriteSheet.tileSize;
    i32 tilesHigh = spriteSheet.height / spriteSheet.tileSize;
    i32 sy = (sprite / tilesWide);
    i32 sx = sprite % tilesWide;

    SDL_Rect source = (SDL_Rect){
        .x = sx * spriteSheet.tileSize,
        .y = sy * spriteSheet.tileSize,
        .w = spriteSheet.tileSize,
        .h = spriteSheet.tileSize,
    };
    SDL_Rect destination = (SDL_Rect){
        .x = x * TILE_SIZE,
        .y = y * TILE_SIZE,
        .w = TILE_SIZE,
        .h = TILE_SIZE,
    };
    SDL_RenderCopyEx(renderer, spriteSheet.texture, &source, &destination, angle, nil, nil);
}


void UpdateScoreText() {
    sprintf_s(scoreText, MAX_SCORE_TEXT, "Score: %llu", score);
}

Snake CreateSnake(V2f position, V2f direction, u64 length) {
    Snake snake = { 0 };
    assert(length <= MAX_SNAKE_LENGTH);
    snake.length = length;
    snake.directions[0] = direction;
    snake.positions[0] = position;
    for (i32 i = 1; i < length; i++) {
        V2f offset = V2fMul(direction, -i);
        snake.positions[i] = V2fAddV2f(position, offset);
        snake.directions[i] = direction;
    }
    return snake;
}

Snake GrowSnake() {
    Snake next = { 0 };
    next.length = snake.length + GROWTH_FACTOR;
    assert(next.length <= MAX_SNAKE_LENGTH);
    
    for (u64 i = 0; i < next.length; i++) {
        if (i < snake.length) {
            next.positions[i] = snake.positions[i];
            next.directions[i] = snake.directions[i];
        }
        else {
            next.positions[i] = next.positions[snake.length - 1];
            next.directions[i] = next.directions[snake.length - 1];
        }
    }
    return next;
}


void UpdateSnake(V2f direction) {
    // body
    for (u64 i = snake.length - 1; i > 0; i--) {
        snake.positions[i] = snake.positions[i - 1];
        snake.directions[i] = snake.directions[i - 1];
    }
    // head
    snake.directions[0] = direction;
    snake.positions[0] = V2fAddV2f(snake.positions[0], snake.directions[0]);
}

void DrawSnake() {
    for (u64 i = 0; i < snake.length; i++) {
        SpriteName sprite = 0;
        f64 angle = 0;
        V2f direction = { 0 };
        if (i == 0) {
            direction = snake.directions[i];
            sprite = SN_HEAD;

            if (direction.x == 1) {
                angle = 90;
            }
            if (direction.x == -1) {
                angle = -90;
            }
            if (direction.y == -1) {
                angle = 0;
            }
            if (direction.y == 1) {
                angle = 180;
            }

        }
        else if (i == snake.length - 1) {
            direction = snake.directions[i-1];
            sprite = SN_TAIL;
            if (direction.x == 1) {
                angle = 90;
            }
            if (direction.x == -1) {
                angle = -90;
            }
            if (direction.y == -1) {
                angle = 0;
            }
            if (direction.y == 1) {
                angle = 180;
            }
            
        }
        else {
            V2f pa = snake.positions[i - 1];
            V2f pb = snake.positions[i];
            V2f pc = snake.positions[i + 1];
            V2f diffA = V2fNormalize(V2fSubV2f(pb, pa));
            V2f diffC = V2fNormalize(V2fSubV2f(pb, pc));
            if (V2fEqV2f(diffA, (V2f) { 1, 0 }) && V2fEqV2f(diffC, (V2f) { -1, 0 })
                || V2fEqV2f(diffA, (V2f) { -1, 0 }) && V2fEqV2f(diffC, (V2f) { 1, 0 })
                || V2fEqV2f(diffA, (V2f) { -1, 0 }) && V2fEqV2f(diffC, (V2f) { -1, 0 })
                || V2fEqV2f(diffA, (V2f) { 0, -1 }) && V2fEqV2f(diffC, (V2f) { 0, 1 })
                || V2fEqV2f(diffA, (V2f) { 0, 1 }) && V2fEqV2f(diffC, (V2f) { 0, -1 })
                ) {
                if (diffA.x) {
                    angle = 90;
                }
                sprite = SN_BODY_STRAIGHT;
            } else if (
              V2fEqV2f(diffA, (V2f) { 1, 0 }) && V2fEqV2f(diffC, (V2f) { 0, 1 })
                || V2fEqV2f(diffA, (V2f) { 0, 1 }) && V2fEqV2f(diffC, (V2f) { 1, 0 })
                ) {
                angle = 180;
                sprite = SN_BODY_CURVED;
            }
            else if (V2fEqV2f(diffA, (V2f) { 0, -1 }) && V2fEqV2f(diffC, (V2f) { 1, 0 })
                || V2fEqV2f(diffA, (V2f) { 1, 0 }) && V2fEqV2f(diffC, (V2f) { 0, -1 })
                ) {
                angle = 90;
                sprite = SN_BODY_CURVED;
            }
            else if (V2fEqV2f(diffA, (V2f) { 0, 1 }) && V2fEqV2f(diffC, (V2f) { -1, 0 })
                || V2fEqV2f(diffA, (V2f) { -1, 0 }) && V2fEqV2f(diffC, (V2f) { 0, 1 })
                ) {
                angle = -90;
                    sprite = SN_BODY_CURVED;
            }
            else if (V2fEqV2f(diffA, (V2f) { -1, 0 }) && V2fEqV2f(diffC, (V2f) { 0, -1 })
                || V2fEqV2f(diffA, (V2f) { 0, -1 }) && V2fEqV2f(diffC, (V2f) { -1, 0 })) {
                angle = 0;
                    sprite = SN_BODY_CURVED;
            }
            else {
                sprite = SN_TRANSPARENT;
            }
            
        }
        DrawSprite(sprite, snake.positions[i].x, snake.positions[i].y, angle);
    }
}

void SpawnApple() {
    for (u64 i = 0; i < MAX_APPLES; i++) {
        if (apples[i].isActive) continue;
        V2f pos = (V2f){
            .x = rand() % TILES_WIDE,
            .y = rand() % TILES_TALL,
        };
        // TODO make sure snake is not already here
        // TODO make sure apple is not already here
        apples[i].position = pos;
        apples[i].isActive = true;
        break;
    }
}

void DrawApples() {
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    for (u64 i = 0; i < MAX_APPLES; i++) {
        if (!apples[i].isActive) continue;
        DrawSprite(SN_APPLE, apples[i].position.x, apples[i].position.y, 0);
    }
}

void DrawText(const char* text, f32 x, f32 y, f32 w, f32 h) {
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, (SDL_Color) { 255, 255, 255, 255 });
    assert(surface);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    assert(texture);
    SDL_FreeSurface(surface);
    SDL_Rect textRect = (SDL_Rect){
        .x = x,
        .y = y,
        .w = w,
        .h = h,
    };
    if (SDL_RenderCopy(renderer, texture, nil, &textRect) != 0) {
        assert(false);
    }
}

void GameModeStart() {
    {
        const char* text = "Snake";
        i32 textWidth, textHeight;
        TTF_SizeText(font, text, &textWidth, &textHeight);
        DrawText(text, WINDOW_WIDTH / 2 - textWidth / 2, WINDOW_HEIGHT / 4 - textHeight - 2, textWidth, textHeight);
    }
    {
        const char* text = "Press any key to play";
        i32 textWidth, textHeight;
        TTF_SizeText(font, text, &textWidth, &textHeight);
        DrawText(text, WINDOW_WIDTH / 2 - textWidth / 2, WINDOW_HEIGHT / 2 - textHeight - 2, textWidth, textHeight);
    }

}

void GameModePlay() {
    if (SDL_GetTicks() - updateTime >= MS_PER_UPDATE) {
        SDL_KeyCode key = PopInput();
        V2f direction = snake.directions[0];
        if (key == SDLK_LEFT) {
            direction = (V2f){ -1, 0 };
        }
        if (key == SDLK_RIGHT) {
            direction = (V2f){ 1, 0 };
        }
        if (key == SDLK_UP) {
            direction = (V2f){ 0, -1 };
        }
        if (key == SDLK_DOWN) {
            direction = (V2f){ 0, 1 };
        }

        if (ateLastFrame) {
            snake = GrowSnake();
            ateLastFrame = false;
        }
        UpdateSnake(direction);
        // handle boundary collisions
        if (snake.positions[0].x < 0 || snake.positions[0].x >= TILES_WIDE || snake.positions[0].y < 0 || snake.positions[0].y >= TILES_TALL) {
            gameMode = GM_GAME_OVER;
            return;
        }
        // handle self collisions
        for (u64 i = 1; i < snake.length; i++) {
            V2f head = snake.positions[0];
            V2f body = snake.positions[i];
            if (V2fEqV2f(head, body)) {
                gameMode = GM_GAME_OVER;
                return;
            }
        }


        for (u64 i = 0; i < MAX_APPLES; i++) {
            if (!apples[i].isActive) continue;
            if (V2fEqV2f(apples[i].position, snake.positions[0])) {
                score += 100;
                apples[i].isActive = false;
                ateLastFrame = true;
            }
        }

        if (SDL_GetTicks() - appleTime >= MS_PER_APPLE_SPAWN) {
            SpawnApple();
            appleTime = SDL_GetTicks();
        }

        if (prevScore != score) {
            UpdateScoreText();
            prevScore = score;
        }

        updateTime = SDL_GetTicks();
    }

    DrawApples();
    DrawSnake(snake);
    {
        const char* text = scoreText;
        i32 textWidth, textHeight;
        TTF_SizeText(font, text, &textWidth, &textHeight);
        DrawText(text, 10, 10, textWidth, textHeight);
    }
}

void GameModeGameOver() {
    {
        const char* text = "Game Over";
        i32 textWidth, textHeight;
        TTF_SizeText(font, text, &textWidth, &textHeight);
        DrawText(text, WINDOW_WIDTH / 2 - textWidth / 2, WINDOW_HEIGHT / 4 - textHeight - 2, textWidth, textHeight);
    }
    {
        const char* text = scoreText;
        i32 textWidth, textHeight;
        TTF_SizeText(font, text, &textWidth, &textHeight);
        DrawText(text, WINDOW_WIDTH / 2 - textWidth / 2, WINDOW_HEIGHT / 2 - textHeight - 2, textWidth, textHeight);
    }
}

int main(int argc, char* args[]) {
PLATFORM_INIT:
    srand((u32)time(nil));

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        assert(false);
    }
    
    if (TTF_Init() != 0) {
        assert(false);
    }

    if (IMG_Init(IMG_INIT_PNG) == 0) {
        assert(false);
    }

    SDL_Window* window = SDL_CreateWindow("Snake", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    assert(window);
    
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    assert(renderer);
    
    font = TTF_OpenFont("assets/fonts/Roboto-Regular.ttf", 48);
    assert(font);

    {
        SDL_Surface* surface = IMG_Load("assets/images/sprites.png");
        assert(surface);
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        assert(texture);

        spriteSheet = (SpriteSheet){
            .texture = texture,
            .width = surface->w,
            .height = surface->h,
            .tileSize = TILE_SIZE,
        };

        SDL_FreeSurface(surface);
    }

    i32 windowWidth, windowHeight, rendererWidth, rendererHeight;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    SDL_GetRendererOutputSize(renderer, &rendererWidth, &rendererHeight);
    f32 scaleX = (f32)rendererWidth / (f32)windowWidth;
    f32 scaleY = (f32)rendererHeight / (f32)windowHeight;
    assert(scaleX == scaleY);   

    SDL_RenderSetScale(renderer, scaleX, scaleY);

GAME_INIT:
    UpdateScoreText();

    snake = CreateSnake((V2f) { 4, 2 }, (V2f) { 1, 0 }, 4);

    isRunning = true;
    startTime = SDL_GetTicks();
    updateTime = SDL_GetTicks();
    appleTime = SDL_GetTicks();
    prevScore = score;
    ateLastFrame = false;
    wasKeyPressed = false;
    SDL_KeyCode lastKey = nil;

EVENT_LOOP:
    while (isRunning) {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                isRunning = false;
                break;
            }
            if (event.type == SDL_KEYDOWN) {
                if (gameMode == GM_START || gameMode == GM_GAME_OVER) {
                    score = 0;
                    gameMode = GM_PLAY;
                    goto GAME_INIT;
                }
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE: {
                    isRunning = false;
                    break;
                }
                case SDLK_LEFT: {
                    if (!event.key.repeat && lastKey != SDLK_RIGHT) {
                        lastKey = SDLK_LEFT;
                        PushInput(event.key.keysym.sym);
                        wasKeyPressed = true;
                    }
                    break;
                }
                case SDLK_RIGHT: {
                    if (!event.key.repeat && lastKey != SDLK_LEFT) {
                        lastKey = SDLK_RIGHT;
                        PushInput(event.key.keysym.sym);
                        wasKeyPressed = true;
                    }
                    break;
                }
                case SDLK_UP: {
                    if (!event.key.repeat && lastKey != SDLK_DOWN) {
                        lastKey = SDLK_UP;
                        PushInput(event.key.keysym.sym);
                        wasKeyPressed = true;
                    }
                    break;
                }
                case SDLK_DOWN: {
                    if (!event.key.repeat && lastKey != SDLK_UP) {
                        lastKey = SDLK_DOWN;
                        PushInput(event.key.keysym.sym);
                        wasKeyPressed = true;
                    }
                    break;
                }
                }
            }
        }

    UPDATE_AND_RENDER:
#ifdef DEBUG_MODE
        gameMode = GM_PLAY;
        if (!wasKeyPressed) {
            goto EVENT_LOOP;
        }
#else
        if (SDL_GetTicks() - startTime < MS_PER_FRAME) {
            goto EVENT_LOOP;
        }
#endif
        // clear to black
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        // draw grid
        SDL_SetRenderDrawColor(renderer, 25, 25, 25, 255);
        for (i32 y = 0; y < TILES_TALL; y++) {
            SDL_RenderDrawLine(renderer, 0, y * TILE_SIZE, TILES_WIDE * TILE_SIZE, y * TILE_SIZE);

            for (i32 x = 0; x < TILES_WIDE; x++) {
                SDL_RenderDrawLine(renderer, x * TILE_SIZE, 0, x * TILE_SIZE, TILES_TALL * TILE_SIZE);
            }
        }

        switch (gameMode) {
        case GM_START: {
            GameModeStart();
            break;
        }
        case GM_PLAY: {
            GameModePlay();
            break;
        }
        case GM_GAME_OVER: {
            GameModeGameOver();
            break;
        }
        default:
            assert(false);
        }

        SDL_RenderPresent(renderer);
        wasKeyPressed = false;
        startTime = SDL_GetTicks();
    }

    return 0;
}
