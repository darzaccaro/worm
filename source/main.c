#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <SDL_mixer.h>

#include "prelude.h"
#include "config.h"
#include "types.h"
#include "globals.h"

void PlaySound(SFX sfx) {
    assert(soundMap[sfx]);
    Mix_PlayChannel(-1, soundMap[sfx], 0);
}

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
        } else {
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

Worm CreateWorm(V2f position, V2f direction, u64 length) {
    Worm worm = { 0 };
    assert(length <= MAX_WORM_LENGTH);
    worm.length = length;
    worm.directions[0] = direction;
    worm.positions[0] = position;
    for (i32 i = 1; i < length; i++) {
        V2f offset = V2fMul(direction, -i);
        worm.positions[i] = V2fAddV2f(position, offset);
        worm.directions[i] = direction;
    }
    return worm;
}


Worm GrowWorm() {
    Worm next = { 0 };
    next.length = worm.length + 1;
    assert(next.length <= MAX_WORM_LENGTH);
    
    for (u64 i = 0; i < next.length; i++) {
        if (i < worm.length) {
            next.positions[i] = worm.positions[i];
            next.directions[i] = worm.directions[i];
        } else {
            next.positions[i] = next.positions[worm.length - 1];
            next.directions[i] = next.directions[worm.length - 1];
        }
    }
    return next;
}


void UpdateWorm(V2f direction) {
    for (u64 i = worm.length - 1; i > 0; i--) {
        worm.positions[i] = worm.positions[i - 1];
        worm.directions[i] = worm.directions[i - 1];
    }
    worm.directions[0] = direction;
    worm.positions[0] = V2fAddV2f(worm.positions[0], worm.directions[0]);
}

void DrawWorm() {
    for (u64 i = 0; i < worm.length; i++) {
        SpriteName sprite = 0;
        f64 angle = 0;
        V2f direction = { 0 };
        if (i == 0) {
            direction = worm.directions[i];
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

        } else if (i == worm.length - 1) {
            direction = worm.directions[i-1];
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
            
        } else {
            V2f pa = worm.positions[i - 1];
            V2f pb = worm.positions[i];
            V2f pc = worm.positions[i + 1];
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
            } else if (V2fEqV2f(diffA, (V2f) { 0, -1 }) && V2fEqV2f(diffC, (V2f) { 1, 0 })
                || V2fEqV2f(diffA, (V2f) { 1, 0 }) && V2fEqV2f(diffC, (V2f) { 0, -1 })
                ) {
                angle = 90;
                sprite = SN_BODY_CURVED;
            } else if (V2fEqV2f(diffA, (V2f) { 0, 1 }) && V2fEqV2f(diffC, (V2f) { -1, 0 })
                || V2fEqV2f(diffA, (V2f) { -1, 0 }) && V2fEqV2f(diffC, (V2f) { 0, 1 })
                ) {
                angle = -90;
                sprite = SN_BODY_CURVED;
            } else if (V2fEqV2f(diffA, (V2f) { -1, 0 }) && V2fEqV2f(diffC, (V2f) { 0, -1 })
                || V2fEqV2f(diffA, (V2f) { 0, -1 }) && V2fEqV2f(diffC, (V2f) { -1, 0 })) {
                angle = 0;
                sprite = SN_BODY_CURVED;
            } else {
                sprite = SN_TRANSPARENT;
            }
            
        }
        DrawSprite(sprite, worm.positions[i].x, worm.positions[i].y, angle);
    }
}

