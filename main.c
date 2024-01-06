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

#define ARRAY_COUNT(array) (sizeof(array) / sizeof(array[0]))

#if 0


typedef struct FixedArray {
    void* elements;
    u64 capacity;
    u64 count;
    u64 elementSize;
} FixedArray;


FixedArray _FixedArrayCreate(u64 elementSize, u64 capacity) {
    FixedArray r = {0};
    r.elements = calloc(capacity, elementSize);
    assert(r.elements);
    r.capacity = capacity;
    r.elementSize = elementSize;
    return r;
}

#define FixedArrayCreate(type, capacity) _FixedArrayCreate(sizeof(type), (capacity))

void _FixedArrayAppend(FixedArray* array, void* element, u64 elementSize) {
    assert(elementSize == array->elementSize);
    assert(array->capacity > array->count + 1);
    u8* p = array->elements;
    p += elementSize * (array->count);
    *p = element;
    array->count++;
}
#define FixedArrayAppend(array, element) _FixedArrayAppend((array), (typeof(element))(element), sizeof(element))

void* FixedArrayAt(FixedArray* array, u64 index) {
    assert(index < array->count);
    u8* p = array->elements;
    p += array->elementSize * index;
    return (void*) p;
}

#define FixedArray(type, capacity) FixedArray_##type

// TODO declare generics like this, then use _Generic to dispatch...
// your handmade talk can be on this!
#define DeclareFixedArray(type) \
    typedef struct { \
        u64 count; \
        u64 capacity; \
        type* elements; \
    } FixedArray_##type; \
    FixedArray_##type FixedArrayCreate_##type(u64 capacity) { \
        FixedArray_##type r = {0}; \
        r.elements = calloc(capacity, sizeof(type)); \
        assert(r.elements); \
        r.capacity = capacity; \
        return r; \
    } \
    void FixedArrayAppend_##type(FixedArray_##type* array, type element) { \
        assert(array->capacity > array->count + 1); \
        array->elements[array->count++] = element; \
    } \

#define FixedArrayCreate(type, capacity) FixedArrayCreate_##type(capacity)
#define FixedArrayAppend(type, array, element) FixedArrayAppend_##type(array, element)
#define FixedArray(type) FixedArray_##type

DeclareFixedArray(u8);


// TODO make sure these structs are tightly packed
typedef struct {
    u64 capacity;
    u64 count;
    u64 stride;
    void* data;
} DynamicArray;

DynamicArray DynamicArrayCreate(u64 capacity, u64 stride) {
    DynamicArray array = { 0 };//malloc(sizeof(header) + capacity * stride);
    array.capacity = capacity;
    array.stride = stride;
    array.count = 0;
    array.data = malloc(capacity * stride);
    assert(array.data);
    return array;
}

void DynamicArrayAppend(DynamicArray* array, void* element) {
    assert(array->capacity + 1 > array->count); // TODO realloc
    u8* p = array->data;
    p += array->count * array->stride;
    *p = element;
    array->count++;
}

void* DynamicArrayAt(DynamicArray array, u64 index) {
    assert(index < array.count);
    u8* p = array.data;
    p += array.count * array.stride;
    return *p;
}

#endif

#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>

#define TILE_SIZE     64
#define TILES_WIDE    16
#define TILES_TALL    12
#define WINDOW_WIDTH  TILES_WIDE * TILE_SIZE
#define WINDOW_HEIGHT TILES_TALL * TILE_SIZE
#define FRAMES_PER_SECOND 60
#define MS_PER_FRAME ((1.f/60.f) * 1000.f)
#define MS_PER_UPDATE MS_PER_FRAME * 10.f
#define MAX_APPLES 16
#define MAX_INPUT_QUEUE_SIZE 3
#define MAX_SCORE_TEXT 128
#define MS_PER_APPLE_SPAWN 4 * 1000

typedef struct {
    f32 x;
    f32 y;
} V2f;

V2f V2fAddV2f(V2f a, V2f b) {
    return (V2f) {
        .x = a.x + b.x,
        .y = a.y + b.y,
    };
}

V2f V2fMul(V2f a, f32 b) {
    return (V2f) {
        .x = a.x * b,
        .y = a.y * b,
    };
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
    V2f* positions;
    V2f* directions;
    u64 length;
} Snake;


typedef struct {
    SDL_KeyCode inputs[MAX_INPUT_QUEUE_SIZE];
} InputQueue;

SDL_KeyCode InputQueuePush(InputQueue* iq, SDL_KeyCode key) {
    for (u64 i = 0; i < MAX_INPUT_QUEUE_SIZE; i++) {
        if (iq->inputs[i] == 0) {
            iq->inputs[i] = key;
            break;
        }
    }
}

SDL_KeyCode InputQueuePop(InputQueue* iq) {
    SDL_KeyCode key = iq->inputs[0];
    for (u64 i = 0; i < MAX_INPUT_QUEUE_SIZE; i++) {
        if (i == MAX_INPUT_QUEUE_SIZE - 1) {
            iq->inputs[i] = 0;
        } else {
            iq->inputs[i] = iq->inputs[i + 1];
        }
    }
    return key;
}

