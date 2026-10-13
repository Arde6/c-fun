#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <math.h>
#include "render.h"
#include "map.h"
#include "sprite.h"

#define max(a, b) ((a) > (b) ? (a) : (b))

// ---------- tuning (units: terminal cells and seconds) ----------
#define TICK_HZ        60
#define GRAVITY        400.0f   // cells/s^2
#define JUMP_SPEED     130.0f    // cells/s  (~5.6 cells high)
#define MOVE_SPEED     55.0f    // cells/s
#define ACCEL          500.0f   // cells/s^2 while a direction is held
#define FRICTION       160.0f   // cells/s^2 after release
#define JUMP_BUFFER    0.10     // s: a jump pressed just before landing still counts
#define JUMP_CUT       0.5f     // releasing jump early multiplies upward speed by this

// Fallback only (terminals without key-release events):
#define HOLD_FIRST     0.45     // s a first press counts as held (covers the OS repeat delay)
#define HOLD_REPEAT    0.12     // s a repeating key counts as held between repeat events
#define REPEAT_DETECT  0.80     // s: a second press within this of the first = key repeat

#define SPAWN_X  20.0f
#define SPAWN_Y  19.0f
#define RESPAWN_DEPTH 80.0f

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t resized = 0;

static void on_quit_signal(int sig) { (void)sig; running = 0; }
static void on_winch(int sig)       { (void)sig; resized = 1; }

static const char SHADE_RAMP[] = ".-=+*#%@";

static Sprite *player_idle_sprite = NULL;
static Sprite *player_walk_sprite = NULL;
static Sprite *player_walk_intro_sprite = NULL;
static Sprite *platform_sprite = NULL;

// ---------- terminal ----------
static struct termios orig_termios;
static bool kitty_on = false;

static void write_str(const char *s) {
    size_t len = strlen(s);
    while (len > 0) {
        ssize_t w = write(STDOUT_FILENO, s, len);
        if (w <= 0) return;
        s += w;
        len -= (size_t)w;
    }
}

static void disable_raw_mode(void) {
    if (kitty_on) { write_str("\x1b[<u"); kitty_on = false; }
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);   // keep ISIG so Ctrl+C works in fallback mode
    raw.c_iflag &= ~(IXON);            // Ctrl+S won't freeze the terminal
    raw.c_cc[VMIN] = 0;                // read() returns immediately...
    raw.c_cc[VTIME] = 0;               // ...with whatever is available
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static int get_terminal_size(int *w, int *h) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0 || ws.ws_row == 0)
        return -1;
    *w = ws.ws_col;
    *h = ws.ws_row;
    return 0;
}

static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

// Ask the terminal whether it speaks the kitty keyboard protocol.
// We send "query flags" followed by "primary device attributes". Every terminal
// answers the second; only supporting ones answer the first, and always before it.
static bool detect_kitty(void) {
    write_str("\x1b[?u\x1b[c");

    unsigned char buf[128];
    size_t len = 0;
    bool supported = false;
    double deadline = now_s() + 0.25;

    while (len < sizeof buf) {
        double left = deadline - now_s();
        if (left <= 0) break;

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        struct timeval tv = { 0, (suseconds_t)(left * 1e6) };
        if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) <= 0) break;

        ssize_t n = read(STDIN_FILENO, buf + len, sizeof buf - len);
        if (n <= 0) break;
        len += (size_t)n;

        bool done = false;
        for (size_t i = 0; i + 2 < len; i++) {
            if (buf[i] == 27 && buf[i + 1] == '[' && buf[i + 2] == '?') {
                size_t j = i + 3;
                while (j < len && ((buf[j] >= '0' && buf[j] <= '9') || buf[j] == ';')) j++;
                if (j < len) {
                    if (buf[j] == 'u') supported = true;
                    else if (buf[j] == 'c') done = true;
                }
            }
        }
        if (done) break;
    }
    return supported;
}

// ---------- input ----------
enum { KEY_UP = 1000, KEY_DOWN, KEY_RIGHT, KEY_LEFT };

typedef struct {
    bool   kitty;          // true: real press/release events available

    // precise mode
    bool   left, right;    // currently held
    bool   jump_held;
    bool   jump_cut;       // jump released this tick (for variable jump height)

    // fallback mode, index 0 = left, 1 = right
    double last[2];        // when each direction was last seen
    bool   repeating[2];   // has the OS key-repeat kicked in for it

    double jump_time;      // last jump press (for buffering)
} Input;

