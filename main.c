// main.c - Main program entry point

#include "frame_generator.h"
#include "glyph_cache.h"
#include <SDL2/SDL.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
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

  SDL_Window *window =
      SDL_CreateWindow("Line-Boil", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H, SDL_WINDOW_SHOWN);

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

  // Load glyphs
  for (int i = 0; i < g_line_count; i++)
    for (int j = 0; g_lines[i][j]; j++)
      load_glyph(&cache, renderer, (unsigned char)g_lines[i][j]);

  SDL_Event ev;
  int running = 1;

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

  // Pre-generate frames
  for (int i = 0; i < PREG && running; i++) {
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) {
        running = 0;
        break;
      }
    }
    if (!running)
      break;

    uint32_t *pixels = (uint32_t *)malloc(WIN_W * WIN_H * sizeof(uint32_t));
    if (!pixels) {
      fprintf(stderr, "Failed alloc pixels for pregen frame %d\n", i);
      running = 0;
      break;
    }
    render_frame_to_pixels(pixels, &cache, frame_dt * i);

    SDL_Texture *tex =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                          SDL_TEXTUREACCESS_STATIC, WIN_W, WIN_H);
    if (!tex) {
      fprintf(stderr, "Failed create texture: %s\n", SDL_GetError());
      free(pixels);
      running = 0;
      break;
    }
    SDL_UpdateTexture(tex, NULL, pixels, WIN_W * sizeof(uint32_t));

    framesA[i] = tex;
    free(pixels);

    printf("Frame pre-generated\n");
    SDL_Delay(1);
  }

  if (!running) {
    for (int k = 0; k < PREG; k++)
      if (framesA[k])
        SDL_DestroyTexture(framesA[k]);
    free(framesA);
    cleanup_glyph_cache(&cache);
    free(ttf_data);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
  }

  // Start background generator
  g_bg_cache = &cache;
  bg_keep_running = 1;
  pthread_t bg_thread;
  pthread_create(&bg_thread, NULL, background_generator,
                 (void *)(intptr_t)PREG);

  SDL_Texture **framesB_textures = NULL;
  int framesB_textures_size = 0;
  int framesB_textures_cap = 0;

  const Uint32 framems = 1000 / FPS;

  // Play pre-generated frames
  for (int i = 0; i < PREG && running; i++) {
    Uint32 frame_start = SDL_GetTicks();

    // Convert background-produced pixel buffers to textures
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

    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT)
        running = 0;
    }
    if (!running)
      break;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, framesA[i], NULL, NULL);
    SDL_RenderPresent(renderer);

    Uint32 elapsed = SDL_GetTicks() - frame_start;
    if (elapsed < framems)
      SDL_Delay(framems - elapsed);
  }

  // Play background-generated frames
  int bidx = 0;
  while (running) {
    Uint32 frame_start = SDL_GetTicks();

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

    if (bidx < framesB_textures_size) {
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
      if (!bg_keep_running)
        break;
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

  // Cleanup
  bg_keep_running = 0;
  pthread_join(bg_thread, NULL);

  for (int i = 0; i < PREG; i++)
    if (framesA[i])
      SDL_DestroyTexture(framesA[i]);
  free(framesA);

  for (int i = 0; i < framesB_textures_size; i++)
    if (framesB_textures[i])
      SDL_DestroyTexture(framesB_textures[i]);
  free(framesB_textures);

  pthread_mutex_lock(&bg_lock);
  for (int i = 0; i < framesB_pixels_size; i++) {
    free(framesB_pixels[i]);
  }
  free(framesB_pixels);
  framesB_pixels = NULL;
  framesB_pixels_size = framesB_pixels_cap = 0;
  pthread_mutex_unlock(&bg_lock);

  cleanup_glyph_cache(&cache);
  free(ttf_data);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
