#include <portaudio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../include/voice_chat.h"

int main() {
    PaError err;
    PaStream *stream;
    Room room = {0};

    // Initialize PortAudio
    err = Pa_Initialize();
    if (err != paNoError) {
        fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
        return 1;
    }

    // Open audio stream
    err = Pa_OpenDefaultStream(&stream, NUM_CHANNELS, 0, PA_SAMPLE_FORMAT, SAMPLE_RATE, FRAMES_PER_BUFFER, audioCallback, &room);
    if (err != paNoError) {
        fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
        return 1;
    }

    // Initialize UDP server
    initUdpServer(&room);

    // Start audio stream
    err = Pa_StartStream(stream);
    if (err != paNoError) {
        fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
        return 1;
    }

    printf("Voice chat server running. Press Enter to stop.\n");
    getchar();

    // Cleanup
    Pa_StopStream(stream);
    Pa_Terminate();
    return 0;
}