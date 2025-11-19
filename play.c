#include <SDL2/SDL.h>
#include <gif_lib.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  SDL_Texture **frames;
  int *delays;
  int frame_count;
  int width;
  int height;
} GIFData;

typedef struct {
  GIFData glyphs[128];
  int loaded[128];
} GlyphCache;

// Load a single GIF file and extract all frames
GIFData load_gif(SDL_Renderer *renderer, const char *filename) {
  GIFData gif = {0};
  int error;

  GifFileType *gif_file = DGifOpenFileName(filename, &error);
  if (!gif_file) {
    printf("Failed to open GIF: %s\n", filename);
    return gif;
  }

  if (DGifSlurp(gif_file) == GIF_ERROR) {
    printf("Failed to read GIF: %s\n", filename);
    DGifCloseFile(gif_file, &error);
    return gif;
  }

  gif.width = gif_file->SWidth;
  gif.height = gif_file->SHeight;
  gif.frame_count = gif_file->ImageCount;
  gif.frames = malloc(sizeof(SDL_Texture *) * gif.frame_count);
  gif.delays = malloc(sizeof(int) * gif.frame_count);

  // Allocate buffer for compositing frames
  Uint32 *buffer = calloc(gif.width * gif.height, sizeof(Uint32));

  for (int i = 0; i < gif_file->ImageCount; i++) {
    SavedImage *image = &gif_file->SavedImages[i];
    ColorMapObject *cmap = image->ImageDesc.ColorMap ? image->ImageDesc.ColorMap
                                                     : gif_file->SColorMap;

    // Get delay from graphics control extension
    gif.delays[i] = 10; // default 100ms
    for (int j = 0; j < image->ExtensionBlockCount; j++) {
      ExtensionBlock *ext = &image->ExtensionBlocks[j];
      if (ext->Function == GRAPHICS_EXT_FUNC_CODE) {
        int delay = (ext->Bytes[2] << 8) | ext->Bytes[1];
        gif.delays[i] = delay > 0 ? delay : 10;
        break;
      }
    }

    // Composite this frame onto the buffer
    int left = image->ImageDesc.Left;
    int top = image->ImageDesc.Top;
    int width = image->ImageDesc.Width;
    int height = image->ImageDesc.Height;

    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        int buf_idx = (top + y) * gif.width + (left + x);
        int img_idx = y * width + x;
        GifByteType color_idx = image->RasterBits[img_idx];

        if (cmap && color_idx < cmap->ColorCount) {
          GifColorType *color = &cmap->Colors[color_idx];
          buffer[buf_idx] = 0xFF000000 | (color->Red << 16) |
                            (color->Green << 8) | color->Blue;
        }
      }
    }

    // Create SDL texture from buffer
    SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(
        buffer, gif.width, gif.height, 32, gif.width * 4, 0x00FF0000,
        0x0000FF00, 0x000000FF, 0xFF000000);

    gif.frames[i] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
  }

  free(buffer);
  DGifCloseFile(gif_file, &error);

  return gif;
}

// Load a glyph by ASCII value
void load_glyph(SDL_Renderer *renderer, GlyphCache *cache, int ascii) {
  if (ascii < 0 || ascii >= 128 || cache->loaded[ascii])
    return;

  char filename[64];
  sprintf(filename, "glyph_%03d.gif", ascii);

  cache->glyphs[ascii] = load_gif(renderer, filename);
  cache->loaded[ascii] = 1;
}

// Check if a character has a descender
int has_descender(int ascii) {
  // Lowercase letters with descenders
  if (ascii == 'g' || ascii == 'j' || ascii == 'p' || ascii == 'q' ||
      ascii == 'y') {
    return 1;
  }
  // Punctuation that descends below baseline
  if (ascii == ',' || ascii == ';') {
    return 1;
  }
  return 0;
}

// Find the tallest glyph height for non-descender characters
int get_baseline_height(GlyphCache *cache, const char *text) {
  int max_height = 0;
  for (int i = 0; text[i]; i++) {
    int ascii = (unsigned char)text[i];
    if (cache->loaded[ascii] && !has_descender(ascii)) {
      GIFData *gif = &cache->glyphs[ascii];
      if (gif->height > max_height) {
        max_height = gif->height;
      }
    }
  }
  return max_height;
}

void render_text(SDL_Renderer *renderer, GlyphCache *cache, const char *text,
                 int x, int y, Uint32 time_ms) {
  int cursor_x = x;
  int baseline_height = get_baseline_height(cache, text);

  for (int i = 0; text[i]; i++) {
    int ascii = (unsigned char)text[i];

    if (!cache->loaded[ascii]) {
      load_glyph(renderer, cache, ascii);
    }

    GIFData *gif = &cache->glyphs[ascii];
    if (gif->frame_count == 0) {
      cursor_x += 20; // Skip missing glyphs
      continue;
    }

    // Calculate which frame to show based on time
    int total_delay = 0;
    for (int j = 0; j < gif->frame_count; j++) {
      total_delay += gif->delays[j];
    }

    int frame_time = time_ms % (total_delay * 10);
    int current_frame = 0;
    int accumulated = 0;

    for (int j = 0; j < gif->frame_count; j++) {
      accumulated += gif->delays[j] * 10;
      if (frame_time < accumulated) {
        current_frame = j;
        break;
      }
    }

    int y_offset;
    if (has_descender(ascii)) {
      y_offset = y + (baseline_height - gif->height) + 13;
    } else if (ascii == '\'' || ascii == '\"') {
      y_offset = y;
    } else {
      y_offset = y + (baseline_height - gif->height);
    }

    SDL_Rect dest = {cursor_x, y_offset, gif->width, gif->height};
    SDL_RenderCopy(renderer, gif->frames[current_frame], NULL, &dest);

    cursor_x += gif->width;
  }
}

int main(int argc, char *argv[]) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("SDL init failed: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window *window =
      SDL_CreateWindow("GIF Text Renderer", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1600, 500, SDL_WINDOW_SHOWN);

  SDL_Renderer *renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

  GlyphCache cache = {0};

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
  int line_gap = 30; // vertical space between lines

  int running = 1;
  SDL_Event event;

  while (running) {
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT)
        running = 0;
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    int y_offset = 0; // starting y position
    for (int i = 0; i < line_count; i++) {
      render_text(renderer, &cache, lines[i], 0, y_offset, SDL_GetTicks());
      y_offset += 24 + line_gap; // 24 = assumed line height, adjust as needed
    }

    SDL_RenderPresent(renderer);
  }

  // Cleanup
  for (int i = 0; i < 128; i++) {
    if (cache.loaded[i]) {
      for (int j = 0; j < cache.glyphs[i].frame_count; j++) {
        SDL_DestroyTexture(cache.glyphs[i].frames[j]);
      }
      free(cache.glyphs[i].frames);
      free(cache.glyphs[i].delays);
    }
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
