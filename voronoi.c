// voronoi.c - Voronoi noise implementation

#include "voronoi.h"
#include <math.h>

static inline uint32_t hash2d(int x, int y) {
  uint32_t h = x * 374761393u + y * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}

float voronoi(float x, float y, float t) {
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

void boil_frame(uint8_t *dst, uint8_t *src, int w, int h, float t,
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
