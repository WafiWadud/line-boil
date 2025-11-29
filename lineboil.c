// lineboil.c - Real-time animated text renderer with boiling effect

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <SDL2/SDL.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
// Glyph cache
// ----------------------
typedef struct {
  uint8_t *base_bitmap;   // original glyph bitmap
  uint8_t *boiled_bitmap; // not used by background generator (kept for original
                          // rendering)
  SDL_Texture *texture;   // glyph texture used for on-render direct drawing
  int width;
  int height;
  int loaded;
} GlyphData;

typedef struct {
  GlyphData glyphs[128];
  stbtt_fontinfo font;
  float scale;
} GlyphCache;

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

// This is the original renderer that draws directly to SDL_Renderer (keeps
// working)
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

// ----------------------------------------------------
// FRAME TIMING + BACKGROUND (thread safe raw-pixel generation)
// ----------------------------------------------------

static const int FPS = 12;
static const float frame_dt = 1.0f / 12.0f;
static const int PREG = 72; // pre-generate count

// Window/frame dimensions (match main window)
static int WIN_W = 1600;
static int WIN_H = 500;

// framesB raw pixel buffers (produced by background thread)
// protected by mutex
static uint32_t **framesB_pixels = NULL;
static int framesB_pixels_size = 0;
static int framesB_pixels_cap = 0;

static int bg_keep_running = 1;
static pthread_mutex_t bg_lock = PTHREAD_MUTEX_INITIALIZER;

// Text lines (declare globally to be used by background generator)
static const char *g_lines[] = {
    "SPHINX OF BLACK QUARTZ, JUDGE MY VOW!",
    "sphinx of black quartz, judge my vow!",
    "0123456789",
    "\"Hello!\" he said.",
    "We need: eggs, spam, ham, etc.",
    "Some of them are going with us: Tiffin and co; Tyler, among others.",
    "\'Who are you?\' he asked.",
    "What!",
    "I went to Arby's recently (really?)"};

static int g_line_count = sizeof(g_lines) / sizeof(g_lines[0]);
static int g_line_gap = 30;

// Helper: composite a boiled glyph into a destination pixel buffer.
// dest is WIN_W*WIN_H uint32_t pixels, format same as original code (0xRRGGBBAA
// with AA in low byte)
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
      // Keep same pixel format used elsewhere: white RGB with alpha in low byte
      dest[dy * dest_w + dx] = 0xFFFFFF00u | (uint32_t)a;
    }
  }
}

