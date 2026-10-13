#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "map.h"

#define max(a, b) ((a) > (b) ? (a) : (b))

static Platform *platforms = NULL;
static int platform_count = 0;

void init_map(int screen_w, int screen_h, float difficulty) {
    if (platforms) {
        free(platforms);
    }

    platform_count = 15;
    platforms = malloc(platform_count * sizeof(Platform));

    // Estimate player width based on your main.c scaling (10% of screen width)
    int estimated_player_w = max(3, (int)(screen_w * 0.1f));

    int current_x = 5;
    int current_y = screen_h - 12;

    for (int i = 0; i < platform_count; i++) {
        // 1. Width: Make platforms comfortably wider than the character
        int min_w = estimated_player_w * 2;
        int max_w = estimated_player_w * 4;
        platforms[i].w = min_w + (rand() % (max_w - min_w + 1));

        // 2. Height: Vary height so they aren't all thin strips (mix thin ledges and thick blocks)
        if (i == 0) {
            platforms[i].h = 4; // Start platform is nice and solid
        } else {
            // Randomize height between 2 (thin ledge) and 8 (thick block/pillar)
            platforms[i].h = 2 + (rand() % 7);
        }

        platforms[i].x = current_x;
        platforms[i].y = current_y;

        // 3. Gaps: Scaled to character width so the jump is actually proportioned correctly
        int min_gap = (int)(estimated_player_w * 0.8f * difficulty);
        int max_gap = (int)(estimated_player_w * 2.0f * difficulty);
        int gap = min_gap + (rand() % (max_gap - min_gap + 1));

        current_x += platforms[i].w + gap;

        // 4. Vertical flow: Mix climbs and drops within safe bounds
        int y_dir = (i % 2 == 0) ? -1 : 1;
        int y_jump = (rand() % 4) + 3; 
        
        current_y += y_dir * y_jump;

        // Keep well within screen bounds, leaving breathing room at top and bottom for UI/graphics
        if (current_y < 8) current_y = 8;
        if (current_y > screen_h - 8) current_y = screen_h - 8;
    }
}

void free_map(void) {
    free(platforms);
    platforms = NULL;
    platform_count = 0;
}

const Platform* get_platforms(void) {
    return platforms;
}

int get_platform_count(void) {
    return platform_count;
}