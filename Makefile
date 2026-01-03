# ======================
# Compiler
# ======================
CC = clang

# ======================
# Target
# ======================
TARGET = lineboil

# ======================
# Sources
# ======================
SRCS = main.c voronoi.c glyph_cache.c frame_generator.c
OBJS = $(SRCS:.c=.o)

# ======================
# Libraries
# ======================
LIBS = -lSDL2 -lm -lpthread

# ======================
# Common flags
# ======================
COMMON_CFLAGS  = -Wall -Wextra -I.
COMMON_LDFLAGS =

# ======================
# Release flags
# ======================
RELEASE_CFLAGS = \
	-O3 -march=native -flto -ffast-math \
	-fno-ident -fno-asynchronous-unwind-tables -fno-stack-protector \
	-funroll-loops -fomit-frame-pointer \
	-ffunction-sections -fdata-sections \
	-ffreestanding -fno-exceptions

RELEASE_LDFLAGS = \
	-flto -Wl,--gc-sections,--strip-all

# ======================
# Debug flags
# ======================
DEBUG_CFLAGS = \
	-O0 -g3 -ggdb3 \
	-fno-omit-frame-pointer \
	-fno-inline \
	-fno-optimize-sibling-calls

DEBUG_LDFLAGS = \
	-g3

# ======================
# Default target = release
# ======================
all: release

# ======================
# Release build
# ======================
release: CFLAGS  = $(COMMON_CFLAGS) $(RELEASE_CFLAGS)
release: LDFLAGS = $(COMMON_LDFLAGS) $(RELEASE_LDFLAGS)
release: $(TARGET)

# ======================
# Debug build
# ======================
debug: CFLAGS  = $(COMMON_CFLAGS) $(DEBUG_CFLAGS)
debug: LDFLAGS = $(COMMON_LDFLAGS) $(DEBUG_LDFLAGS)
debug: $(TARGET)

# ======================
# Link
# ======================
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@ $(LIBS)

# ======================
# Compile
# ======================
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ======================
# Dependencies
# ======================
main.o: main.c glyph_cache.h frame_generator.h stb_truetype.h
voronoi.o: voronoi.c voronoi.h
glyph_cache.o: glyph_cache.c glyph_cache.h voronoi.h stb_truetype.h
frame_generator.o: frame_generator.c frame_generator.h glyph_cache.h voronoi.h

# ======================
# Clean
# ======================
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all release debug clean
