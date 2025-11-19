#define MSF_GIF_IMPL
#include "msf_gif.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

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
// Per-glyph GIF generator
// ----------------------

int main(int argc, char **argv) {
  const char *fontfile = "font.otf";
  if (argc >= 2)
    fontfile = argv[1];

  // parameters
  const int FIRST = 31;
  const int LAST = 128;
  const int FRAMES = 60;         // frames per glyph
  const float SIZE_PX = 64.0f;   // pixel height used for baking
  const float STRENGTH = 3.0f;   // boil strength
  const float FREQ = 0.04f;      // boil frequency
  const int CENTI_PER_FRAME = 8; // centiseconds per GIF frame (8 -> 12.5 FPS)
  const int QUALITY = 16;        // msf gif quality (1..16)

  // load TTF
  FILE *fp = fopen(fontfile, "rb");
  if (!fp) {
    fprintf(stderr, "Failed to open font file '%s'", fontfile);
    return 1;
  }
  fseek(fp, 0, SEEK_END);
  long fsize = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  uint8_t *ttf_data = (uint8_t *)malloc(fsize);
  if (!ttf_data) {
    fclose(fp);
    return 2;
  }
  fread(ttf_data, 1, fsize, fp);
  fclose(fp);

  stbtt_fontinfo font;
  if (!stbtt_InitFont(&font, ttf_data,
                      stbtt_GetFontOffsetForIndex(ttf_data, 0))) {
    fprintf(stderr, "stbtt_InitFont failed");
    free(ttf_data);
    return 3;
  }

  float scale = stbtt_ScaleForPixelHeight(&font, SIZE_PX);

  // enable transparency threshold so background becomes transparent in GIFs
  msf_gif_alpha_threshold = 128; // pixels with alpha < 128 will be transparent

  printf("Per-glyph GIF generator");
  printf("Font: %s, size: %.1f px, frames/glyph: %d", fontfile, SIZE_PX,
         FRAMES);

  for (int code = FIRST; code <= LAST; code++) {
    int x0, y0, x1, y1;
    stbtt_GetCodepointBitmapBox(&font, code, scale, scale, &x0, &y0, &x1, &y1);
    int w = x1 - x0;
    int h = y1 - y0;
    if (w <= 0 || h <= 0)
      continue; // skip empty glyphs (space, etc.)

    int gw = w;
    int gh = h;

    // get glyph bitmap (grayscale alpha values)
    int gw_out = gw, gh_out = gh;
    uint8_t *glyph = stbtt_GetCodepointBitmap(&font, scale, scale, code,
                                              &gw_out, &gh_out, 0, 0);
    if (!glyph)
      continue;

    // work buffers
    uint8_t *boiled = (uint8_t *)malloc(gw * gh);
    if (!boiled) {
      stbtt_FreeBitmap(glyph, NULL);
      free(ttf_data);
      return 4;
    }

    // create RGBA frame buffer (msf expects RGBA8)
    uint8_t *rgba = (uint8_t *)malloc(gw * gh * 4);
    if (!rgba) {
      free(boiled);
      stbtt_FreeBitmap(glyph, NULL);
      free(ttf_data);
      return 5;
    }

    // begin msf gif
    MsfGifState state = {};
    if (!msf_gif_begin(&state, gw, gh)) {
      fprintf(stderr, "msf_gif_begin failed for code %d", code);
      free(rgba);
      free(boiled);
      stbtt_FreeBitmap(glyph, NULL);
      continue;
    }

    // for each frame, boil and submit
    for (int f = 0; f < FRAMES; f++) {
      float t = f * 0.3f;
      boil_frame(boiled, glyph, gw, gh, t, STRENGTH, FREQ);

      // convert single-channel to RGBA (white glyph, alpha from sample)
      for (int i = 0; i < gw * gh; i++) {
        uint8_t a = boiled[i];
        rgba[i * 4 + 0] = 255; // R
        rgba[i * 4 + 1] = 255; // G
        rgba[i * 4 + 2] = 255; // B
        rgba[i * 4 + 3] = a;   // A
      }

      if (!msf_gif_frame(&state, rgba, CENTI_PER_FRAME, QUALITY, gw * 4)) {
        fprintf(stderr, "msf_gif_frame failed for code %d frame %d", code, f);
        break;
      }
    }

    // finish gif and write to disk
    MsfGifResult res = msf_gif_end(&state);
    if (res.data && res.dataSize) {
      char outname[128];
      snprintf(outname, sizeof(outname), "glyph_%03d.gif", code);
      FILE *out = fopen(outname, "wb");
      if (out) {
        fwrite(res.data, 1, res.dataSize, out);
        fclose(out);
        printf("Wrote %s (w=%d h=%d frames=%d)", outname, gw, gh, FRAMES);
      } else {
        fprintf(stderr, "Failed to open %s for writing", outname);
      }
      msf_gif_free(res);
    } else {
      fprintf(stderr, "msf_gif_end returned no data for code %d", code);
    }

    // cleanup
    free(rgba);
    free(boiled);
    stbtt_FreeBitmap(glyph, NULL);
  }

  free(ttf_data);
  printf("Done.");
  return 0;
}
