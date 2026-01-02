// frame_generator.c - Background frame generation implementation

#include "frame_generator.h"
#include "voronoi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Constants
const int FPS = 12;
const float frame_dt = 1.0f / 12.0f;
const int PREG = 144;

// Window dimensions
int WIN_W = 1600;
int WIN_H = 500;

// Text lines
const char *g_lines[] = {
    "SPHINX OF BLACK QUARTZ, JUDGE MY VOW!",
    "sphinx of black quartz, judge my vow!",
    "0123456789",
    "\"Hello!\" he said.",
    "We need: eggs, spam, ham, etc.",
    "Some of them are going with us: Tiffin and co; Tyler, among others.",
    "\'Who are you?\' he asked.",
    "What!",
    "I went to Arby's recently (really?)"};

int g_line_count = sizeof(g_lines) / sizeof(g_lines[0]);
int g_line_gap = 30;

// Background thread state
uint32_t **framesB_pixels = NULL;
int framesB_pixels_size = 0;
int framesB_pixels_cap = 0;
int bg_keep_running = 1;
pthread_mutex_t bg_lock = PTHREAD_MUTEX_INITIALIZER;
GlyphCache *g_bg_cache = NULL;

static void blit_glyph_to_pixels(uint32_t *dest, int dest_w, int dest_h,
                                 uint8_t *boiled, int gw, int gh, int dst_x,
                                 int dst_y) {
  for (int yy = 0; yy < gh; yy++) {
    int dy = dst_y + yy;
    if (dy < 0 || dy >= dest_h)
      continue;
    for (int xx = 0; xx < gw; xx++) {
      int dx = dst_x + xx;
      if (dx < 0 || dx >= dest_w)
        continue;
      uint8_t a = boiled[yy * gw + xx];
      if (a == 0)
        continue;
      dest[dy * dest_w + dx] = 0xFFFFFF00u | (uint32_t)a;
    }
  }
}

void render_frame_to_pixels(uint32_t *pixels, GlyphCache *cache, float t) {
  memset(pixels, 0, WIN_W * WIN_H * sizeof(uint32_t));

  int line_y = 0;
  const float STRENGTH = 4.0f;
  const float FREQ = 0.04f;

  for (int li = 0; li < g_line_count; li++) {
    const char *text = g_lines[li];
    int baseline = get_baseline_height(cache, text);
    int cursor = 0;
    float offset = 0.0f;

    for (int i = 0; text[i]; i++) {
      int c = (unsigned char)text[i];
      GlyphData *g = &cache->glyphs[c];

      if (!g->loaded || g->width == 0 || g->height == 0) {
        cursor += 20;
        offset += 0.5f;
        continue;
      }

      float ft = (t + offset) * 0.3f;

      int gw = g->width;
      int gh = g->height;
      uint8_t *tmp = (uint8_t *)malloc(gw * gh);
      if (!tmp) {
        cursor += g->width;
        offset += 0.5f;
        continue;
      }
      boil_frame(tmp, g->base_bitmap, gw, gh, ft, STRENGTH, FREQ);

      int yoff;
      if (has_descender(c))
        yoff = line_y + (baseline - g->height) + 13;
      else if (c == '\'' || c == '"')
        yoff = line_y;
      else
        yoff = line_y + (baseline - g->height);

      blit_glyph_to_pixels(pixels, WIN_W, WIN_H, tmp, gw, gh, cursor, yoff);
      free(tmp);

      cursor += g->width;
      offset += 0.5f;
    }

    line_y += 24 + g_line_gap;
  }
}

void *background_generator(void *arg) {
  int idx = (int)(intptr_t)arg;
  GlyphCache *cache = g_bg_cache;
  if (!cache)
    return NULL;

  while (bg_keep_running) {
    float t = frame_dt * idx++;

    uint32_t *pixels = (uint32_t *)malloc(WIN_W * WIN_H * sizeof(uint32_t));
    if (!pixels) {
      usleep(5000);
      continue;
    }

    render_frame_to_pixels(pixels, cache, t);

    pthread_mutex_lock(&bg_lock);
    if (framesB_pixels_size == framesB_pixels_cap) {
      framesB_pixels_cap = framesB_pixels_cap ? framesB_pixels_cap * 2 : 512;
      framesB_pixels = (uint32_t **)realloc(
          framesB_pixels, framesB_pixels_cap * sizeof(uint32_t *));
    }
    framesB_pixels[framesB_pixels_size++] = pixels;
    pthread_mutex_unlock(&bg_lock);
    printf("Frame generated\n");

    usleep(1000);
  }

  return NULL;
}