static void input_init(Input *in, bool kitty) {
    memset(in, 0, sizeof *in);
    in->kitty = kitty;
    in->last[0] = in->last[1] = -1e9;
    in->jump_time = -1e9;
}

static bool is_left(int k)  { return k == 'a' || k == KEY_LEFT; }
static bool is_right(int k) { return k == 'd' || k == KEY_RIGHT; }
static bool is_jump(int k)  { return k == 'w' || k == ' ' || k == KEY_UP; }

// event: 1 = press, 2 = repeat, 3 = release (fallback mode always passes 1)
static void handle_key(Input *in, int key, int mods, int event, double now) {
    if (event == 3) {                       // release: only exists in precise mode
        if (is_left(key))  in->left = false;
        else if (is_right(key)) in->right = false;
        else if (is_jump(key)) { in->jump_held = false; in->jump_cut = true; }
        return;
    }

    if (key == 'c' && (((mods - 1) & 4) != 0)) { running = 0; return; }   // Ctrl+C
    if (key == 'q') { if (event == 1) running = 0; return; }

    if (is_left(key) || is_right(key)) {
        if (in->kitty) {
            if (is_left(key)) in->left = true; else in->right = true;
        } else {
            int d = is_right(key) ? 1 : 0;
            in->repeating[d] = (now - in->last[d] < REPEAT_DETECT);
            in->last[d] = now;
            in->last[1 - d] = -1e9;         // opposite direction cancels
            in->repeating[1 - d] = false;
        }
        return;
    }

    if (is_jump(key)) {
        if (event == 1) { in->jump_time = now; in->jump_held = true; }
    }

    // Fallback: the terminal stops repeating a held direction as soon as another key
    // is pressed, so give any repeating direction a fresh grace period instead of
    // letting it die after HOLD_REPEAT.
    if (!in->kitty) {
        for (int d = 0; d < 2; d++) {
            if (in->repeating[d] && now - in->last[d] < HOLD_REPEAT) {
                in->repeating[d] = false;
                in->last[d] = now;
            }
        }
    }
}

// Parse the body of a CSI sequence: parameters + final byte (s[len-1]).
static void parse_csi(Input *in, const unsigned char *s, size_t len, double now) {
    if (len == 0) return;
    if (s[0] == '<' || s[0] == '=' || s[0] == '>' || s[0] == '?') return;   // replies, not keys

    int v[4][3];
    for (int i = 0; i < 4; i++) for (int j = 0; j < 3; j++) v[i][j] = -1;
    int fi = 0, si = 0;
    for (size_t i = 0; i + 1 < len; i++) {
        unsigned char c = s[i];
        if (c >= '0' && c <= '9') {
            if (fi < 4 && si < 3) {
                if (v[fi][si] < 0) v[fi][si] = 0;
                v[fi][si] = v[fi][si] * 10 + (c - '0');
            }
        } else if (c == ';') { fi++; si = 0; }
        else if (c == ':')   { si++; }
    }

    int mods  = v[1][0] > 0 ? v[1][0] : 1;
    int event = v[1][1] > 0 ? v[1][1] : 1;
    int key = 0;

    switch (s[len - 1]) {
        case 'u': key = v[0][0]; break;
        case 'A': key = KEY_UP;    break;
        case 'B': key = KEY_DOWN;  break;
        case 'C': key = KEY_RIGHT; break;
        case 'D': key = KEY_LEFT;  break;
        default: return;
    }
    if (key > 0) handle_key(in, key, mods, event, now);
}

