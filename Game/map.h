#ifndef MAP_H
#define MAP_H

#include <stddef.h>

typedef struct {
    int x, y;
    int w, h;
} Platform;

void init_map(int screen_w, int screen_h, float difficulty);
void free_map(void);
const Platform* get_platforms(void);
int get_platform_count(void);

#endif