# **README — lineboil**

**lineboil** is a small SDL2 project that simulates _line boiling_ — a hand-drawn animation jitter effect — on monospaced text.
It renders the text repeatedly with small randomized offsets, producing a stylized “boiling lines” animation at **12 frames per second**.

> [!WARNING] This program is a memory hog! Make sure to adjust the pregreneration count if you have limited RAM.

The program works in two stages:

1. **Pre-renders 144 frames** (12 seconds at 12 FPS) off-screen before showing anything.
2. **Plays those frames**, while a background thread continues generating more frames for seamless playback.

---

## **Features**

- Smooth 12 FPS playback
- 72-frame preroll for instant animation start
- Continuous background frame generation while playback occurs
- Uses SDL2 + stb_truetype
- High-resolution, low-jitter character-level animation
- Cross-platform C code (Linux, Windows, BSD, macOS)

---

## **Building**

A Makefile is already included.
To build:

```bash
make
```

This produces an executable named:

```
lineboil
```

### **Dependencies**

You’ll need development headers for:

- `SDL2`

On Linux/Arch:

```bash
sudo pacman -S sdl2
```

On Debian/Ubuntu:

```bash
sudo apt install libsdl2-dev
```

---

## **Running**

Simply run:

```bash
./lineboil
```

What happens:

1. A window opens immediately (blank).
2. The program quietly generates 72 frames off-screen.
3. Once done, playback starts at **12 FPS**.
4. Meanwhile, a background thread keeps generating new frames.
5. When the initial 72 frames finish, the newly generated frames begin playing automatically.

No input required; the animation loops continuously as long as the program stays open.

---

## **How It Works (Frame Pipeline Overview)**

### **1. Pre-generation phase**

Before the first frame is shown, the program:

- Renders each frame into an `SDL_Texture`
- Stores them in a ring buffer
- Produces exactly **72 textures** for the initial minute of animation

These frames are rendered using:

- A fixed seed per frame
- Randomized per-glyph offsets
- A consistent layout grid for text

### **2. Playback phase**

Playback runs on the main thread at a fixed 12 FPS:

- Reads from the pre-generated frame array
- Draws one texture per frame
- Tracks frame timers precisely using SDL ticks

### **3. Background generation**

While playback runs:

- A worker thread continues producing additional frames
- These frames are appended to a second buffer
- When the first block finishes, the program immediately switches to the new frames

The effect:
No frame drops, no delays, no visible hiccups.

---

## **Why Pre-generate Frames?**

Line boiling is expensive — every character jitter must be recalculated.
Pre-generating frames:

- Guarantees perfect playback timing
- Avoids CPU spikes
- Lets slow machines still maintain 12 FPS (A.K.A mine)
- Allows the background generator to work asynchronously

---

## **Customizing**

The displayed text is defined in the C source.
Feel free to edit the `main_text[]` constant to display whatever you want.
Multi-line text works fine.
Change the pre-generation count by modifying the `PREG` constant.
