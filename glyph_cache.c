// glyph_cache.c - Glyph caching implementation

#include "glyph_cache.h"
#include "voronoi.h"
#include <stdlib.h>

int has_descender(int ascii) {
  if (ascii == 'g' || ascii == 'j' || ascii == 'p' || ascii == 'q' ||
      ascii == 'y')
    return 1;
  if (ascii == ',' || ascii == ';')
    return 1;
  return 0;
}

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

  int gw = w, gh = h;
  uint8_t *glyph = stbtt_GetCodepointBitmap(
      &cache->font, cache->scale, cache->scale, ascii, &gw, &gh, 0, 0);

  cache->glyphs[ascii].width = gw;
  cache->glyphs[ascii].height = gh;
  cache->glyphs[ascii].base_bitmap = glyph;
  cache->glyphs[ascii].boiled_bitmap = malloc(gw * gh);
  cache->glyphs[ascii].texture = SDL_CreateTexture(
      renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, gw, gh);
  SDL_SetTextureBlendMode(cache->glyphs[ascii].texture, SDL_BLENDMODE_BLEND);

  cache->glyphs[ascii].loaded = 1;
}

int get_baseline_height(GlyphCache *cache, const char *text) {
  int maxh = 0;
  for (int i = 0; text[i]; i++) {
    int c = (unsigned char)text[i];
    if (cache->glyphs[c].loaded && !has_descender(c))
      if (cache->glyphs[c].height > maxh)
        maxh = cache->glyphs[c].height;
  }
  return maxh;
}

void render_text(SDL_Renderer *renderer, GlyphCache *cache, const char *text,
                 int x, int y, float time) {
  int cursor = x;
  int baseline = get_baseline_height(cache, text);

  const float STRENGTH = 4.0f;
  const float FREQ = 0.04f;
  float offset = 0.0f;

  for (int i = 0; text[i]; i++) {
    int c = (unsigned char)text[i];
    GlyphData *g = &cache->glyphs[c];

    if (!g->loaded || g->width == 0 || g->height == 0) {
      cursor += 20;
      offset += 0.5f;
      continue;
    }

    float t = (time + offset) * 0.3f;
    boil_frame(g->boiled_bitmap, g->base_bitmap, g->width, g->height, t,
               STRENGTH, FREQ);

    uint32_t *pixels;
    int pitch;
    SDL_LockTexture(g->texture, NULL, (void **)&pixels, &pitch);
    for (int yy = 0; yy < g->height; yy++) {
      for (int xx = 0; xx < g->width; xx++) {
        uint8_t a = g->boiled_bitmap[yy * g->width + xx];
        pixels[yy * (pitch / 4) + xx] = 0xFFFFFF00 | (uint32_t)a;
      }
    }
    SDL_UnlockTexture(g->texture);

    int yoff;
    if (has_descender(c))
      yoff = y + (baseline - g->height) + 13;
    else if (c == '\'' || c == '"')
      yoff = y;
    else
      yoff = y + (baseline - g->height);

    SDL_Rect dst = {cursor, yoff, g->width, g->height};
    SDL_RenderCopy(renderer, g->texture, NULL, &dst);

    cursor += g->width;
    offset += 0.5f;
  }
}

void cleanup_glyph_cache(GlyphCache *cache) {
  for (int i = 0; i < 128; i++) {
    if (cache->glyphs[i].loaded) {
      if (cache->glyphs[i].base_bitmap)
        stbtt_FreeBitmap(cache->glyphs[i].base_bitmap, NULL);
      if (cache->glyphs[i].boiled_bitmap)
        free(cache->glyphs[i].boiled_bitmap);
      if (cache->glyphs[i].texture)
        SDL_DestroyTexture(cache->glyphs[i].texture);
    }
  }
}