// Render a full frame into the provided pixel buffer (RGBA-like packing used
// above). This function doesn't touch SDL; it only uses font base bitmaps
// (read-only) and local temp buffers. t is the time value to feed into
// boil_frame.
static void render_frame_to_pixels(uint32_t *pixels, GlyphCache *cache,
                                   float t) {
  // Clear to transparent (so when copied to texture and rendered on black
  // background it appears correctly)
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

      // Use a temporary buffer so we don't modify shared glyph->boiled_bitmap
      int gw = g->width;
      int gh = g->height;
      uint8_t *tmp = (uint8_t *)malloc(gw * gh);
      if (!tmp) {
        // fallback: skip
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

// Background thread: generates complete frames (raw pixel buffers) starting
// from start_idx
void *background_generator(void *arg) {
  int idx = (int)(intptr_t)arg;
  // We need a GlyphCache pointer: passed via a global pointer through arg?
  // Simpler: pass pointer via global (set before launching)
  GlyphCache *cache = (GlyphCache *)NULL;
  // We'll get it via a global pointer set before spawning. To avoid more
  // globals, cast arg to struct in main. BUT simpler: main will provide pointer
  // via a global variable set before thread creation.
  extern GlyphCache *g_bg_cache;
  cache = g_bg_cache;
  if (!cache)
    return NULL;

  while (bg_keep_running) {
    float t = frame_dt * idx++;

    // allocate pixels
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
    printf("Frame generated");

    // small throttle so we don't starve main thread completely
    usleep(1000);
  }

  return NULL;
}

// We'll expose a global pointer for the background thread to access the glyph
// cache (safe as long as glyphs are loaded before thread starts)
GlyphCache *g_bg_cache = NULL;

// ----------------------------------------------------
//                      MAIN
// ----------------------------------------------------
int main(int argc, char *argv[]) {
  const char *fontfile = "font.otf";
  if (argc >= 2)
    fontfile = argv[1];

  // Load font data
  FILE *fp = fopen(fontfile, "rb");
  if (!fp) {
    fprintf(stderr, "Failed to open font file '%s'\n", fontfile);
    return 1;
  }
  fseek(fp, 0, SEEK_END);
  long fsize = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  uint8_t *ttf_data = (uint8_t *)malloc(fsize);
  if (fread(ttf_data, 1, fsize, fp) != (size_t)fsize) {
    fprintf(stderr, "Failed to read font\n");
    fclose(fp);
    return 1;
  }
  fclose(fp);

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
    return 1;
  }

  // Create window immediately (Option A) — will show black while pre-rendering
  SDL_Window *window =
      SDL_CreateWindow("Line-Boil", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H, SDL_WINDOW_SHOWN);

  // request render-target support
  SDL_Renderer *renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
  if (!renderer) {
    fprintf(stderr, "SDL Renderer creation failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  // Initialize glyph cache
  GlyphCache cache = {0};
  if (!stbtt_InitFont(&cache.font, ttf_data,
                      stbtt_GetFontOffsetForIndex(ttf_data, 0))) {
    fprintf(stderr, "stbtt_InitFont failed\n");
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  cache.scale = stbtt_ScaleForPixelHeight(&cache.font, 64.0f);

  // load glyphs used in the lines
  for (int i = 0; i < g_line_count; i++)
    for (int j = 0; g_lines[i][j]; j++)
      load_glyph(&cache, renderer, (unsigned char)g_lines[i][j]);

  // Pre-generate PREG full frames (blocking) but keep window responsive and
  // black.
  SDL_Event ev;
  int running = 1;

  // We'll store pre-generated textures here
  SDL_Texture **framesA = (SDL_Texture **)malloc(PREG * sizeof(SDL_Texture *));
  if (!framesA) {
    fprintf(stderr, "Alloc framesA failed\n");
    SDL_Quit();
    return 1;
  }

  // Show black screen while pre-rendering
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
  SDL_RenderPresent(renderer);

  for (int i = 0; i < PREG && running; i++) {
    // handle quit events while generating
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) {
        running = 0;
        break;
      }
    }
    if (!running)
      break;

    // allocate pixel buffer and render into it (CPU-only)
    uint32_t *pixels = (uint32_t *)malloc(WIN_W * WIN_H * sizeof(uint32_t));
    if (!pixels) {
      fprintf(stderr, "Failed alloc pixels for pregen frame %d\n", i);
      running = 0;
      break;
    }
    render_frame_to_pixels(pixels, &cache, frame_dt * i);

    // create texture on main thread and upload pixels
    SDL_Texture *tex =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                          SDL_TEXTUREACCESS_STATIC, WIN_W, WIN_H);
    if (!tex) {
      fprintf(stderr, "Failed create texture: %s\n", SDL_GetError());
      free(pixels);
      running = 0;
      break;
    }
    // Update texture (note: pixel layout matches what render_text used)
    SDL_UpdateTexture(tex, NULL, pixels, WIN_W * sizeof(uint32_t));

    framesA[i] = tex;
    free(pixels);

    // optional: present a black frame to keep UI alive (we already have black
    // shown) small yield so window remains responsive
    printf("Frame pre-generated");
    SDL_Delay(1);
  }

  if (!running) {
    // cleanup preallocated frames if any and exit
    for (int k = 0; k < PREG; k++)
      if (framesA[k])
        SDL_DestroyTexture(framesA[k]);
    free(framesA);

    // cleanup glyphs
    for (int gi = 0; gi < 128; gi++) {
      if (cache.glyphs[gi].loaded) {
        if (cache.glyphs[gi].base_bitmap)
          stbtt_FreeBitmap(cache.glyphs[gi].base_bitmap, NULL);
        if (cache.glyphs[gi].boiled_bitmap)
          free(cache.glyphs[gi].boiled_bitmap);
        if (cache.glyphs[gi].texture)
          SDL_DestroyTexture(cache.glyphs[gi].texture);
      }
    }

    free(ttf_data);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
  }

  // Start background generator now (it will fill framesB_pixels with raw pixel
  // buffers)
  g_bg_cache = &cache;
  bg_keep_running = 1;
  pthread_t bg_thread;
  pthread_create(&bg_thread, NULL, background_generator,
                 (void *)(intptr_t)PREG);

  // During playback, main thread will convert raw pixel buffers to textures
  // before showing them. Keep an array for framesB textures (created on main
  // thread from framesB_pixels)
  SDL_Texture **framesB_textures = NULL;
  int framesB_textures_size = 0;
  int framesB_textures_cap = 0;

  // Play framesA at 12 FPS, while converting any background-produced pixel
  // buffers
  const Uint32 framems = 1000 / FPS;

  for (int i = 0; i < PREG && running; i++) {
    Uint32 frame_start = SDL_GetTicks();

    // move any produced raw pixels into textures (main thread only)
    pthread_mutex_lock(&bg_lock);
    while (framesB_pixels_size > 0) {
      uint32_t *pix = framesB_pixels[--framesB_pixels_size];
      // convert to texture
      if (framesB_textures_size == framesB_textures_cap) {
        framesB_textures_cap =
            framesB_textures_cap ? framesB_textures_cap * 2 : 256;
        framesB_textures = (SDL_Texture **)realloc(
            framesB_textures, framesB_textures_cap * sizeof(SDL_Texture *));
      }
      SDL_Texture *t =
          SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                            SDL_TEXTUREACCESS_STATIC, WIN_W, WIN_H);
      if (!t) {
        // if texture creation fails, discard
        free(pix);
      } else {
        SDL_UpdateTexture(t, NULL, pix, WIN_W * sizeof(uint32_t));
        framesB_textures[framesB_textures_size++] = t;
        free(pix);
      }
    }
    pthread_mutex_unlock(&bg_lock);

    // handle events
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT)
        running = 0;
    }
    if (!running)
      break;

    // render the pre-generated texture
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, framesA[i], NULL, NULL);
    SDL_RenderPresent(renderer);

    // maintain 12 FPS
    Uint32 elapsed = SDL_GetTicks() - frame_start;
    if (elapsed < framems)
      SDL_Delay(framems - elapsed);
  }

  // After finishing framesA, keep playing framesB_textures in order (they were
  // converted on-the-fly)
  int bidx = 0;
  while (running) {
    Uint32 frame_start = SDL_GetTicks();

    // First, convert any new raw pixel frames to textures
    pthread_mutex_lock(&bg_lock);
    while (framesB_pixels_size > 0) {
      uint32_t *pix = framesB_pixels[--framesB_pixels_size];
      if (framesB_textures_size == framesB_textures_cap) {
        framesB_textures_cap =
            framesB_textures_cap ? framesB_textures_cap * 2 : 256;
        framesB_textures = (SDL_Texture **)realloc(
            framesB_textures, framesB_textures_cap * sizeof(SDL_Texture *));
      }
      SDL_Texture *t =
          SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                            SDL_TEXTUREACCESS_STATIC, WIN_W, WIN_H);
      if (!t) {
        free(pix);
      } else {
        SDL_UpdateTexture(t, NULL, pix, WIN_W * sizeof(uint32_t));
        framesB_textures[framesB_textures_size++] = t;
        free(pix);
      }
    }
    pthread_mutex_unlock(&bg_lock);

    // If there is a frame to show, show it; otherwise wait a bit for background
    // to produce one.
    if (bidx < framesB_textures_size) {
      // events
      while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT)
          running = 0;
      }
      if (!running)
        break;

      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
      SDL_RenderClear(renderer);
      SDL_RenderCopy(renderer, framesB_textures[bidx], NULL, NULL);
      SDL_RenderPresent(renderer);
      bidx++;
    } else {
      // no frame yet; if background thread is still running, wait small and
      // continue
      if (!bg_keep_running)
        break;
      // allow event handling (so user can quit)
      while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT)
          running = 0;
      }
      SDL_Delay(5);
    }

    Uint32 elapsed = SDL_GetTicks() - frame_start;
    if (elapsed < framems)
      SDL_Delay(framems - elapsed);
  }

  // Signal background to stop and join
  bg_keep_running = 0;
  pthread_join(bg_thread, NULL);

  // cleanup framesA
  for (int i = 0; i < PREG; i++)
    if (framesA[i])
      SDL_DestroyTexture(framesA[i]);
  free(framesA);

  // cleanup framesB textures
  for (int i = 0; i < framesB_textures_size; i++)
    if (framesB_textures[i])
      SDL_DestroyTexture(framesB_textures[i]);
  free(framesB_textures);

  // cleanup any remaining raw pixel buffers (shouldn't be many)
  pthread_mutex_lock(&bg_lock);
  for (int i = 0; i < framesB_pixels_size; i++) {
    free(framesB_pixels[i]);
  }
  free(framesB_pixels);
  framesB_pixels = NULL;
  framesB_pixels_size = framesB_pixels_cap = 0;
  pthread_mutex_unlock(&bg_lock);

  // Cleanup glyphs
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
