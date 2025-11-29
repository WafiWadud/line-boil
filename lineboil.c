// lineboil.c - Real-time animated text renderer with boiling effect

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <SDL2/SDL.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// ----------------------
// Voronoi noise
// ----------------------
static inline uint32_t hash2d(int x, int y) {
  uint32_t h = x * 374761393u + y * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}

static float voronoi(float x, float y, float t) {
  float px = x + sinf(t * 2.1f) * 0.3f;
  float py = y + cosf(t * 1.7f) * 0.3f;

  int xi = (int)floorf(px);
  int yi = (int)floorf(py);

  float minDist = 1e9f;
  for (int yy = -1; yy <= 1; yy++) {
    for (int xx = -1; xx <= 1; xx++) {
      uint32_t h = hash2d(xi + xx, yi + yy);
      float fx = (float)(h & 0xFF) / 255.0f;
      float fy = (float)((h >> 8) & 0xFF) / 255.0f;
      float cx = xi + xx + fx;
      float cy = yi + yy + fy;
      float dx = cx - px;
      float dy = cy - py;
      float d = dx * dx + dy * dy;
      if (d < minDist)
        minDist = d;
    }
  }
  return sqrtf(minDist);
}

static void boil_frame(uint8_t *dst, uint8_t *src, int w, int h, float t,
                       float strength, float freq) {
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      float nx = x + voronoi(x * freq, y * freq, t) * strength;
      float ny = y + voronoi(y * freq, x * freq, t * 1.37f) * strength;

      int ix = (int)nx;
      int iy = (int)ny;
      uint8_t sample = 0;
      if (ix >= 0 && iy >= 0 && ix < w && iy < h)
        sample = src[iy * w + ix];
      dst[y * w + x] = sample;
    }
  }
}

// ----------------------
// Glyph cache structure
// ----------------------
typedef struct {
  uint8_t *base_bitmap;   // Original glyph bitmap
  uint8_t *boiled_bitmap; // Working buffer for boiled frame
  SDL_Texture *texture;
  int width;
  int height;
  int loaded;
} GlyphData;

typedef struct {
  GlyphData glyphs[128];
  stbtt_fontinfo font;
  float scale;
} GlyphCache;

// Check if a character has a descender
int has_descender(int ascii) {
  if (ascii == 'g' || ascii == 'j' || ascii == 'p' || ascii == 'q' ||
      ascii == 'y') {
    return 1;
  }
  if (ascii == ',' || ascii == ';') {
    return 1;
  }
  return 0;
}

// Load a glyph bitmap
void load_glyph(GlyphCache *cache, SDL_Renderer *renderer, int ascii) {
  if (ascii < 0 || ascii >= 128 || cache->glyphs[ascii].loaded)
    return;

  int x0, y0, x1, y1;
  stbtt_GetCodepointBitmapBox(&cache->font, ascii, cache->scale, cache->scale,
                              &x0, &y0, &x1, &y1);
  int w = x1 - x0;
  int h = y1 - y0;

  if (w <= 0 || h <= 0) {
    cache->glyphs[ascii].loaded = 1;
    return;
  }

  int gw_out = w, gh_out = h;
  uint8_t *glyph = stbtt_GetCodepointBitmap(
      &cache->font, cache->scale, cache->scale, ascii, &gw_out, &gh_out, 0, 0);
  if (!glyph)
    return;

  cache->glyphs[ascii].width = gw_out;
  cache->glyphs[ascii].height = gh_out;
  cache->glyphs[ascii].base_bitmap = glyph;
  cache->glyphs[ascii].boiled_bitmap = malloc(gw_out * gh_out);
  cache->glyphs[ascii].texture =
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                        SDL_TEXTUREACCESS_STREAMING, gw_out, gh_out);
  SDL_SetTextureBlendMode(cache->glyphs[ascii].texture, SDL_BLENDMODE_BLEND);
  cache->glyphs[ascii].loaded = 1;
}

// Find the tallest glyph height for non-descender characters
int get_baseline_height(GlyphCache *cache, const char *text) {
  int max_height = 0;
  for (int i = 0; text[i]; i++) {
    int ascii = (unsigned char)text[i];
    if (cache->glyphs[ascii].loaded && !has_descender(ascii)) {
      if (cache->glyphs[ascii].height > max_height) {
        max_height = cache->glyphs[ascii].height;
      }
    }
  }
  return max_height;
}

