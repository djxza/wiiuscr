#include <SDL3/SDL.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_oldnames.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>

#include "./loop.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* =========================
   CONFIG
   ========================= */

#define WIDTH 1280
#define HEIGHT 720

#define BUFFER_SCALE 1
#define BUFFER_WIDTH (WIDTH / BUFFER_SCALE)
#define BUFFER_HEIGHT (HEIGHT / BUFFER_SCALE)

/* =========================
   WINDOW / RENDERER
   ========================= */

typedef struct {
  u2 size;
  SDL_Window *handle;
  const char *title;
  bool running;
} window_t;

typedef struct {
  SDL_Renderer *handle;
  SDL_Texture *tex;
  u32 buffer[BUFFER_WIDTH * BUFFER_HEIGHT];
} renderer_t;

/* =========================
   STATE
   ========================= */

typedef struct {
  renderer_t ren;
  window_t win;
} state_t;

void init_sdl(void) {
  ASSERT(SDL_Init(SDL_INIT_VIDEO), "SDL_Init failed: %s", SDL_GetError());
}

void init_win(state_t *s, u2 size, const char *title) {
  s->win.size = size;
  s->win.title = title;
  s->win.handle = SDL_CreateWindow(title, size.x, size.y, 0);
  // SDL_SetRelativeMouseMode(true);
  assert(s->win.handle);
}

void init_renderer(state_t *s) {
  s->ren.handle = SDL_CreateRenderer(s->win.handle, NULL);
  assert(s->ren.handle);

  s->ren.tex = SDL_CreateTexture(s->ren.handle, SDL_PIXELFORMAT_RGBA8888,
                                 SDL_TEXTUREACCESS_STREAMING, BUFFER_WIDTH,
                                 BUFFER_HEIGHT);

  assert(s->ren.tex);
  SDL_SetTextureScaleMode(s->ren.tex, SDL_SCALEMODE_NEAREST);
}

void kill_state(state_t *s) {
  SDL_DestroyTexture(s->ren.tex);
  SDL_DestroyRenderer(s->ren.handle);
  SDL_DestroyWindow(s->win.handle);
}

void kill_sdl(void) { SDL_Quit(); }

/* =========================
   RENDER
   ========================= */

void clear_buffer(renderer_t *r, u32 color) {
  for (u32 i = 0; i < BUFFER_WIDTH * BUFFER_HEIGHT; ++i)
    r->buffer[i] = color;
}

void render(renderer_t *r) {
  SDL_UpdateTexture(r->tex, NULL, r->buffer, BUFFER_WIDTH * sizeof(u32));
  SDL_RenderClear(r->handle);
  SDL_RenderTexture(r->handle, r->tex, NULL, NULL);
  SDL_RenderPresent(r->handle);
}

int main(void) {
  init_sdl();

  state_t state = {0};

  init_win(&state, (u2){WIDTH, HEIGHT}, "CPU RAYCASTER");
  init_renderer(&state);

  state.win.running = true;

  u64 last = SDL_GetTicks();

  screen_t scr;
  scr.pixels = state.ren.buffer;
  scr.properties.width = WIDTH;
  scr.properties.height = HEIGHT;

  // init all the shit to prepare for the loop ahaaaaaaaaaaaaa
  init();

  while (state.win.running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT)
        state.win.running = false;
    }

    u64 now = SDL_GetTicks();
    f32 dt = (now - last) / 1000.0f;
    last = now;

    const bool *keys = SDL_GetKeyboardState(NULL);

    loop(&scr, keys);

    render(&state.ren);
  }

  kill();

  kill_state(&state);
  kill_sdl();
  return 0;
}
