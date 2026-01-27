#include <SDL2/SDL.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <SDL2/SDL_stdinc.h>
#include "common.h"
#include "soundManager.h"

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

void CloseSoundManager(SoundManager* sm) {
    SDL_CloseAudioDevice(sm->device);
}

void InitNotePlayer(NotePlayer* np) {
    SDL_AudioSpec want, have;
    SDL_memset(&want, 0, sizeof(want));
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;  // Mono for simplicity
    want.samples = 1024;
    want.callback = NULL;  // No callback, we'll queue audio

    np->device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    np->isPlaying = false;
    np->frequency = 440.0f;  // Default frequency (A4)
    SDL_PauseAudioDevice(np->device, 0);  // Unpause audio
}

void GenerateNote(NotePlayer* np, Sint16* buffer, int num_samples) {
    const double two_pi = 2.0 * M_PI;
    const int attackMs = 10;
    const int releaseMs = 40;
    const int attackSamples = (SAMPLE_RATE * attackMs) / 1000;
    const int releaseSamples = (SAMPLE_RATE * releaseMs) / 1000;

    double phase = np->phase;
    double phaseInc = two_pi * np->frequency / (double)SAMPLE_RATE;

    for (int i = 0; i < num_samples; ++i) {
        // compute envelope
        double env = 1.0;
        if (np->samplesPlayed < attackSamples) {
            env = (double)np->samplesPlayed / (double)attackSamples;
        }
        if (np->releasing) {
            int relPos = np->samplesPlayed - np->releaseStartSample;
            if (relPos >= releaseSamples) {
                env = 0.0;
            } else {
                double relGain = 1.0 - ((double)relPos / (double)releaseSamples);
                // if still in attack region, multiply; otherwise replace
                if (np->samplesPlayed < attackSamples) env *= relGain;
                else env = relGain;
            }
        }
        double sample = sin(phase) * AMPLITUDE * env;
        // scale to 16-bit PCM
        buffer[i] = (Sint16)(sample * 32767.0);

        // advance
        phase += phaseInc;
        if (phase >= two_pi) phase -= two_pi;
        np->samplesPlayed++;
    }

    np->phase = phase;
}

void PlayNote(NotePlayer* np, float frequency) {
    if (np->isPlaying) return;  // Already playing

    np->frequency = frequency;
    np->isPlaying = true;
    np->phase = 0.0;
    np->samplesPlayed = 0;
    np->releasing = false;
    np->releaseStartSample = 0;

    const int blockMs = 50;
    const int blockSamples = (SAMPLE_RATE * blockMs) / 1000;
    Sint16 buf[blockSamples];
    GenerateNote(np, buf, blockSamples);
    SDL_QueueAudio(np->device, buf, blockSamples * sizeof(Sint16));
}

void StopNote(NotePlayer* np) {
    if (!np->isPlaying) return;  // Not playing

    np->releasing = true;
    np->releaseStartSample = np->samplesPlayed;

    np->isPlaying = false;
}

void UpdateNote(NotePlayer* np) {
    if (!np->isPlaying) return;
    Uint32 queuedBytes = SDL_GetQueuedAudioSize(np->device);
    int queuedSamples = queuedBytes / sizeof(Sint16);
    const int lowWaterMs = 100;
    const int lowWaterSamples = (SAMPLE_RATE * lowWaterMs) / 1000;
    const int blockMs = 50;
    const int blockSamples = (SAMPLE_RATE * blockMs) / 1000;

    if (queuedSamples < lowWaterSamples) {
        Sint16 buf[blockSamples];
        GenerateNote(np, buf, blockSamples);
        SDL_QueueAudio(np->device, buf, blockSamples * sizeof(Sint16));
    }

    if (np->releasing) {
        const int releaseMs = 40;
        const int releaseSamples = (SAMPLE_RATE * releaseMs) / 1000;
        if ((np->samplesPlayed - np->releaseStartSample) >= releaseSamples) {
            np->isPlaying = false;
            np->releasing = false;
            // optional: SDL_ClearQueuedAudio(np->device);
        }
    }
}

