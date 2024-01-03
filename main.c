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

#define nil 0

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

#define TILE_SIZE     64
#define TILES_WIDE    16
#define TILES_TALL    12
#define WINDOW_WIDTH  TILES_WIDE * TILE_SIZE
#define WINDOW_HEIGHT TILES_TALL * TILE_SIZE
#define FRAMES_PER_SECOND 60
#define MS_PER_FRAME ((1.f/60.f) * 1000.f)
#define MS_PER_UPDATE MS_PER_FRAME * 10.f

static SDL_Renderer* renderer;

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

typedef struct {
    V2f* positions;
    V2f direction;
    u64 length;
} Snake;

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
        SDL_Rect rect = {
            .x = snake.positions[i].x * TILE_SIZE,
            .y = snake.positions[i].y * TILE_SIZE,
            .w = TILE_SIZE,
            .h = TILE_SIZE
        };
        SDL_RenderFillRect(renderer, &rect);
    }
}

int main(int argc, char* args[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        assert(false);
    }
    SDL_Window* window = SDL_CreateWindow("Snake Rogue", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    assert(window);

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    assert(renderer);

    Snake snake = SnakeCreate((V2f) { 1, 2 }, (V2f) { 1, 0 }, 4);

    bool isRunning = true;
    u32 startTime = SDL_GetTicks();
    u32 updateTime = SDL_GetTicks();
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
                case SDLK_LEFT: {
                    if (!event.key.repeat) {
                        snake.direction = (V2f){ .x = -1.f, .y = 0.f };
                    }
                    break;
                }
                case SDLK_RIGHT: {
                    if (!event.key.repeat) {
                        snake.direction = (V2f){ .x = 1.f, .y = 0.f };
                    }
                    break;
                }
                case SDLK_UP: {
                    if (!event.key.repeat) {
                        snake.direction = (V2f){ .x = 0.f, .y = -1.f };
                    }
                    break;
                }
                case SDLK_DOWN: {
                    if (!event.key.repeat) {
                        snake.direction = (V2f){ .x = 0.f, .y = 1.f };
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
            SnakeUpdate(snake);
            updateTime = SDL_GetTicks();
        }

        SnakeDraw(snake);
        SDL_RenderPresent(renderer);
        startTime = SDL_GetTicks();
    }


    return 0;
}
