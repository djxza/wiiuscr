#define STB_IMAGE_IMPLEMENTATION
#define USE_SDL3
#include "loop.h"
#include "stb_image.h"

typedef struct {
  u32 *pixels;
  s2 size;
  int channels;
} tex_t;

/*
  Pixel format: 0xRRGGBBAA  (RGBA in u32)
*/
static inline u32 rgba_to_u32(u8 r, u8 g, u8 b, u8 a) {
  return ((u32)r << 24) | ((u32)g << 16) | ((u32)b << 8) | (u32)a;
}

static inline u8 get_r(u32 c) { return (c >> 24) & 0xFF; }
static inline u8 get_g(u32 c) { return (c >> 16) & 0xFF; }
static inline u8 get_b(u32 c) { return (c >> 8) & 0xFF; }
static inline u8 get_a(u32 c) { return c & 0xFF; }

/*
  Straight alpha blending in RGBA space
*/
static u32 alpha_blend(u32 src, u32 dst) {
  u32 sa = get_a(src);
  if (sa == 0)
    return dst;
  if (sa == 255)
    return src;

  u32 da = get_a(dst);

  u32 sr = get_r(src);
  u32 sg = get_g(src);
  u32 sb = get_b(src);

  u32 dr = get_r(dst);
  u32 dg = get_g(dst);
  u32 db = get_b(dst);

  u32 inv = 255 - sa;

  u32 out_r = (sr * sa + dr * inv) / 255;
  u32 out_g = (sg * sa + dg * inv) / 255;
  u32 out_b = (sb * sa + db * inv) / 255;
  u32 out_a = sa + (da * inv) / 255;

  return rgba_to_u32(out_r, out_g, out_b, out_a);
}

void cp_img(tex_t *pt, const char *path) {
  int req_channels = 4; // force RGBA
  u8 *tmp = stbi_load(path, &pt->size.width, &pt->size.height, &pt->channels,
                      req_channels);

  ASSERT(tmp != NULL, "Failed to load image: %s", path);

  usize pixel_count = pt->size.width * pt->size.height;
  pt->pixels = (u32 *)malloc(pixel_count * sizeof(u32));
  ASSERT(pt->pixels != NULL, "Failed to allocate texture memory");

  for (usize i = 0; i < pixel_count; ++i) {
    usize base = i * 4;
    u8 r = tmp[base + 0];
    u8 g = tmp[base + 1];
    u8 b = tmp[base + 2];
    u8 a = tmp[base + 3];
    pt->pixels[i] = rgba_to_u32(r, g, b, a);
  }

  stbi_image_free(tmp);
}

void kill_img(tex_t *pt) {
  if (pt->pixels) {
    free(pt->pixels);
    pt->pixels = NULL;
  }
}

usize vec_to_idx(u2 p, s2 prop) { return p.y * prop.width + p.x; }

void put_pixel(u2 p, u32 col, screen_t *ps) {
  if (p.x < ps->properties.width && p.y < ps->properties.height) {
    ps->pixels[vec_to_idx(p, ps->properties)] = col;
  }
}

void put_pixel_blend(u2 p, u32 col, screen_t *ps) {
  if (p.x < ps->properties.width && p.y < ps->properties.height) {
    usize idx = vec_to_idx(p, ps->properties);
    ps->pixels[idx] = alpha_blend(col, ps->pixels[idx]);
  }
}

void draw_img(u2 pos, tex_t *pt, screen_t *ps) {
  ASSERT(pt->pixels != NULL, "Texture not loaded\n");

  int start_x = pos.x;
  int start_y = pos.y;
  int end_x = pos.x + pt->size.width;
  int end_y = pos.y + pt->size.height;

  if (start_x >= ps->properties.width || start_y >= ps->properties.height)
    return;

  if (end_x > ps->properties.width)
    end_x = ps->properties.width;
  if (end_y > ps->properties.height)
    end_y = ps->properties.height;
  if (start_x < 0)
    start_x = 0;
  if (start_y < 0)
    start_y = 0;

  for (int y = start_y; y < end_y; ++y) {
    for (int x = start_x; x < end_x; ++x) {
      int src_x = x - pos.x;
      int src_y = y - pos.y;

      usize src_idx = src_y * pt->size.width + src_x;
      u32 src_pixel = pt->pixels[src_idx];

      if (get_a(src_pixel) == 0)
        continue;

      usize dst_idx = y * ps->properties.width + x;
      ps->pixels[dst_idx] = (get_a(src_pixel) == 255)
                                ? src_pixel
                                : alpha_blend(src_pixel, ps->pixels[dst_idx]);
    }
  }
}