typedef enum {
    SN_DARK_STRAIGHT,
    SN_DARK_TOP_LEFT,
    SN_DARK_TOP_RIGHT,
    SN_HEAD,
    SN_APPLE,
    SN_DARK_BOTTOM_LEFT,
    SN_DARK_BOTTOM_RIGHT,
    SN_TAIL,
    SN_LIGHT_STRAIGHT,
    SN_LIGHT_TOP_LEFT,
    SN_LIGHT_TOP_RIGHT,
    SN_LIGHT_BOTTOM_LEFT,
    SN_LIGHT_BOTTOM_RIGHT,
} SpriteName;

typedef struct {
    SDL_Texture* texture;
    i32 width, height;
    i32 tileSize;
} SpriteSheet;


static SDL_Renderer* renderer;
static SpriteSheet spriteSheet;
static TTF_Font* font;
static Apple apples[MAX_APPLES];
static Snake snake;
static InputQueue inputQueue;
static u64 score;
static char* scoreText[MAX_SCORE_TEXT];
static bool onlyUpdateOnKeyPress = false;

void DrawSprite(SpriteSheet sheet, SpriteName sprite, u64 x, u64 y, f64 angle) {
    i32 tilesWide = sheet.width / sheet.tileSize;
    i32 tilesHigh = sheet.height / sheet.tileSize;
    i32 sy = (sprite / tilesWide);
    i32 sx = sprite % tilesWide;

    SDL_Rect source = (SDL_Rect){
        .x = sx * sheet.tileSize,
        .y = sy * sheet.tileSize,
        .w = sheet.tileSize,
        .h = sheet.tileSize,
    };
    SDL_Rect destination = (SDL_Rect){
        .x = x * TILE_SIZE,
        .y = y * TILE_SIZE,
        .w = TILE_SIZE,
        .h = TILE_SIZE,
    };
    SDL_RenderCopyEx(renderer, spriteSheet.texture, &source, &destination, angle, nil, nil);
}


void ScoreTextUpdate(char* scoreText, u64 score) {
    sprintf_s(scoreText, MAX_SCORE_TEXT, "Score: %llu", score);
}

Snake SnakeCreate(V2f position, V2f direction, u64 length) {
    Snake snake = { 0 };
    snake.positions = malloc(sizeof(V2f) * length);
    assert(snake.positions);
    snake.directions = malloc(sizeof(V2f) * length);
    assert(snake.directions);
    snake.length = length;
    snake.directions[0] = direction;
    snake.positions[0] = position;
    for (u64 i = 1; i < length; i++) {
        snake.positions[i] = position;
        snake.directions[i] = direction;
    }
    return snake;
}

void SnakeUpdate(Snake snake, V2f direction) {
    // body
    for (u64 i = snake.length - 1; i > 0; i--) {
        snake.positions[i] = snake.positions[i - 1];
        snake.directions[i] = snake.directions[i - 1];
    }
    // head
    snake.directions[0] = direction;
    snake.positions[0] = V2fAddV2f(snake.positions[0], snake.directions[0]);
}

void SnakeDraw(Snake snake) {
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);

    for (u64 i = 0; i < snake.length; i++) {
        SpriteName sprite = 0;
        f64 angle = 0;
        V2f direction = { 0 };
        if (i == 0) {
            direction = snake.directions[i];
            sprite = SN_HEAD;
        }
        else if (i == snake.length - 1) {
            direction = snake.directions[i - 1];
            sprite = SN_TAIL;
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
                if (i % 2 == 0) {
                    sprite = SN_DARK_STRAIGHT;
                }
                else {
                    sprite = SN_LIGHT_STRAIGHT;
                }
            } else if (
              V2fEqV2f(diffA, (V2f) { 1, 0 }) && V2fEqV2f(diffC, (V2f) { 0, 1 })
                || V2fEqV2f(diffA, (V2f) { 0, 1 }) && V2fEqV2f(diffC, (V2f) { 1, 0 })
                ) {
                if (i % 2 == 0) {
                    sprite = SN_DARK_BOTTOM_RIGHT;
                }
                else {
                    sprite = SN_LIGHT_BOTTOM_RIGHT;
                }
            }
            else if (V2fEqV2f(diffA, (V2f) { 0, -1 }) && V2fEqV2f(diffC, (V2f) { 1, 0 })
                || V2fEqV2f(diffA, (V2f) { 1, 0 }) && V2fEqV2f(diffC, (V2f) { 0, -1 })
           
                ) {
                if (i % 2 == 0) {
                    sprite = SN_DARK_TOP_RIGHT;
                }
                else {
                    sprite = SN_LIGHT_TOP_RIGHT;
                }
            }
            else if (V2fEqV2f(diffA, (V2f) { 0, 1 }) && V2fEqV2f(diffC, (V2f) { -1, 0 })
                || V2fEqV2f(diffA, (V2f) { -1, 0 }) && V2fEqV2f(diffC, (V2f) { 0, 1 })
                ) {
                if (i % 2 == 0) {
                    sprite = SN_DARK_BOTTOM_LEFT;
                }
                else {
                    sprite = SN_LIGHT_BOTTOM_LEFT;
                }
            }
            else if (V2fEqV2f(diffA, (V2f) { -1, 0 }) && V2fEqV2f(diffC, (V2f) { 0, -1 })
                || V2fEqV2f(diffA, (V2f) { 0, -1 }) && V2fEqV2f(diffC, (V2f) { -1, 0 })) {
                if (i % 2 == 0) {
                    sprite = SN_DARK_TOP_LEFT;
                }
                else {
                    sprite = SN_LIGHT_TOP_LEFT;
                }
            }
            else {
                sprite = SN_APPLE;
            }
            
        }
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
        DrawSprite(spriteSheet, sprite, snake.positions[i].x, snake.positions[i].y, angle);
    }
}

