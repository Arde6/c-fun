#ifndef VOICE_CHAT_H
#define VOICE_CHAT_H

#include <portaudio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SAMPLE_RATE 44100
#define FRAMES_PER_BUFFER 512
#define NUM_CHANNELS 2
#define PA_SAMPLE_FORMAT paInt16
#define PA_SAMPLE_TYPE int16_t
#define UDP_PORT 5004
#define MAX_CLIENTS 10

typedef struct {
    int socket_fd;
    struct sockaddr_in addr;
} Client;

typedef struct {
    Client clients[MAX_CLIENTS];
    int num_clients;
} Room;

int audioCallback(const void *inputBuffer, void *outputBuffer,
                  unsigned long framesPerBuffer,
                  const PaStreamCallbackTimeInfo* timeInfo,
                  PaStreamCallbackFlags statusFlags,
                  void *userData);

void initUdpServer(Room* room);

#endif // VOICE_CHAT_H