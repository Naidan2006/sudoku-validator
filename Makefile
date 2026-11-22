CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2
LDFLAGS ?= -pthread
GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0 2>/dev/null)
GTK_LIBS   := $(shell pkg-config --libs gtk+-3.0 2>/dev/null)
HAVE_GTK   := $(shell pkg-config --exists gtk+-3.0 2>/dev/null && echo yes)

# Console always, GUI only when GTK 3 is installed
all: sudoku_validator $(if $(HAVE_GTK),sudoku_gui)

sudoku_validator: main.c sudoku_core.c sudoku_core.h
	$(CC) $(CFLAGS) main.c sudoku_core.c -o $@ $(LDFLAGS)

sudoku_gui: gui.c sudoku_core.c sudoku_core.h
	$(CC) $(CFLAGS) $(GTK_CFLAGS) gui.c sudoku_core.c -o $@ $(GTK_LIBS) $(LDFLAGS)

# Windows GUI (MSYS2 UCRT64): no console window
gui-windows: gui.c sudoku_core.c sudoku_core.h
	$(CC) $(CFLAGS) $(GTK_CFLAGS) gui.c sudoku_core.c -o sudoku_gui.exe $(GTK_LIBS) $(LDFLAGS) -mwindows

test: sudoku_validator
	tests/run_tests.sh ./sudoku_validator

tsan: main.c sudoku_core.c sudoku_core.h
	$(CC) -g -O1 -fsanitize=thread -pthread main.c sudoku_core.c -o sudoku_tsan
	tests/run_tests.sh ./sudoku_tsan

asan: main.c sudoku_core.c sudoku_core.h
	$(CC) -g -O1 -fsanitize=address,undefined -pthread main.c sudoku_core.c -o sudoku_asan
	tests/run_tests.sh ./sudoku_asan

clean:
	rm -f sudoku_validator sudoku_gui sudoku_gui.exe sudoku_tsan sudoku_asan

.PHONY: all gui-windows test tsan asan clean
