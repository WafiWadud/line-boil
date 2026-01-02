// frame_generator.h - Background frame generation

#ifndef FRAME_GENERATOR_H
#define FRAME_GENERATOR_H

#include "glyph_cache.h"
#include <pthread.h>
#include <stdint.h>

// Frame generation constants
extern const int FPS;
extern const float frame_dt;
extern const int PREG;

// Window dimensions
extern int WIN_W;
extern int WIN_H;

// Text lines for rendering
extern const char *g_lines[];
extern int g_line_count;
extern int g_line_gap;

// Background thread control
extern int bg_keep_running;
extern pthread_mutex_t bg_lock;

// Frame buffers (raw pixels from background thread)
extern uint32_t **framesB_pixels;
extern int framesB_pixels_size;
extern int framesB_pixels_cap;

// Global cache pointer for background thread
extern GlyphCache *g_bg_cache;

// Render a full frame into pixel buffer
void render_frame_to_pixels(uint32_t *pixels, GlyphCache *cache, float t);

// Background thread function
void *background_generator(void *arg);

#endif // FRAME_GENERATOR_H
