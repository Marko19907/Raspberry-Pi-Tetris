# Compiler and flags
CC = gcc
CFLAGS = -Wall -O2

# Source file and output executable
SRC = tetris.c
OUTPUT = tetris

# Default target, which builds the executable
all: clean $(OUTPUT)

# Rule to build the executable from the source file
$(OUTPUT): $(SRC)
	$(CC) $(CFLAGS) $< -o $@

# Clean target to remove the executable
clean:
	rm -f $(OUTPUT)