// Drain everything waiting on stdin. Sequences can be split across reads, so
// an incomplete tail is kept for the next call.
static void poll_input(Input *in, double now) {
    static unsigned char ibuf[512];
    static size_t ilen = 0;

    ssize_t n;
    while (ilen < sizeof ibuf && (n = read(STDIN_FILENO, ibuf + ilen, sizeof ibuf - ilen)) > 0) {
        ilen += (size_t)n;

        size_t i = 0;
        while (i < ilen) {
            if (ibuf[i] == 27) {
                if (i + 1 >= ilen) break;                       // lone ESC: wait for more
                if (ibuf[i + 1] == '[') {
                    size_t j = i + 2;
                    while (j < ilen && !(ibuf[j] >= 0x40 && ibuf[j] <= 0x7E)) j++;
                    if (j >= ilen) {                            // incomplete sequence
                        if (ilen - i > 40) i = ilen;            // garbage: drop it
                        break;
                    }
                    parse_csi(in, ibuf + i + 2, j - (i + 2) + 1, now);
                    i = j + 1;
                } else if (ibuf[i + 1] == 'O') {                // SS3 arrows (application mode)
                    if (i + 2 >= ilen) break;
                    switch (ibuf[i + 2]) {
                        case 'A': handle_key(in, KEY_UP,    1, 1, now); break;
                        case 'C': handle_key(in, KEY_RIGHT, 1, 1, now); break;
                        case 'D': handle_key(in, KEY_LEFT,  1, 1, now); break;
                    }
                    i += 3;
                } else {
                    i++;                                        // stray ESC
                }
            } else {
                handle_key(in, tolower(ibuf[i]), 1, 1, now);
                i++;
            }
        }
        if (i < ilen) memmove(ibuf, ibuf + i, ilen - i);
        ilen -= i;
    }
}

static bool legacy_held(const Input *in, int d, double now) {
    double window = in->repeating[d] ? HOLD_REPEAT : HOLD_FIRST;
    return now - in->last[d] < window;
}

static float move_axis(const Input *in, double now) {
    if (in->kitty) return (in->right ? 1.0f : 0.0f) - (in->left ? 1.0f : 0.0f);
    return (legacy_held(in, 1, now) ? 1.0f : 0.0f) - (legacy_held(in, 0, now) ? 1.0f : 0.0f);
}

// ---------- game ----------
typedef struct {
    float x, y;
    int width, height; // Width will be from x leftwards and height from y upwards
    float vx, vy;
    bool is_grounded;
    bool facing_right;
} Player;

typedef struct { float left, right, top, bottom; } Rect;

static Rect player_rect(const Player *p) {    
    Rect r;    
    r.left   = p->x - p->width  + 1;    
    r.right  = p->x + 1;            
    r.top    = p->y - p->height + 1;    
    r.bottom = p->y + 1;            
    return r;
}

typedef struct { int x, y; } Camera;

static Camera camera_follow(const Player *p, int screen_w, int screen_h) {    
    Camera c;    
    c.x = (int)floorf(p->x) - (screen_w + p->width) / 2;   
    c.y = (int)floorf(p->y) - (screen_h * 2) / 3;          
    return c;
}

static float approach(float v, float target, float max_delta) {
    if (v < target) { v += max_delta; if (v > target) v = target; }
    else if (v > target) { v -= max_delta; if (v < target) v = target; }
    return v;
}

static bool overlaps(Rect a, const Platform *b) {    
    return a.left < b->x + b->w && a.right > b->x &&           
           a.top  < b->y + b->h && a.bottom > b->y;       
}

static void update(Player *p, Input *in, double now, float dt) {    
    float axis = move_axis(in, now);    
    if (axis != 0.0f) p->facing_right = axis > 0.0f;
    float rate = (axis != 0.0f) ? ACCEL : FRICTION;    
    p->vx = approach(p->vx, axis * MOVE_SPEED, rate * dt);    
    
    if (now - in->jump_time < JUMP_BUFFER && p->is_grounded) {        
        p->vy = -JUMP_SPEED;        
        in->jump_time = -1e9;    
    }    
    if (in->jump_cut) {        
        if (p->vy < 0) p->vy *= JUMP_CUT;        
        in->jump_cut = false;    
    }    
    p->vy += GRAVITY * dt;    
    
    // X axis collision
    p->x += p->vx * dt;    
    Rect r = player_rect(p);    
    for (int i = 0; i < get_platform_count(); i++) {        
        const Platform *pl = &get_platforms()[i];        
        if (!overlaps(r, pl)) continue;        
        if (p->vx > 0)      p->x = pl->x - 1;                         
        else if (p->vx < 0) p->x = pl->x + pl->w + p->width - 1;      
        p->vx = 0;        
        r = player_rect(p);    
    }    
    
    // Y axis collision
    p->is_grounded = false;    
    p->y += p->vy * dt;    
    r = player_rect(p);    
    for (int i = 0; i < get_platform_count(); i++) {        
        const Platform *pl = &get_platforms()[i];        
        if (!overlaps(r, pl)) continue;        
        if (p->vy > 0) {                                              
            p->y = pl->y - 1;            
            p->is_grounded = true;        
        } else if (p->vy < 0) {                                       
            p->y = pl->y + pl->h + p->height - 1;        
        }        
        p->vy = 0;        
        r = player_rect(p);    
    }    
    
    // Respawn if fallen off
    if (p->y > RESPAWN_DEPTH) {        
        p->x = SPAWN_X; p->y = SPAWN_Y;        
        p->vx = 0;      p->vy = 0;    
    }
}

