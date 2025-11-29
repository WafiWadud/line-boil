# Compiler
CC = clang

# Compiler flags
CFLAGS = -Wall -Wextra -O3 -march=native -flto -ffast-math \
         -fno-ident -fno-asynchronous-unwind-tables -fno-stack-protector \
         -funroll-loops -fomit-frame-pointer -ffunction-sections -fdata-sections \
         -ffreestanding -fno-exceptions -lpthread

# Linker flags
LDFLAGS = -Wl,--gc-sections,--strip-all

# Libraries
LIBS = -lSDL2 -lm

# Target
all: lineboil

lineboil: lineboil.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@ $(LIBS)

# Clean up compiled binary
clean:
	rm -f lineboil

.PHONY: all clean
