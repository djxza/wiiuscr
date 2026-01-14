#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USE_SDL3

#ifdef USE_SDL3

#include <SDL3/SDL.h>
#include <SDL3/SDL_oldnames.h>

#define KEY_A SDL_SCANCODE_A
#define KEY_B SDL_SCANCODE_B
#define KEY_C SDL_SCANCODE_C
#define KEY_D SDL_SCANCODE_D
#define KEY_E SDL_SCANCODE_E
#define KEY_F SDL_SCANCODE_F
#define KEY_G SDL_SCANCODE_G
#define KEY_H SDL_SCANCODE_H
#define KEY_I SDL_SCANCODE_I
#define KEY_J SDL_SCANCODE_J
#define KEY_K SDL_SCANCODE_K
#define KEY_L SDL_SCANCODE_L
#define KEY_M SDL_SCANCODE_M
#define KEY_N SDL_SCANCODE_N
#define KEY_O SDL_SCANCODE_O
#define KEY_P SDL_SCANCODE_P
#define KEY_Q SDL_SCANCODE_Q
#define KEY_R SDL_SCANCODE_R
#define KEY_S SDL_SCANCODE_S
#define KEY_T SDL_SCANCODE_T
#define KEY_U SDL_SCANCODE_U
#define KEY_V SDL_SCANCODE_V
#define KEY_W SDL_SCANCODE_W
#define KEY_X SDL_SCANCODE_X
#define KEY_Y SDL_SCANCODE_Y
#define KEY_Z SDL_SCANCODE_Z

#define KEY_0 SDL_SCANCODE_0
#define KEY_1 SDL_SCANCODE_1
#define KEY_2 SDL_SCANCODE_2
#define KEY_3 SDL_SCANCODE_3
#define KEY_4 SDL_SCANCODE_4
#define KEY_5 SDL_SCANCODE_5
#define KEY_6 SDL_SCANCODE_6
#define KEY_7 SDL_SCANCODE_7
#define KEY_8 SDL_SCANCODE_8
#define KEY_9 SDL_SCANCODE_9

#define KEY_SPACE SDL_SCANCODE_SPACE
#define KEY_ENTER SDL_SCANCODE_RETURN
#define KEY_ESCAPE SDL_SCANCODE_ESCAPE
#define KEY_BACKSPACE SDL_SCANCODE_BACKSPACE
#define KEY_TAB SDL_SCANCODE_TAB
#define KEY_SHIFT SDL_SCANCODE_LSHIFT
#define KEY_CTRL SDL_SCANCODE_LCTRL
#define KEY_ALT SDL_SCANCODE_LALT

#define KEY_UP SDL_SCANCODE_UP
#define KEY_DOWN SDL_SCANCODE_DOWN
#define KEY_LEFT SDL_SCANCODE_LEFT
#define KEY_RIGHT SDL_SCANCODE_RIGHT

#define KEY_COUNT SDL_SCANCODE_COUNT

#endif // USE_SDL3

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

#define cerr(...)                                                              \
  do {                                                                         \
    fprintf(stderr, ANSI_COLOR_RESET);                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
    fprintf(stderr, "%s\n", ANSI_COLOR_RESET);                                 \
  } while (0)

#define ASSERT(expr, ...)                                                      \
  if (!(expr)) {                                                               \
    cerr(__VA_ARGS__);                                                         \
    exit(EXIT_FAILURE);                                                        \
  }

typedef float f32;
typedef double f64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef size_t usize;

typedef struct {
  f32 x, y;
} v2;

typedef struct {
  i32 x, y;
} i2;

typedef struct {
  u32 x, y;
} u2;

typedef struct {
  u32 width;
  u32 height;
} s2;

typedef struct {
  s2 properties;
  u32 *pixels;
} screen_t;

usize vec_to_idx(u2 p, s2 prop);
void put_pixel(u2 p, u32 col, screen_t *ps);
void init();
void kill();
void loop(screen_t *ps, const bool *keys);
