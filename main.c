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

#define TILE_SIZE     32
#define TILES_WIDE    32
#define TILES_TALL    24
#define WINDOW_WIDTH  TILES_WIDE * TILE_SIZE
#define WINDOW_HEIGHT TILES_TALL * TILE_SIZE
#define FRAMES_PER_SECOND 60
#define MS_PER_FRAME ((1.f/60.f) * 1000.f)
#define MS_PER_UPDATE MS_PER_FRAME * 10.f
#define MAX_APPLES 16
#define MAX_INPUT_QUEUE_SIZE 3
#define MAX_SCORE_TEXT 128

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

typedef struct {
    V2f position;
    bool isActive;
} Apple;

typedef struct {
    V2f* positions;
    V2f direction;
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

static SDL_Renderer* renderer;
static TTF_Font* font;
static Apple apples[MAX_APPLES];
static Snake snake;
static InputQueue inputQueue;
static u64 score;
static char* scoreText[MAX_SCORE_TEXT];

void ScoreTextUpdate(char* scoreText, u64 score) {
    sprintf_s(scoreText, MAX_SCORE_TEXT, "Score: %llu", score);
}

Snake SnakeCreate(V2f position, V2f direction, u64 length) {
    Snake snake = { 0 };
    snake.positions = malloc(sizeof(V2f) * length);
    assert(snake.positions);
    snake.length = length;
    snake.direction = direction;
    snake.positions[0] = position;
    for (u64 i = 1; i < length; i++) {
        V2f pos = V2fAddV2f(position, V2fMul(direction, i));
        snake.positions[i] = (V2f){
            .x = pos.x,
            .y = pos.y,
        };
    }
    return snake;
}

void SnakeUpdate(Snake snake) {
    // body
    for (u64 i = snake.length - 1; i > 0; i--) {
        snake.positions[i] = snake.positions[i - 1];
    }
    // head
    snake.positions[0] = V2fAddV2f(snake.positions[0], snake.direction);
}

void SnakeDraw(Snake snake) {
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    for (u64 i = 0; i < snake.length; i++) {
        SDL_FRect rect = {
            .x = snake.positions[i].x * TILE_SIZE,
            .y = snake.positions[i].y * TILE_SIZE,
            .w = TILE_SIZE,
            .h = TILE_SIZE
        };
        SDL_RenderFillRectF(renderer, &rect);
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
        /*
        for (u64 i = 0; i < snake.length; i++) {
            if (V2fEqV2f(pos, snake.positions[i])) {

            }
        }
        */
        apples[i].position = pos;
        apples[i].isActive = true;
        break;
    }
}

void DrawApples() {
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    for (u64 i = 0; i < ARRAY_COUNT(apples); i++) {
        if (!apples[i].isActive) continue;
        SDL_FRect rect = {
            .x = apples[i].position.x * TILE_SIZE,
            .y = apples[i].position.y * TILE_SIZE,
            .w = TILE_SIZE,
            .h = TILE_SIZE,
        };
        SDL_RenderFillRectF(renderer, &rect);
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

    font = TTF_OpenFont("assets/fonts/Roboto-Regular.ttf", 64);
    assert(font);

    SDL_Window* window = SDL_CreateWindow("Snake Rogue", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    assert(window);
    

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    assert(renderer);

    i32 windowWidth, windowHeight, rendererWidth, rendererHeight;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    SDL_GetRendererOutputSize(renderer, &rendererWidth, &rendererHeight);
    f32 scaleX = (f32)rendererWidth / (f32)windowWidth;
    f32 scaleY = (f32)rendererHeight / (f32)windowHeight;
    assert(scaleX == scaleY);
        

    SDL_RenderSetScale(renderer, scaleX, scaleY);

    ScoreTextUpdate(scoreText, score);

    snake = SnakeCreate((V2f) { 1, 2 }, (V2f) { 1, 0 }, 4);

    bool isRunning = true;
    u32 startTime = SDL_GetTicks();
    u32 updateTime = SDL_GetTicks();
    u64 prevScore = score;
    bool ateLastFrame = false;
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
                    }
                    break;
                }
                }
            }
        }
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Update and render
        while (SDL_GetTicks() - startTime < MS_PER_FRAME) {
            continue;
        }

        if (SDL_GetTicks() - updateTime >= MS_PER_UPDATE) {
            SDL_KeyCode key = InputQueuePop(&inputQueue);
            if (key == SDLK_LEFT) {
                snake.direction = (V2f){ -1, 0 };
            }
            if (key == SDLK_RIGHT) {
                snake.direction = (V2f){ 1, 0 };
            }
            if (key == SDLK_UP) {
                snake.direction = (V2f){ 0, -1 };
            }
            if (key == SDLK_DOWN) {
                snake.direction = (V2f){ 0, 1 };
            }
            // TODO handle this key this frame.

            if (ateLastFrame) {
                snake.length++;
                ateLastFrame = false;
            }
            SnakeUpdate(snake);
            // handle collisions
            for (u64 i = 0; i < MAX_APPLES; i++) {
                if (!apples[i].isActive) continue;
                if (V2fEqV2f(apples[i].position, snake.positions[0])) {
                    score += 100;
                    apples[i].isActive = false;
                    ateLastFrame = true;
                }
            }
            
            SpawnApple();

            if (prevScore != score) {
                ScoreTextUpdate(scoreText, score);
                prevScore = score;
            }
            
            updateTime = SDL_GetTicks();
        }

        DrawApples();
        SnakeDraw(snake);

        SDL_Surface* textSurface = TTF_RenderText_Blended(font, scoreText, (SDL_Color) {255, 255, 255, 255});
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
        SDL_DestroyTexture(textTexture);
        startTime = SDL_GetTicks();
    }

    return 0;
}