static void draw_text(Renderer *r, int x, int y, const char *s,
                      unsigned char cr, unsigned char cg, unsigned char cb) {
    for (; *s; s++, x++) draw_pixel(r, x, y, *s, cr, cg, cb);
}

static void draw_sprite_region(Renderer *rend, Camera cam, const Sprite *sprite,
                               int source_x, int source_y, int source_w, int source_h,
                               int wx, int wy, int target_w, int target_h, bool flip_x) {
    if (!sprite || source_w <= 0 || source_h <= 0 || target_w <= 0 || target_h <= 0 ||
        source_x < 0 || source_y < 0 || source_x + source_w > sprite->width ||
        source_y + source_h > sprite->height) return;

    for (int y = 0; y < target_h; y++) {
        int sample_y = source_y + (int)((long long)y * source_h / target_h);
        png_bytep row = sprite->row_pointers[sample_y];

        for (int x = 0; x < target_w; x++) {
            int frame_x = (int)((long long)x * source_w / target_w);
            int sample_x = source_x + (flip_x ? source_w - 1 - frame_x : frame_x);
            png_bytep px = &(row[sample_x * 4]);

            // Skip fully transparent pixels
            if (px[3] == 0) continue;

            // Map pixel brightness to a terminal character using your brightness formula from drawer.c[cite: 6]
            unsigned int brightness = (299u * px[0] + 587u * px[1] + 114u * px[2]) / 1000u;
            size_t index = brightness * (sizeof(SHADE_RAMP) - 2) / 255;
            char ch = SHADE_RAMP[index];

            // Convert world coordinates to screen coordinates using the camera
            int screen_x = (wx + x) - cam.x;
            int screen_y = (wy + y) - cam.y;

            draw_pixel(rend, screen_x, screen_y, ch, px[0], px[1], px[2]);
        }
    }
}

static void draw_sprite(Renderer *rend, Camera cam, const Sprite *sprite,
                        int wx, int wy, int target_w, int target_h) {
    if (sprite)
        draw_sprite_region(rend, cam, sprite, 0, 0, sprite->width, sprite->height,
                           wx, wy, target_w, target_h, false);
}

static void fill_world_rect(Renderer *rend, Camera cam, int wx, int wy, int w, int h,                            
                            char ch, unsigned char cr, unsigned char cg, unsigned char cb) {    
    int x0 = wx - cam.x, y0 = wy - cam.y;    
    int x1 = x0 + w,     y1 = y0 + h;           
    if (x0 < 0) x0 = 0;    
    if (y0 < 0) y0 = 0;    
    if (x1 > rend->width)  x1 = rend->width;    
    if (y1 > rend->height) y1 = rend->height;    
    for (int y = y0; y < y1; y++)        
        for (int x = x0; x < x1; x++)            
            draw_pixel(rend, x, y, ch, cr, cg, cb);
}

