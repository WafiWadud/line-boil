# Compiler
CC = clang

# Compiler flags
CFLAGS = -Wall -Wextra -O3 -march=native -flto -ffast-math \
         -fno-ident -fno-asynchronous-unwind-tables -fno-stack-protector \
         -funroll-loops -fomit-frame-pointer -ffunction-sections -fdata-sections \
         -ffreestanding -fno-exceptions -I.

# Linker flags
LDFLAGS = -Wl,--gc-sections,--strip-all -flto

# Libraries
LIBS = -lSDL2 -lm -lpthread

# Source files
SRCS = main.c voronoi.c glyph_cache.c frame_generator.c

# Object files
OBJS = $(SRCS:.c=.o)

# Target
TARGET = lineboil

# Default target
all: $(TARGET)

# Link object files into final binary
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@ $(LIBS)

# Compile source files into object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Dependencies
main.o: main.c glyph_cache.h frame_generator.h stb_truetype.h
voronoi.o: voronoi.c voronoi.h
glyph_cache.o: glyph_cache.c glyph_cache.h voronoi.h stb_truetype.h
frame_generator.o: frame_generator.c frame_generator.h glyph_cache.h voronoi.h

# Clean up compiled files
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
