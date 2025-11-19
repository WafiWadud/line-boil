# line-boil

A minimal toolchain for generating "boiling" animated GIFs from font glyphs and rendering animated text in C.

---

## Overview

**line-boil** is a project for producing animated GIFs of each glyph in a font, with a subtle "boiling" noise effect. It also includes a simple SDL2-based renderer to display animated text using those generated GIF glyphs.

- `gen.c` creates per-glyph boiling GIFs from a font.
- `play.c` renders animated text in an SDL2 window from generated GIF glyphs.

---

## Features

- Procedural boiling effect for each glyph using Voronoi noise.
- Generates a GIF for each printable ASCII glyph (codepoints 32–127) from a supplied TrueType or OpenType font.
- Built-in fast GIF writing (`msf_gif.h`).
- Minimal dependencies: only SDL2 and giflib needed for playback.
- Simple Makefile for building.

---

## Requirements

**Generation (`gen.c`)**

- C compiler (Clang recommended)
- Font file (`font.otf` or any OTF/TTF font)
- No external dependencies (all required headers included)

**Playback (`play.c`)**

- SDL2 development libraries
- giflib development libraries

---

## Build Instructions

On Linux/macOS:

```sh
# Install SDL2 and giflib if needed
# (Debian/Ubuntu: sudo apt install libsdl2-dev libgif-dev)

make all
```

This builds two binaries:

- `gen` – glyph GIF generator
- `play` – animated text player

To clean up:

```sh
make clean
```

---

## Usage

### 1. Generate Glyph GIFs

By default, `gen` uses `font.otf`. You can specify another font file:

```sh
./gen path/to/your/fontfile.otf
```

This outputs `glyph_XXX.gif` files (one per glyph) in the working directory.

### 2. Play Animated Text

After glyph GIFs are generated, use `play`:

```sh
./play
```

An SDL2 window will open, displaying sample animated sentences using the generated boiling glyphs.

---

## File Overview

- `gen.c` – Generates GIFs for every glyph using procedural noise, creating an animated "boil" effect.
- `play.c` – Loads generated GIFs and renders strings of animated text in a window.
- `msf_gif.h`, `stb_truetype.h` – Bundled dependencies for GIF encoding and font rasterization.
- `Makefile` – Simplified build system.

---

## License

- **msf_gif.h** and other bundled sources are under MIT or Public Domain (see respective headers).
- This repository’s original code is MIT licensed.

See [msf_gif.h](https://github.com/WafiWadud/line-boil/blob/main/msf_gif.h) and other headers for details.

---

## Credits

- [stb_truetype.h](https://github.com/nothings/stb) by Sean Barrett & contributors
- [msf_gif.h](https://github.com/notnullnotvoid/msf_gif) by notnullnotvoid
- Project by [WafiWadud](https://github.com/WafiWadud)

---

## Example

![](glyph_065.gif)

---

## Advanced

To change effect strength or GIF quality, tweak constants near the top of `gen.c`.

---

## Troubleshooting

- Ensure that `font.otf` (or the font you specify) is present and readable.
- SDL2/giflib must be installed for running `play.c`.

---

Happy boiling!
