# Compiler
CC = clang

# Common flags for both targets
CFLAGS = -Wall -Wextra -O3 -march=native -flto -ffast-math \
         -fno-ident -fno-asynchronous-unwind-tables -fno-stack-protector \
         -funroll-loops -fomit-frame-pointer -ffunction-sections -fdata-sections \
         -ffreestanding -fno-exceptions

LDFLAGS = -Wl,--gc-sections,--strip-all -lm

# SDL2 and GIF libraries for play target
PLAY_LIBS = -lSDL2 -lgif

# Targets
all: gen play

gen: gen.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

play: play.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(PLAY_LIBS) $< -o $@

# Clean up compiled binaries
clean:
	rm -f gen play

