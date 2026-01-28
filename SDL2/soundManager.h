#ifndef SOUNDMANAGER_H
#define SOUNDMANAGER_H

#include <SDL2/SDL.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <SDL2/SDL_stdinc.h>
#include "common.h"

#define SAMPLE_RATE 44100
#define AMPLITUDE 0.2f

typedef struct {
    SDL_AudioDeviceID device;
    Uint32 lastBeepTime;
    bool wasCollidingY;  // Track Y-axis collision state
    bool wasCollidingX;  // Track X-axis collision state
} SoundManager;

typedef struct {
    SDL_AudioDeviceID device;
    bool isPlaying;
    float frequency;        // Current frequency of the note
    double phase;           // Phase for waveform generation
    int samplesPlayed;      // Number of samples played
    int releaseStartSample; // Sample index where release started
    bool releasing;         // Whether the note is in the release phase
} NotePlayer;


void InitSoundManager(SoundManager* sm);
void PlayBeep(SoundManager* sm, int duration_ms, int beepCooldown);
void CloseSoundManager(SoundManager* sm);

void InitNotePlayer(NotePlayer* np);
void GenerateNote(NotePlayer* np, Sint16* buffer, int num_samples);
void PlayNote(NotePlayer* np, float frequency);
void StopNote(NotePlayer* np);
void UpdateNote(NotePlayer* np);

#endif