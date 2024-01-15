#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float  f32;
typedef double f64;

typedef const char* cstring;

#define nil      0
#define global   static
#define persist  static
#define internal static

typedef struct {
    f32 x;
    f32 y;
} V2f;

V2f V2fAddV2f(V2f a, V2f b) {
    return (V2f) { a.x + b.x, a.y + b.y };
}

V2f V2fMul(V2f a, f32 b) {
    return (V2f) { a.x* b, a.y* b };
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
    return (V2f) { v.x / magnitude, v.y / magnitude };
}