static void render(Renderer *r, const Player *p, const Input *in, double now) {
    clear_back_buffer(r, ' ', 0, 0, 0);
    Camera cam = camera_follow(p, r->width, r->height);

    // Scale the complete platform texture into each platform.
    for (int i = 0; i < get_platform_count(); i++) {
        const Platform *pl = &get_platforms()[i];
        if (platform_sprite) {
            draw_sprite(r, cam, platform_sprite, pl->x, pl->y, pl->w, pl->h);
        } else {
            // Fallback if texture isn't loaded yet
            fill_world_rect(r, cam, pl->x, pl->y, pl->w, pl->h, '#', 150, 100, 50);
        }
    }

    // Render player matching dynamic hitbox scaling
    int fx = (int)floorf(p->x);
    int fy = (int)floorf(p->y);
    int px_draw_x = fx - p->width + 1;
    int py_draw_y = fy - p->height + 1;

    const Sprite *sprite = NULL;
    int source_x = 0, source_y = 0, source_w = 0, source_h = 0;
    if (fabsf(p->vx) > 2.0f && player_walk_sprite) {
        int walk_frame_count = player_walk_intro_sprite ? 26 : 24;
        int frame = (int)(now * 12.0) % walk_frame_count;
        if (player_walk_intro_sprite && frame < 2) {
            sprite = player_walk_intro_sprite;
            source_x = frame * 45;
            source_w = 45;
            source_h = 58;
        } else {
            if (player_walk_intro_sprite) frame -= 2;
            sprite = player_walk_sprite;
            source_x = (frame % 4) * 45;
            source_y = (frame / 4) * 58;
            source_w = 45;
            source_h = 58;
        }
    } else if (player_idle_sprite) {
        int frame = (int)(now * 5.0) % 10;
        sprite = player_idle_sprite;
        source_x = frame * 46;
        source_w = 46;
        source_h = 55;
    }

    if (sprite) {
        draw_sprite_region(r, cam, sprite, source_x, source_y, source_w, source_h,
                           px_draw_x, py_draw_y, p->width, p->height, p->facing_right);
    } else {
        // Fallback if texture isn't loaded yet
        fill_world_rect(r, cam, px_draw_x, py_draw_y, p->width, p->height, '@', 255, 0, 0);
    }

    draw_text(r, 0, 0, in->kitty ? "input: precise (key release events)"
                                 : "input: fallback (timeout-based)", 120, 120, 120);
    present_renderer(r);
}

int main(void) {
    int width, height;
    if (get_terminal_size(&width, &height) == -1) {
        perror("ioctl");
        return 1;
    }

    enable_raw_mode();

    struct sigaction sa = {0};
    sa.sa_handler = on_quit_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sa.sa_handler = on_winch;
    sigaction(SIGWINCH, &sa, NULL);

    Renderer *renderer = create_renderer(width, height);
    if (!renderer) return 1;

    platform_sprite = load_sprite("platform.png");
    player_idle_sprite = load_sprite("sample(idle&walk)/idle/sprite sheets/idle.png");
    player_walk_sprite = load_sprite("sample(idle&walk)/walk/sprite sheets/walk.png");
    player_walk_intro_sprite = load_sprite("sample(idle&walk)/walk/sprite sheets/from idle.png");

    init_map(width, height, 1.0f);

    // The alternate screen keeps its own keyboard-mode stack, so enable this
    // after create_renderer() switched to it, and disable it before leaving.
    bool kitty = detect_kitty();
    if (kitty) {
        // flags: 1 disambiguate | 2 report press/repeat/release | 8 report all keys as escape codes
        write_str("\x1b[>11u");
        kitty_on = true;
    }

    Player player = { 
        .x = SPAWN_X, 
        .y = SPAWN_Y, 
        .width = max(3, width * 0.1f), 
        .height = max(5, height * 0.30f),   
        .vx = 0.0f, 
        .vy = 0.0f, 
        .is_grounded = false,
        .facing_right = false
    };
    Input input;
    input_init(&input, kitty);

    const long tick_ns = 1000000000L / TICK_HZ;
    const float dt = 1.0f / TICK_HZ;

    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);

    while (running) {
        if (resized) {
            resized = 0;
            if (get_terminal_size(&width, &height) == 0)
                resize_renderer(renderer, width, height);
        }

        double now = now_s();
        poll_input(&input, now);
        update(&player, &input, now, dt);
        render(renderer, &player, &input, now);

        // Sleep until an absolute deadline: no drift, frame work time is accounted for.
        next.tv_nsec += tick_ns;
        if (next.tv_nsec >= 1000000000L) { next.tv_nsec -= 1000000000L; next.tv_sec++; }

        struct timespec cur;
        clock_gettime(CLOCK_MONOTONIC, &cur);
        double behind = (double)(cur.tv_sec - next.tv_sec) + (double)(cur.tv_nsec - next.tv_nsec) / 1e9;
        if (behind > 0.1) next = cur;   // fell far behind (e.g. suspended): resync
        else clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }

    if (kitty_on) { write_str("\x1b[<u"); kitty_on = false; }
    free_sprite(platform_sprite);
    free_sprite(player_idle_sprite);
    free_sprite(player_walk_sprite);
    free_sprite(player_walk_intro_sprite);
    free_renderer(renderer);
    return 0;
}