#include <stdlib.h>
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

static inline float smoothstep(float t) {
  if (t <= 0.0f)
    return 0.0f;
  if (t >= 1.0f)
    return 1.0f;
  return t * t * (3.0f - 2.0f * t);
}

void draw_img_soft(u2 pos, tex_t *pt, int softness_px, screen_t *ps) {
  ASSERT(pt->pixels != NULL, "Texture not loaded");

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

      u8 a = get_a(src_pixel);
      if (a == 0)
        continue;

      // Distance to closest edge
      int dx = src_x < (pt->size.width - 1 - src_x)
                   ? src_x
                   : (pt->size.width - 1 - src_x);

      int dy = src_y < (pt->size.height - 1 - src_y)
                   ? src_y
                   : (pt->size.height - 1 - src_y);

      int d = dx < dy ? dx : dy;

      float edge_alpha = 1.0f;

      if (softness_px > 0 && d < softness_px) {
        float t = (float)d / (float)softness_px;
        edge_alpha = smoothstep(t);
      }

      u8 new_a = (u8)((float)a * edge_alpha);
      if (new_a == 0)
        continue;

      u32 softened = rgba_to_u32(get_r(src_pixel), get_g(src_pixel),
                                 get_b(src_pixel), new_a);

      usize dst_idx = y * ps->properties.width + x;
      ps->pixels[dst_idx] = (new_a == 255)
                                ? softened
                                : alpha_blend(softened, ps->pixels[dst_idx]);
    }
  }
}

void draw_img_rounded_corners(u2 pos, tex_t *pt, int radius_px, u8 corner_mask,
                              screen_t *ps) {
  ASSERT(pt->pixels != NULL, "Texture not loaded");

  int w = pt->size.width;
  int h = pt->size.height;

  // Early return if no rounding needed
  if (radius_px <= 0 || corner_mask == 0) {
    draw_img(pos, pt, ps);
    return;
  }

  // Clamp radius to maximum possible value (half of smallest dimension)
  int max_radius = (w < h ? w : h) / 2;
  if (radius_px > max_radius) {
    radius_px = max_radius;
  }

  int start_x = pos.x;
  int start_y = pos.y;
  int end_x = pos.x + w;
  int end_y = pos.y + h;

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

  // Precompute squared radius for distance comparison
  int r2 = radius_px * radius_px;

  for (int y = start_y; y < end_y; ++y) {
    for (int x = start_x; x < end_x; ++x) {

      int sx = x - pos.x;
      int sy = y - pos.y;

      usize src_idx = sy * w + sx;
      u32 src = pt->pixels[src_idx];

      u8 a = get_a(src);
      if (a == 0)
        continue;

      // Default: pixel is inside the rectangle (not in a corner)
      bool inside = true;
      float edge_factor = 1.0f;

      // Check which quadrant the pixel is in relative to corners
      // and apply appropriate corner rounding if enabled

      if (sx < radius_px && sy < radius_px) { // Top-left quadrant
        if (corner_mask & 0x1) {              // Top-left corner rounded
          int dx = radius_px - sx - 1;
          int dy = radius_px - sy - 1;
          int dist2 = dx * dx + dy * dy;

          if (dist2 > r2) {
            inside = false; // Outside the rounded corner
          } else if (dist2 > (radius_px - 1) * (radius_px - 1)) {
            // In the anti-aliasing region (outer edge)
            float dist = sqrtf((float)dist2);
            edge_factor = 1.0f - ((dist - (float)(radius_px - 1)));
            if (edge_factor < 0.0f)
              edge_factor = 0.0f;
          }
        }
      } else if (sx >= w - radius_px && sy < radius_px) { // Top-right quadrant
        if (corner_mask & 0x2) { // Top-right corner rounded
          int dx = sx - (w - radius_px);
          int dy = radius_px - sy - 1;
          int dist2 = dx * dx + dy * dy;

          if (dist2 > r2) {
            inside = false;
          } else if (dist2 > (radius_px - 1) * (radius_px - 1)) {
            float dist = sqrtf((float)dist2);
            edge_factor = 1.0f - ((dist - (float)(radius_px - 1)));
            if (edge_factor < 0.0f)
              edge_factor = 0.0f;
          }
        }
      } else if (sx < radius_px &&
                 sy >= h - radius_px) { // Bottom-left quadrant
        if (corner_mask & 0x4) {        // Bottom-left corner rounded
          int dx = radius_px - sx - 1;
          int dy = sy - (h - radius_px);
          int dist2 = dx * dx + dy * dy;

          if (dist2 > r2) {
            inside = false;
          } else if (dist2 > (radius_px - 1) * (radius_px - 1)) {
            float dist = sqrtf((float)dist2);
            edge_factor = 1.0f - ((dist - (float)(radius_px - 1)));
            if (edge_factor < 0.0f)
              edge_factor = 0.0f;
          }
        }
      } else if (sx >= w - radius_px &&
                 sy >= h - radius_px) { // Bottom-right quadrant
        if (corner_mask & 0x8) {        // Bottom-right corner rounded
          int dx = sx - (w - radius_px);
          int dy = sy - (h - radius_px);
          int dist2 = dx * dx + dy * dy;

          if (dist2 > r2) {
            inside = false;
          } else if (dist2 > (radius_px - 1) * (radius_px - 1)) {
            float dist = sqrtf((float)dist2);
            edge_factor = 1.0f - ((dist - (float)(radius_px - 1)));
            if (edge_factor < 0.0f)
              edge_factor = 0.0f;
          }
        }
      }

      if (!inside)
        continue;

      // Apply anti-aliasing edge factor
      u8 out_a = (u8)((float)a * edge_factor);
      if (out_a == 0)
        continue;

      u32 out = rgba_to_u32(get_r(src), get_g(src), get_b(src), out_a);

      usize dst_idx = y * ps->properties.width + x;
      ps->pixels[dst_idx] =
          (out_a == 255) ? out : alpha_blend(out, ps->pixels[dst_idx]);
    }
  }
}
// Convenience functions for common corner patterns
void draw_img_rounded_top(u2 pos, tex_t *pt, int radius_px, screen_t *ps) {
  // Top-left and top-right corners rounded: 0x1 | 0x2 = 0x3
  draw_img_rounded_corners(pos, pt, radius_px, 0x3, ps);
}