void render_text(SDL_Renderer *renderer, GlyphCache *cache, const char *text,
                 int x, int y, float time) {
  int cursor_x = x;
  int baseline_height = get_baseline_height(cache, text);

  const float STRENGTH = 4.0f;
  const float FREQ = 0.04f;

  // Add unique offset per character to make them boil independently
  float char_time_offset = 0.0f;

  for (int i = 0; text[i]; i++) {
    int ascii = (unsigned char)text[i];

    if (!cache->glyphs[ascii].loaded) {
      cursor_x += 20;
      char_time_offset += 0.5f;
      continue;
    }

    GlyphData *glyph = &cache->glyphs[ascii];
    if (glyph->width == 0 || glyph->height == 0) {
      cursor_x += 20;
      char_time_offset += 0.5f;
      continue;
    }

    // Generate boiled frame with unique time offset per character
    float t = (time + char_time_offset) * 0.3f;
    boil_frame(glyph->boiled_bitmap, glyph->base_bitmap, glyph->width,
               glyph->height, t, STRENGTH, FREQ);

    // Convert to RGBA and update texture
    uint32_t *pixels;
    int pitch;
    SDL_LockTexture(glyph->texture, NULL, (void **)&pixels, &pitch);

    for (int py = 0; py < glyph->height; py++) {
      for (int px = 0; px < glyph->width; px++) {
        uint8_t a = glyph->boiled_bitmap[py * glyph->width + px];
        // RGBA format: 0xAABBGGRR (little endian)
        pixels[py * (pitch / 4) + px] = 0xFFFFFF00 | a; // White RGB with alpha
      }
    }

    SDL_UnlockTexture(glyph->texture);

    // Calculate y offset
    int y_offset;
    if (has_descender(ascii)) {
      y_offset = y + (baseline_height - glyph->height) + 13;
    } else if (ascii == '\'' || ascii == '\"') {
      y_offset = y;
    } else {
      y_offset = y + (baseline_height - glyph->height);
    }

    SDL_Rect dest = {cursor_x, y_offset, glyph->width, glyph->height};
    SDL_RenderCopy(renderer, glyph->texture, NULL, &dest);

    cursor_x += glyph->width;
    char_time_offset += 0.5f; // Increment time offset for next character
  }
}

int main(int argc, char *argv[]) {
  const char *fontfile = "font.otf";
  if (argc >= 2)
    fontfile = argv[1];

  // Load font
  FILE *fp = fopen(fontfile, "rb");
  if (!fp) {
    fprintf(stderr, "Failed to open font file '%s'\n", fontfile);
    return 1;
  }
  fseek(fp, 0, SEEK_END);
  long fsize = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  uint8_t *ttf_data = (uint8_t *)malloc(fsize);
  fread(ttf_data, 1, fsize, fp);
  fclose(fp);

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("SDL init failed: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window *window =
      SDL_CreateWindow("Line-Boil Real-time", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1600, 500, SDL_WINDOW_SHOWN);

  SDL_Renderer *renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

  // Initialize glyph cache
  GlyphCache cache = {0};
  if (!stbtt_InitFont(&cache.font, ttf_data,
                      stbtt_GetFontOffsetForIndex(ttf_data, 0))) {
    fprintf(stderr, "stbtt_InitFont failed\n");
    return 1;
  }
  cache.scale = stbtt_ScaleForPixelHeight(&cache.font, 64.0f);

  const char *lines[] = {
      "SPHINX OF BLACK QUARTZ, JUDGE MY VOW!",
      "sphinx of black quartz, judge my vow!",
      "0123456789",
      "\"Hello!\" he said.",
      "We need: eggs, spam, ham, etc.",
      "Some of them are going with us: Tiffin and co; Tyler, among others.",
      "\'Who are you?\' he asked.",
      "What!",
      "I went to Arby's recently (really?)"};

  int line_count = sizeof(lines) / sizeof(lines[0]);
  int line_gap = 30;

  // Pre-load all glyphs used in the text
  for (int i = 0; i < line_count; i++) {
    for (int j = 0; lines[i][j]; j++) {
      load_glyph(&cache, renderer, (unsigned char)lines[i][j]);
    }
  }

  int running = 1;
  SDL_Event event;
  Uint32 start_time = SDL_GetTicks();

  while (running) {
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT)
        running = 0;
    }

    float time = (SDL_GetTicks() - start_time) / 1000.0f;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    int y_offset = 0;
    for (int i = 0; i < line_count; i++) {
      render_text(renderer, &cache, lines[i], 0, y_offset, time);
      y_offset += 24 + line_gap;
    }

    SDL_RenderPresent(renderer);
  }

  // Cleanup
  for (int i = 0; i < 128; i++) {
    if (cache.glyphs[i].loaded) {
      if (cache.glyphs[i].base_bitmap)
        stbtt_FreeBitmap(cache.glyphs[i].base_bitmap, NULL);
      if (cache.glyphs[i].boiled_bitmap)
        free(cache.glyphs[i].boiled_bitmap);
      if (cache.glyphs[i].texture)
        SDL_DestroyTexture(cache.glyphs[i].texture);
    }
  }

  free(ttf_data);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
