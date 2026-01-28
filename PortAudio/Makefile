CC = gcc
CFLAGS = -Wall -Iinclude -I/usr/include
LDFLAGS = -lportaudio -lm
TARGET = voice_chat_server

all: $(TARGET)

$(TARGET): src/main.o src/voice_chat.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

src/main.o: src/main.c include/voice_chat.h
	$(CC) $(CFLAGS) -c $< -o $@

src/voice_chat.o: src/voice_chat.c include/voice_chat.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) src/*.o
