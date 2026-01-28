#include <portaudio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../include/voice_chat.h"

int audioCallback(const void *inputBuffer, void *outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData) {
    Room *room = (Room *)userData;
    const PA_SAMPLE_TYPE *in = (const PA_SAMPLE_TYPE *)inputBuffer;
    int i;

    // Send audio data to all connected clients
    for (i = 0; i < room->num_clients; i++) {
        sendto(room->clients[i].socket_fd, in, framesPerBuffer * NUM_CHANNELS * sizeof(PA_SAMPLE_TYPE), 0, (struct sockaddr *)&room->clients[i].addr, sizeof(room->clients[i].addr));
    }

    return paContinue;
}

void initUdpServer(Room* room) {
    int udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(UDP_PORT),
        .sin_addr.s_addr = INADDR_ANY
    };
    bind(udp_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    // Add the UDP socket as a client in the room
    room->clients[0].socket_fd = udp_fd;
    room->num_clients = 1;
}