/*
  Downscale using premultiplied averaging (RGBA)
*/
tex_t downscale_img(int times, tex_t t) {
  tex_t result;
  result.size.width = t.size.width / times;
  result.size.height = t.size.height / times;
  result.channels = t.channels;

  result.pixels =
      (u32 *)malloc(result.size.width * result.size.height * sizeof(u32));
  ASSERT(result.pixels, "downscale alloc failed");

  for (int y = 0; y < result.size.height; ++y) {
    for (int x = 0; x < result.size.width; ++x) {

      u64 r_sum = 0, g_sum = 0, b_sum = 0, a_sum = 0;

      for (int j = 0; j < times; ++j) {
        for (int i = 0; i < times; ++i) {
          int src_x = x * times + i;
          int src_y = y * times + j;

          usize idx = src_y * t.size.width + src_x;
          u32 p = t.pixels[idx];

          u8 r = get_r(p);
          u8 g = get_g(p);
          u8 b = get_b(p);
          u8 a = get_a(p);

          a_sum += a;
          r_sum += r * a;
          g_sum += g * a;
          b_sum += b * a;
        }
      }

      u32 count = times * times;
      u8 a_avg = (u8)(a_sum / count);

      u8 r_avg = 0, g_avg = 0, b_avg = 0;
      if (a_sum > 0) {
        r_avg = (u8)(r_sum / a_sum);
        g_avg = (u8)(g_sum / a_sum);
        b_avg = (u8)(b_sum / a_sum);
      }

      usize dst_idx = y * result.size.width + x;
      result.pixels[dst_idx] = rgba_to_u32(r_avg, g_avg, b_avg, a_avg);
    }
  }

  return result;
}

typedef struct {
  const char *icon;
} app_t;

typedef struct {
  app_t *handle;
  usize size;
  u32 cols;
  u32 rows;
} apps_t;

void draw_bg(screen_t *ps) {
  tex_t bg = {0};
  cp_img(&bg, "./res/gfx/bg.png");
  draw_img((u2){0, 0}, &bg, ps);
  kill_img(&bg);
}

apps_t apps = {0};
tex_t brew = {0};
tex_t tile = {0};

void init_apps() {
  int gc = 6;
  apps.handle = malloc(gc * sizeof(app_t));
  ASSERT(apps.handle, "apps");
  apps.size = gc;
  apps.rows = 3;
  apps.cols = 5;
}

void init_tiles() { cp_img(&tile, "./res/gfx/tile.png"); }

void draw_tile(int x, int y, screen_t *ps) { draw_img((u2){x, y}, &tile, ps); }

void draw_tiles(screen_t *ps) {
  init_apps();
  init_tiles();

  for (int x = 0; x < apps.cols; ++x) {
    for (int y = 0; y < apps.rows; ++y) {
      draw_tile(200 * x + 40, 200 * y + 40, ps);
    }
  }
}

void draw_home(screen_t *ps, const bool *keys) {
  cp_img(&brew, "./res/gfx/brew.png");
  brew = downscale_img(4, brew);

  draw_bg(ps);
  draw_tiles(ps);
  draw_img((u2){512, 512}, &brew, ps);

  kill_img(&brew);
}

void loop(screen_t *ps, const bool *keys) {
  // Clear to white RGBA
  for (int i = 0; i < ps->properties.width * ps->properties.height; ++i) {
    ps->pixels[i] = rgba_to_u32(255, 255, 255, 255);
  }

  draw_home(ps, keys);
}
