#include <SDL2/SDL.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <SDL2/SDL_stdinc.h>
#include "common.h"
#include "keyboard.h"
#include "soundManager.h"

#define WINDOW_W 800
#define WINDOW_H 600

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

    NotePlayer notePlayer;
    InitNotePlayer(&notePlayer);

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
        CloseSoundManager(&soundManager);
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    Ball ball = {400, 100, 0, 0, 20};

    // Enable text input for Unicode characters
    SDL_StartTextInput();

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
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym)
                {
                case SDLK_ESCAPE:
                    running = 0;
                    break;
                default:
                    break;
                }
            // } else if (event.type == SDL_TEXTINPUT) {
            //     printf("Text input: %s\n", event.text.text);
            //     // Check for specific characters like 'å', 'ö', 'ä'
            }
        }

        // --- Keyboard controls ---
        CheckKeys(&ball, &notePlayer);

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
    CloseSoundManager(&soundManager);
    SDL_Quit();
    return 0;
}