void draw_img_rounded_bottom(u2 pos, tex_t *pt, int radius_px, screen_t *ps) {
  // Bottom-left and bottom-right corners rounded: 0x4 | 0x8 = 0xC
  draw_img_rounded_corners(pos, pt, radius_px, 0xC, ps);
}

void draw_img_rounded_left(u2 pos, tex_t *pt, int radius_px, screen_t *ps) {
  // Top-left and bottom-left corners rounded: 0x1 | 0x4 = 0x5
  draw_img_rounded_corners(pos, pt, radius_px, 0x5, ps);
}

void draw_img_rounded_right(u2 pos, tex_t *pt, int radius_px, screen_t *ps) {
  // Top-right and bottom-right corners rounded: 0x2 | 0x8 = 0xA
  draw_img_rounded_corners(pos, pt, radius_px, 0xA, ps);
}

// Original function for all corners rounded (for backward compatibility)
void draw_img_rounded(u2 pos, tex_t *pt, int radius_px, screen_t *ps) {
  // All corners rounded: 0x1 | 0x2 | 0x4 | 0x8 = 0xF
  draw_img_rounded_corners(pos, pt, radius_px, 0xF, ps);
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

tex_t downscale_img_px(int dst_w, int dst_h, tex_t t) {
  tex_t result;
  result.size.width = dst_w;
  result.size.height = dst_h;
  result.channels = t.channels;

  result.pixels = (u32 *)malloc(dst_w * dst_h * sizeof(u32));
  ASSERT(result.pixels, "downscale alloc failed");

  // Scaling ratios
  float scale_x = (float)t.size.width / (float)dst_w;
  float scale_y = (float)t.size.height / (float)dst_h;

  for (int y = 0; y < dst_h; ++y) {
    for (int x = 0; x < dst_w; ++x) {

      // Source pixel bounds for this destination pixel
      int src_x0 = (int)(x * scale_x);
      int src_x1 = (int)((x + 1) * scale_x);
      int src_y0 = (int)(y * scale_y);
      int src_y1 = (int)((y + 1) * scale_y);

      // Clamp (important for edges)
      if (src_x1 <= src_x0)
        src_x1 = src_x0 + 1;
      if (src_y1 <= src_y0)
        src_y1 = src_y0 + 1;
      if (src_x1 > t.size.width)
        src_x1 = t.size.width;
      if (src_y1 > t.size.height)
        src_y1 = t.size.height;

      u64 r_sum = 0, g_sum = 0, b_sum = 0, a_sum = 0;
      u32 count = 0;

      for (int sy = src_y0; sy < src_y1; ++sy) {
        for (int sx = src_x0; sx < src_x1; ++sx) {
          usize idx = sy * t.size.width + sx;
          u32 p = t.pixels[idx];

          u8 r = get_r(p);
          u8 g = get_g(p);
          u8 b = get_b(p);
          u8 a = get_a(p);

          a_sum += a;
          r_sum += r * a;
          g_sum += g * a;
          b_sum += b * a;
          count++;
        }
      }

      u8 a_avg = (u8)(a_sum / (count ? count : 1));

      u8 r_avg = 0, g_avg = 0, b_avg = 0;
      if (a_sum > 0) {
        r_avg = (u8)(r_sum / a_sum);
        g_avg = (u8)(g_sum / a_sum);
        b_avg = (u8)(b_sum / a_sum);
      }

      result.pixels[y * dst_w + x] = rgba_to_u32(r_avg, g_avg, b_avg, a_avg);
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

  u2 selected;
} apps_t;

apps_t apps = {0};
tex_t brew = {0};
tex_t tile = {0};
tex_t bg = {0};

void draw_bg(screen_t *ps) {
  cp_img(&bg, "./res/gfx/bg.png");
  draw_img((u2){0, 0}, &bg, ps);
}

void kill_bg() { kill_img(&bg); }

void init_apps() {
  int gc = 6;
  apps.handle = malloc(gc * sizeof(app_t));
  ASSERT(apps.handle, "apps");
  apps.size = gc;
  apps.rows = 3;
  apps.cols = 5;
}

void init_tiles() { cp_img(&tile, "./res/gfx/tile.png"); }

void draw_tile(int w, int h, int x, int y, screen_t *ps) {
  ASSERT(tile.pixels != NULL, "Tile texture not loaded");

  if (w <= 0 || h <= 0)
    return;

  float scale_x = (float)tile.size.width / (float)w;
  float scale_y = (float)tile.size.height / (float)h;

  int start_x = x;
  int start_y = y;
  int end_x = x + w;
  int end_y = y + h;

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

  for (int dy = start_y; dy < end_y; ++dy) {
    for (int dx = start_x; dx < end_x; ++dx) {

      int local_x = dx - x;
      int local_y = dy - y;

      int src_x = (int)(local_x * scale_x);
      int src_y = (int)(local_y * scale_y);

      if (src_x < 0)
        src_x = 0;
      if (src_y < 0)
        src_y = 0;
      if (src_x >= tile.size.width)
        src_x = tile.size.width - 1;
      if (src_y >= tile.size.height)
        src_y = tile.size.height - 1;

      usize src_idx = src_y * tile.size.width + src_x;
      u32 src_pixel = tile.pixels[src_idx];

      if (get_a(src_pixel) == 0)
        continue;

      usize dst_idx = dy * ps->properties.width + dx;

      ps->pixels[dst_idx] = (get_a(src_pixel) == 255)
                                ? src_pixel
                                : alpha_blend(src_pixel, ps->pixels[dst_idx]);
    }
  }
}

void draw_app(int w, int h, int x, int y, app_t app, screen_t *ps) {
  draw_tile(w, h, x, y, ps);
  int padding = 25;

  tex_t tex;
  cp_img(&tex, app.icon);
  tex = downscale_img_px(w - 2 * padding, h - 2 * padding, tex);

  // draw_img_rounded_corners(u2 pos, tex_t *pt, int radius_px, u8 corner_mask,
  // screen_t *ps)
  draw_img_rounded((u2){x + padding, y + padding}, &tex, 16, ps);
}

void draw_selected(int tile_x, int tile_y, int tile_size, screen_t *ps) {
  u32 color = 0x00FFFFFF; // soft blue

  int padding = -4; // distance from tile

  int length = 24;   // length of each line
  int thickness = 5; // stroke thickness
  int gap = -1;      // diagonal offset from corner

  int left = tile_x + padding;
  int right = tile_x + tile_size - padding - 1;
  int top = tile_y + padding;
  int bottom = tile_y + tile_size - padding - 1;

  int i, t;

  /* ---------- top-left ---------- */

  // horizontal
  for (i = 0; i < length; i++) {
    for (t = -thickness / 2; t <= thickness / 2; t++) {
      put_pixel((u2){left + gap + i, top + t}, color, ps);
    }
  }

  // vertical
  for (i = 0; i < length; i++) {
    for (t = -thickness / 2; t <= thickness / 2; t++) {
      put_pixel((u2){left + t, top + gap + i}, color, ps);
    }
  }

  /* ---------- top-right ---------- */

  // horizontal
  for (i = 0; i < length; i++) {
    for (t = -thickness / 2; t <= thickness / 2; t++) {
      put_pixel((u2){right - gap - length + 1 + i, top + t}, color, ps);
    }
  }

  // vertical
  for (i = 0; i < length; i++) {
    for (t = -thickness / 2; t <= thickness / 2; t++) {
      put_pixel((u2){right + t, top + gap + i}, color, ps);
    }
  }

  /* ---------- bottom-left ---------- */

  // horizontal
  for (i = 0; i < length; i++) {
    for (t = -thickness / 2; t <= thickness / 2; t++) {
      put_pixel((u2){left + gap + i, bottom + t}, color, ps);
    }
  }

  // vertical
  for (i = 0; i < length; i++) {
    for (t = -thickness / 2; t <= thickness / 2; t++) {
      put_pixel((u2){left + t, bottom - gap - length + 1 + i}, color, ps);
    }
  }

  /* ---------- bottom-right ---------- */

  // horizontal
  for (i = 0; i < length; i++) {
    for (t = -thickness / 2; t <= thickness / 2; t++) {
      put_pixel((u2){right - gap - length + 1 + i, bottom + t}, color, ps);
    }
  }

  // vertical
  for (i = 0; i < length; i++) {
    for (t = -thickness / 2; t <= thickness / 2; t++) {
      put_pixel((u2){right + t, bottom - gap - length + 1 + i}, color, ps);
    }
  }
}

// 80 od 576 = .138
void draw_tiles(screen_t *ps) {
  f32 tile_multiplier = .8;

  // Screen size
  const int sw = ps->properties.width;
  const int sh = ps->properties.height;

  // Padding tuning (Wii U vibes)
  const int outer_padding_x = sw * 0.08f; // screen edge padding
  const int outer_padding_y = sh * 0.08f;
  const int inner_padding = 20; // space between tiles

  // Available space for the grid
  const int avail_w =
      sw - 2 * outer_padding_x - (apps.cols - 1) * inner_padding;
  const int avail_h =
      sh - 2 * outer_padding_y - (apps.rows - 1) * inner_padding;

  ASSERT(avail_w > 0 && avail_h > 0, "Invalid tile layout");

  // Tile size (square tiles)
  const int tile_w = avail_w / apps.cols;
  const int tile_h = avail_h / apps.rows;

  int tile_sz = tile_w < tile_h ? tile_w : tile_h;
  tile_sz *= 9;
  tile_sz /= 10;

  // Actual grid size (used for centering)
  const int grid_w = apps.cols * tile_sz + (apps.cols - 1) * inner_padding;
  const int grid_h = apps.rows * tile_sz + (apps.rows - 1) * inner_padding;

  // Center grid
  const int start_x = (sw - grid_w) / 2;
  const int start_y = (sh - grid_h) / 2;

  for (u32 y = 0; y < apps.rows; ++y) {
    for (u32 x = 0; x < apps.cols; ++x) {

      int px = start_x + x * (tile_sz + inner_padding);
      int py = start_y + y * (tile_sz + inner_padding);

      draw_app(tile_sz, tile_sz, px, py, (app_t){"./res/gfx/brew.png"}, ps);

      if (x == apps.selected.x && y == apps.selected.y)
        draw_selected(px, py, tile_sz, ps);
    }
  }
}

void init() {
  cp_img(&brew, "./res/gfx/brew.png");
  brew = downscale_img(4, brew);
  init_apps();
  init_tiles();

  apps.selected = (u2){0, 0};
}

void kill() {
  kill_img(&brew);
  kill_img(&bg);
}

static bool last[KEY_COUNT] = {0};

void draw_home(screen_t *ps, const bool *keys) {
  draw_bg(ps);
  draw_tiles(ps);

  if (keys[KEY_LEFT] && !last[KEY_LEFT] && apps.selected.x > 0)
    --apps.selected.x;
  else if (keys[KEY_RIGHT] && !last[KEY_RIGHT] &&
           apps.selected.x < apps.cols - 1)
    ++apps.selected.x;
  else if (keys[KEY_DOWN] && !last[KEY_DOWN] && apps.selected.y < apps.rows - 1)
    ++apps.selected.y;
  else if (keys[KEY_UP] && !last[KEY_UP] && apps.selected.y > 0)
    --apps.selected.y;

  memcpy(last, keys, sizeof last);
}

void loop(screen_t *ps, const bool *keys) { draw_home(ps, keys); }
