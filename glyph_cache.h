// glyph_cache.h - Glyph caching and rendering

#ifndef GLYPH_CACHE_H
#define GLYPH_CACHE_H

#include "stb_truetype.h"
#include <SDL2/SDL.h>
#include <stdint.h>

typedef struct {
  uint8_t *base_bitmap;
  uint8_t *boiled_bitmap;
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

// Check if character has a descender
int has_descender(int ascii);

// Load a glyph into the cache
void load_glyph(GlyphCache *cache, SDL_Renderer *renderer, int ascii);

// Get baseline height for text
int get_baseline_height(GlyphCache *cache, const char *text);

// Render text directly to SDL renderer (original method)
void render_text(SDL_Renderer *renderer, GlyphCache *cache, const char *text,
                 int x, int y, float time);

// Cleanup glyph cache
void cleanup_glyph_cache(GlyphCache *cache);

#endif // GLYPH_CACHE_H
