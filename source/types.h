#pragma once

typedef struct {
  V2f position;
  bool isActive;
} Apple;

typedef struct {
  V2f positions[MAX_WORM_LENGTH];
  V2f directions[MAX_WORM_LENGTH];
  u64 length;
} Worm;

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
  SN_APPLE_EATEN,
} SpriteName;

typedef struct {
  SDL_Texture* texture;
  i32 width, height;
  i32 tileSize;
} SpriteSheet;

typedef struct {
  SDL_Texture* texture;
  i32 width, height;
} Image;

typedef enum {
  GM_START,
  GM_PLAY,
  GM_GAME_OVER,
} GameMode;

typedef enum {
  SFX_STARTUP,
  SFX_PRESS,
  SFX_EAT1,
  SFX_EAT2,
  SFX_EAT3,
  SFX_EAT4,
  SFX_EAT5,
  SFX_EAT6,
  SFX_EAT7,
  SFX_DIE,
  SFX_GAMEOVER,
  SFX_COUNT,
} SFX;