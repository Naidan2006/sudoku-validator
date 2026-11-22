CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2
LDFLAGS ?= -pthread
GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0 2>/dev/null)
GTK_LIBS   := $(shell pkg-config --libs gtk+-3.0 2>/dev/null)

all: sudoku_validator sudoku_gui

# Console version (no GTK needed)
sudoku_validator: main.c sudoku_core.c sudoku_core.h
	$(CC) $(CFLAGS) main.c sudoku_core.c -o $@ $(LDFLAGS)

# GTK3 GUI version
sudoku_gui: gui.c sudoku_core.c sudoku_core.h
	$(CC) $(CFLAGS) $(GTK_CFLAGS) gui.c sudoku_core.c -o $@ $(GTK_LIBS) $(LDFLAGS)

test: sudoku_validator
	tests/run_tests.sh ./sudoku_validator

clean:
	rm -f sudoku_validator sudoku_gui

.PHONY: all test clean