void SpawnApple() {
    for (u64 i = 0; i < ARRAY_COUNT(apples); i++) {
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
    for (u64 i = 0; i < ARRAY_COUNT(apples); i++) {
        if (!apples[i].isActive) continue;
        DrawSprite(spriteSheet, SN_APPLE, apples[i].position.x, apples[i].position.y, 0);
    }
}


int main(int argc, char* args[]) {
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

    SDL_Window* window = SDL_CreateWindow("Snake Rogue", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    assert(window);
    
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    assert(renderer);
    
    font = TTF_OpenFont("assets/fonts/Roboto-Regular.ttf", 64);
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

    ScoreTextUpdate(scoreText, score);

    snake = SnakeCreate((V2f) { 4, 2 }, (V2f) { 1, 0 }, 4);

    bool isRunning = true;
    u32 startTime = SDL_GetTicks();
    u32 updateTime = SDL_GetTicks();
    u32 appleTime = SDL_GetTicks();
    u64 prevScore = score;
    bool ateLastFrame = false;
    bool wasKeyPressed = false;
    while (isRunning) {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                isRunning = false;
                break;
            }
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE: {
                    isRunning = false;
                    break;
                }
                case SDLK_LEFT: case SDLK_RIGHT: case SDLK_UP: case SDLK_DOWN: {
                    if (!event.key.repeat) {
                        InputQueuePush(&inputQueue, event.key.keysym.sym);
                        wasKeyPressed = true;
                    }
                    break;
                }
                }
            }
        }
        // clear to white
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);
        // draw grid
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        for (i32 y = 0; y < TILES_TALL; y++) {
            SDL_RenderDrawLine(renderer, 0, y * TILE_SIZE, TILES_WIDE * TILE_SIZE, y * TILE_SIZE);

            for (i32 x = 0; x < TILES_WIDE; x++) {
                SDL_RenderDrawLine(renderer, x * TILE_SIZE, 0, x * TILE_SIZE, TILES_TALL * TILE_SIZE);
            }
        }

        // Update and render
        while (SDL_GetTicks() - startTime < MS_PER_FRAME) {
            continue;
        }

        if (onlyUpdateOnKeyPress && wasKeyPressed) {
            wasKeyPressed = false;
            SDL_KeyCode key = InputQueuePop(&inputQueue);
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
            // TODO handle this key this frame.

            if (ateLastFrame) {
                snake.length++;
                ateLastFrame = false;
            }
            SnakeUpdate(snake, direction);
        }

        if (!onlyUpdateOnKeyPress && SDL_GetTicks() - updateTime >= MS_PER_UPDATE) {
            SDL_KeyCode key = InputQueuePop(&inputQueue);
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
            // TODO handle this key this frame.

            if (ateLastFrame) {
                snake.length++;
                ateLastFrame = false;
            }
            SnakeUpdate(snake, direction);
            // handle collisions
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
                ScoreTextUpdate(scoreText, score);
                prevScore = score;
            }
            
            updateTime = SDL_GetTicks();
        }

        DrawApples();
        SnakeDraw(snake);

        // draw text
        SDL_Surface* textSurface = TTF_RenderText_Blended(font, scoreText, (SDL_Color) {0, 0, 0, 255});
        assert(textSurface);
        SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
        assert(textTexture);
        SDL_FreeSurface(textSurface);
        i32 textWidth, textHeight;
        TTF_SizeText(font, scoreText, &textWidth, &textHeight);
        SDL_Rect textRect = (SDL_Rect){
            .x = 10,
            .y = 10,
            .w = textWidth,
            .h = textHeight,
        };
        SDL_RenderCopy(renderer, textTexture, nil, &textRect);

        SDL_RenderPresent(renderer);
        if (textTexture) {
            SDL_DestroyTexture(textTexture);
        }
        startTime = SDL_GetTicks();
    }

    return 0;
}
