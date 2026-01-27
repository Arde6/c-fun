#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <SDL2/SDL.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <SDL2/SDL_stdinc.h>
#include "common.h"
#include "soundManager.h"

extern const SDL_Scancode controls[];
extern const int numControls;

void HandleKeyPress (SDL_Scancode key, Ball* ball, NotePlayer* np);
void CheckKeys(Ball* ball, NotePlayer* np);

#endif