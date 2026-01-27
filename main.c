#include <SDL2/SDL.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>

#define WINDOW_W 800
#define WINDOW_H 600

#define SAMPLE_RATE 44100
#define AMPLITUDE 0.2f

typedef struct {
    SDL_AudioDeviceID device;
    Uint32 lastBeepTime;
    bool wasCollidingY;  // Track Y-axis collision state
    bool wasCollidingX;  // Track X-axis collision state
} SoundManager;

void InitSoundManager(SoundManager* sm) {
    SDL_AudioSpec want, have;
    SDL_memset(&want, 0, sizeof(want));
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = NULL;

    sm->device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    sm->lastBeepTime = 0;
    sm->wasCollidingY = false;
    sm->wasCollidingX = false;
    SDL_PauseAudioDevice(sm->device, 0);
}

void PlayBeep(SoundManager* sm, int duration_ms, int beepCooldown) {
    Uint32 currentTime = SDL_GetTicks();
    if (currentTime - sm->lastBeepTime < beepCooldown) return;

    SDL_ClearQueuedAudio(sm->device);

    const int num_samples = (SAMPLE_RATE * duration_ms) / 1000;
    Sint16 beep[num_samples];

    for (int i = 0; i < num_samples; i++) {
        float t = (float)i / SAMPLE_RATE;
        beep[i] = (Sint16)(sinf(2.0f * M_PI * 440.0f * t) * 32767.0f);
    }

    SDL_QueueAudio(sm->device, beep, num_samples * sizeof(Sint16));
    sm->lastBeepTime = currentTime;
}

void CLoseSoundManager(SoundManager* sm) {
    SDL_CloseAudioDevice(sm->device);
}

typedef struct {
    float x, y;     // Pos
    float vx, vy;   // Velocity
    float radius;   // Size
} Ball;

int main(int argc, char* argv[]) {
    // Init SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        printf("SDL could not initialize audio! SDL_Error: %s/n", SDL_GetError());
        return 1;
    }
    
    SoundManager soundManager;
    InitSoundManager(&soundManager);

    // Create a window
    SDL_Window* window = SDL_CreateWindow(
        "LOL",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        WINDOW_W,
        WINDOW_H,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        printf("No window for you! SDL_Error: %s\n", SDL_GetError());
        CLoseSoundManager(&soundManager);
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    Ball ball = {400, 100, 0, 0, 20};

    // Main loop
    SDL_Event event;
    int running = 1;
    while (running) {

        // --- Events ---

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                int mouseX, mouseY;
                SDL_GetMouseState(&mouseX, &mouseY);

                ball.vx = (mouseX - ball.x) * 0.1f;
                ball.vy = (mouseY - ball.y) * 0.1f;
            }
        }

        // --- Renderer ---

        // Clear Screen
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // --- Ball ---

        // Apply gravity
        ball.vy += 0.5f;

        // Update pos
        ball.y += ball.vy;
        ball.x += ball.vx;

        // Stop tiny ahh movement
        if (ball.vx < 0.1f && ball.vx > -0.1f &&ball.vx != 0.0) {
            ball.vx = 0;
        }
        if (ball.vy < 0.1f && ball.vy > -0.1f &&ball.vy != 0.0) {
            ball.vy = 0;
        }

        // Y physics
        bool isCollidingY = (ball.y + ball.radius) >= WINDOW_H;
        if (isCollidingY) {
            ball.y = WINDOW_H - ball.radius;
            ball.vy = -ball.vy * 0.8f;
            // Friction
            ball.vx = ball.vx * 0.995f;
            // Play beep only on first collision
            if (!soundManager.wasCollidingY) {
                PlayBeep(&soundManager, 200, 200);
            }
        }
        soundManager.wasCollidingY = isCollidingY;

        // X Physics
        bool isCollidingXLeft = (ball.x - ball.radius) <= 0;
        bool isCollidingXRight = (ball.x + ball.radius) >= WINDOW_W;
        bool isCollidingX = isCollidingXLeft || isCollidingXRight;

        if (isCollidingXLeft) {
            ball.x = 0 + ball.radius;
            ball.vx = -ball.vx * 0.9f;
        }
        if (isCollidingXRight) {
            ball.x = WINDOW_W - ball.radius;
            ball.vx = -ball.vx * 0.9f;
        }
        if (isCollidingX && !soundManager.wasCollidingX) {
            PlayBeep(&soundManager, 200, 200);
        }
        soundManager.wasCollidingX = isCollidingX;

        // --- Drawing ---

        // Draw the ball
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        for (int w = 0; w < ball.radius * 2; w++) {
            for (int h = 0; h < ball.radius * 2; h++) {
                int dx = ball.radius - w;
                int dy = ball.radius - h;
                if ((dx * dx + dy * dy) <= (ball.radius * ball.radius)) {
                    SDL_RenderDrawPoint(renderer, ball.x + dx, ball.y + dy);
                }
            }
        }

        // --- Stuff ---
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    // Cleanup
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    CLoseSoundManager(&soundManager);
    SDL_Quit();
    return 0;
}