void SpawnApple() {
    u64 appleCount = 0;
    for (i32 i = 0; i < MAX_APPLES; i++) {
        if (apples[i].isActive) appleCount++;
    }
    u64 validPositionsCount = TILES_TALL * TILES_WIDE - worm.length - appleCount;
    V2f* validPositions = malloc(sizeof(V2f) * validPositionsCount);
    u64 validPositionsFilled = 0;
    for (u64 y = 0; y < TILES_TALL; y++) {
        for (u64 x = 0; x < TILES_WIDE; x++) {
            bool isValid = true;
            V2f position = (V2f){x, y};
            for (u64 a = 0; a < MAX_APPLES; a++) {
                if (apples[a].isActive) {
                    if (V2fEqV2f(apples[a].position, position)) {
                      isValid = false;
                      break;
                    }
                }
            }
            if (!isValid) continue;
            for (u64 s = 0; s < worm.length; s++) {
                if (V2fEqV2f(worm.positions[s], position)) {
                    isValid = false;
                    break;
                }
            }
            if (isValid) {
                assert(validPositionsFilled < validPositionsCount);
                validPositions[validPositionsFilled++] = position;
            }
        }
    }
    assert(validPositionsFilled == validPositionsCount);
    for (u64 i = 0; i < MAX_APPLES; i++) {
        if (apples[i].isActive) continue;
        u64 p = rand() % validPositionsCount;
        V2f position = validPositions[p];
        apples[i].position = position;
        apples[i].isActive = true;
        break;
    }
    free(validPositions);
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

void DrawImage(Image image, f32 x, f32 y) {
    SDL_Rect rect = (SDL_Rect) {
        .x = x,
        .y = y,
        .w = image.width,
        .h = image.height,
    };
    SDL_RenderCopy(renderer, image.texture, nil, &rect);
}

void GameModeStart() {
    DrawImage(titleImage, WINDOW_WIDTH / 2 - titleImage.width / 2,
              WINDOW_HEIGHT / 2 - titleImage.height - 2);
    {
        const char* text = "Press any key to play";
        i32 textWidth, textHeight;
        TTF_SizeText(font, text, &textWidth, &textHeight);
        DrawText(text, WINDOW_WIDTH / 2 - textWidth / 2, WINDOW_HEIGHT / 4 - textHeight - 2, textWidth, textHeight);
    }
}

void GameModePlay() {
    if (SDL_GetTicks() - updateTime >= MS_PER_UPDATE) {
        SDL_KeyCode key = PopInput();
        V2f direction = worm.directions[0];
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
            framesToGrow = GROWTH_FACTOR;
            ateLastFrame = false;
        }
        if (framesToGrow > 0) {
            worm = GrowWorm();
            framesToGrow--;
        }

        UpdateWorm(direction);

        // handle boundary collisions
        if (worm.positions[0].x < 0 || worm.positions[0].x >= TILES_WIDE || worm.positions[0].y < 0 || worm.positions[0].y >= TILES_TALL) {
            timeOfDeath = SDL_GetTicks();
            gameMode = GM_GAME_OVER;
            PlaySound(SFX_DIE);
            PlaySound(SFX_GAMEOVER);
            return;
        }
        // handle self collisions
        for (u64 i = 1; i < worm.length; i++) {
            V2f head = worm.positions[0];
            V2f body = worm.positions[i];
            if (V2fEqV2f(head, body)) {
                timeOfDeath = SDL_GetTicks();
                gameMode = GM_GAME_OVER;
                PlaySound(SFX_DIE);
                PlaySound(SFX_GAMEOVER);
                return;
            }
        }


        for (u64 i = 0; i < MAX_APPLES; i++) {
            if (!apples[i].isActive) continue;
            if (V2fEqV2f(apples[i].position, worm.positions[0])) {
                score += 100;
                apples[i].isActive = false;
                ateLastFrame = true;
                i32 sfx = SFX_EAT1 + rand() % 6;
                PlaySound(sfx);
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
    DrawWorm(worm);
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

    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        assert(false);
    }
    
    if (TTF_Init() != 0) {
        assert(false);
    }

    if (IMG_Init(IMG_INIT_PNG) == 0) {
        assert(false);
    }
        
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        assert(false);
    }
    
    SDL_Window* window = SDL_CreateWindow("Worm", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    assert(window);
    
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    assert(renderer);
    
    font = TTF_OpenFont("assets/fonts/roboto.ttf", 48);
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

    {
        SDL_Surface* surface = IMG_Load("assets/images/title.png");
        assert(surface);
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        assert(texture);
        titleImage = (Image){
            .texture = texture, 
            .width = surface->w, 
            .height = surface->h,
        };
        SDL_FreeSurface(surface);
    }

    soundMap[SFX_STARTUP] = Mix_LoadWAV("assets/audio/sfx/startup.wav");
    assert(soundMap[SFX_STARTUP]);
    soundMap[SFX_DIE] = Mix_LoadWAV("assets/audio/sfx/die.wav");
    assert(soundMap[SFX_DIE]);
    soundMap[SFX_GAMEOVER] = Mix_LoadWAV("assets/audio/sfx/gameover.wav");
    assert(soundMap[SFX_GAMEOVER]);
    soundMap[SFX_PRESS] = Mix_LoadWAV("assets/audio/sfx/press.wav");
    assert(soundMap[SFX_GAMEOVER]);
    soundMap[SFX_EAT1] = Mix_LoadWAV("assets/audio/sfx/eat1.wav");
    assert(soundMap[SFX_EAT1]);
    soundMap[SFX_EAT2] = Mix_LoadWAV("assets/audio/sfx/eat2.wav");
    assert(soundMap[SFX_EAT2]);
    soundMap[SFX_EAT3] = Mix_LoadWAV("assets/audio/sfx/eat3.wav");
    assert(soundMap[SFX_EAT3]);
    soundMap[SFX_EAT4] = Mix_LoadWAV("assets/audio/sfx/eat4.wav");
    assert(soundMap[SFX_EAT4]);
    soundMap[SFX_EAT5] = Mix_LoadWAV("assets/audio/sfx/eat5.wav");
    assert(soundMap[SFX_EAT5]);
    soundMap[SFX_EAT6] = Mix_LoadWAV("assets/audio/sfx/eat6.wav");
    assert(soundMap[SFX_EAT6]);
    soundMap[SFX_EAT7] = Mix_LoadWAV("assets/audio/sfx/eat7.wav");
    assert(soundMap[SFX_EAT7]);


    i32 windowWidth, windowHeight, rendererWidth, rendererHeight;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    SDL_GetRendererOutputSize(renderer, &rendererWidth, &rendererHeight);
    f32 scaleX = (f32)rendererWidth / (f32)windowWidth;
    f32 scaleY = (f32)rendererHeight / (f32)windowHeight;
    assert(scaleX == scaleY);   

    SDL_RenderSetScale(renderer, scaleX, scaleY);

GAME_INIT:
    UpdateScoreText();

    worm = CreateWorm((V2f) { 4, 2 }, (V2f) { 1, 0 }, 4);

    isRunning = true;
    startTime = SDL_GetTicks();
    updateTime = SDL_GetTicks();
    appleTime = SDL_GetTicks();
    prevScore = score;
    ateLastFrame = false;
    wasKeyPressed = false;
    SDL_KeyCode lastKey = nil;
    PlaySound(SFX_STARTUP);

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
                    if (SDL_GetTicks() > timeOfDeath + RESPAWN_TIME) {
                      score = 0;
                      timeOfDeath = 0;
                      gameMode = GM_PLAY;
                      goto GAME_INIT;
                    }
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
                        PlaySound(SFX_PRESS);
                    }
                    break;
                }
                case SDLK_RIGHT: {
                    if (!event.key.repeat && lastKey != SDLK_LEFT) {
                        lastKey = SDLK_RIGHT;
                        PushInput(event.key.keysym.sym);
                        wasKeyPressed = true;
                        PlaySound(SFX_PRESS);
                    }
                    break;
                }
                case SDLK_UP: {
                    if (!event.key.repeat && lastKey != SDLK_DOWN) {
                        lastKey = SDLK_UP;
                        PushInput(event.key.keysym.sym);
                        wasKeyPressed = true;
                        PlaySound(SFX_PRESS);
                    }
                    break;
                }
                case SDLK_DOWN: {
                    if (!event.key.repeat && lastKey != SDLK_UP) {
                        lastKey = SDLK_DOWN;
                        PushInput(event.key.keysym.sym);
                        wasKeyPressed = true;
                        PlaySound(SFX_PRESS);
                    }
                    break;
                }
                }
            }
        }

    UPDATE_AND_RENDER:
        if (SDL_GetTicks() - startTime < MS_PER_FRAME) {
            goto EVENT_LOOP;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

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
