# cl_columns
A port of Sega classic puzzle from the 90s.
Runs under linux terminal.
Jewels are replaced by unicode characters.

# how to play
Pieces falls by comuns of 3 from top screen.
Move and toggle your columns so it makes lines of pieces.
3 or more aligned pieces (horizontally, vertically or diagonally) disappears and score points.

### Required packets to install:
- sudo apt install libncurses5-dev libncursesw5-dev

### compilation
execute './makefile' in source code directory.
NB: that's not pretty, but I haven't found yet how to compile with standard 'makefile' code dealing with wchar_t and libncurses.

### screenshot:
![screenshot](https://raw.githubusercontent.com/louvetr/cl_columns/master/cl_columns_preview.png "Screenshot")