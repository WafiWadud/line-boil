// voronoi.h - Voronoi noise functions

#ifndef VORONOI_H
#define VORONOI_H

#include <stdint.h>

// Generate Voronoi noise at given coordinates and time
float voronoi(float x, float y, float t);

// Apply boiling effect to a frame
void boil_frame(uint8_t *dst, uint8_t *src, int w, int h, float t,
                float strength, float freq);

#endif // VORONOI_H
