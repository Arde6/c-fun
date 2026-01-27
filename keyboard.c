#include <SDL2/SDL.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <SDL2/SDL_stdinc.h>
#include "common.h"
#include "keyboard.h"
#include "soundManager.h"

// Single-definition of controls and count
const SDL_Scancode controls[] = {
    SDL_SCANCODE_UP,
    SDL_SCANCODE_DOWN,
    SDL_SCANCODE_LEFT,
    SDL_SCANCODE_RIGHT,
    // --- Piano ---
    SDL_SCANCODE_Q,
    SDL_SCANCODE_W,
    SDL_SCANCODE_E,
    SDL_SCANCODE_R,
    SDL_SCANCODE_T,
    SDL_SCANCODE_Y,
    SDL_SCANCODE_U,
    SDL_SCANCODE_I,
    SDL_SCANCODE_O,
    SDL_SCANCODE_P,
    SDL_SCANCODE_Z,
    SDL_SCANCODE_X,
    SDL_SCANCODE_C,
    SDL_SCANCODE_V,
    SDL_SCANCODE_B,
    SDL_SCANCODE_N,
    SDL_SCANCODE_M,
    SDL_SCANCODE_COMMA,
    SDL_SCANCODE_PERIOD,
    SDL_SCANCODE_MINUS,
    SDL_SCANCODE_2,
    SDL_SCANCODE_3,
    SDL_SCANCODE_4,
    SDL_SCANCODE_6,
    SDL_SCANCODE_7,
    SDL_SCANCODE_9,
    SDL_SCANCODE_0,
    SDL_SCANCODE_A,
    SDL_SCANCODE_D,
    SDL_SCANCODE_F,
    SDL_SCANCODE_H,
    SDL_SCANCODE_J,
    SDL_SCANCODE_K,
    SDL_SCANCODE_L,
};
const int numControls = sizeof(controls) / sizeof(controls[0]);

void HandleKeyPress (SDL_Scancode key, Ball* ball, NotePlayer* np) {
    switch (key) {
        case SDL_SCANCODE_UP:
            ball->vy = -5.0f;
            break;
        case SDL_SCANCODE_DOWN:
            ball->vy = 5.0f;
            break;
        case SDL_SCANCODE_LEFT:
            ball->vx = -5.0f;
            break;
        case SDL_SCANCODE_RIGHT:
            ball->vx = 5.0f;
            break;
        // --- Piano ---
        case SDL_SCANCODE_Q: 
            if (!np->isPlaying)
                PlayNote(np, 261.63f);
            UpdateNote(np);
            break;  // C4
        default:
            break;
    }
}

void HandleKeyRelease(SDL_Scancode key, NotePlayer* np) {
    if (np->isPlaying) {
        StopNote(np);
    }
}

void CheckKeys(Ball* ball, NotePlayer* np) {
    bool anyPressed = false;
    const Uint8* keystates = SDL_GetKeyboardState(NULL);
    for (int i = 0; i < numControls; i++) {
        if (keystates[controls[i]]) {
            HandleKeyPress(controls[i], ball, np);  // Call handler for the pressed key
            anyPressed = true;
        }
    }
    if (!anyPressed) {
        HandleKeyRelease(0, np);  // No key is pressed
    }
}