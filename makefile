CC = clang

CCFLAGS = -std=c2x -Wall -Wpedantic -g -O2 -v
INCFLAGS  = -I./src
INCFLAGS += -I./lib/SDL/include
INCFLAGS += -I./lib/SDL_image/include/

_SDL_BUILD_DIR = .build
SDL_LIB = ./lib/SDL/$(_SDL_BUILD_DIR)
IMG_LIB = ./lib/SDL_image/$(_SDL_BUILD_DIR)

LDFLAGS = -L$(SDL_LIB) -L$(IMG_LIB) -lSDL3 -lSDL3_image -lm

SRC = ./src/main.c

all:
	$(CC) $(CCFLAGS) $(INCFLAGS) -c $(SRC) -o ./src/main.o
	$(CC) $(CCFLAGS) $(INCFLAGS) -c ./src/loop.c -o src/loop.o
	$(CC) ./src/main.o ./src/loop.o $(LDFLAGS) -o ./bin/main

lib_sdl:
	cd ./lib/SDL && cmake -S . -B $(_SDL_BUILD_DIR)
	cd ./lib/SDL/$(_SDL_BUILD_DIR) && make

clean_sdl:
	rm -rf ./lib/SDL/$(_SDL_BUILD_DIR)

lib_img:
	cd ./lib/SDL_image && cmake -S . -B $(_SDL_BUILD_DIR)
	cd ./lib/SDL_image/$(_SDL_BUILD_DIR) && make

clean_img:
	rm -rf ./lib/SDL_image/$(_SDL_BUILD_DIR)

libs: lib_sdl lib_img

clean:
	rm -f ./bin/main ./src/main.o

run: all
	LD_LIBRARY_PATH=./lib/SDL/.build:./lib/SDL_image/.build ./bin/main
