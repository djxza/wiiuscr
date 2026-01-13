#define STB_IMAGE_IMPLEMENTATION
#define USE_SDL3
#include "loop.h"
#include "stb_image.h"

typedef struct {
  u32 *pixels;
  s2 size;
  int channels;
} tex_t;

static u32 rgba_to_u32(u8 r, u8 g, u8 b, u8 a) {
  // ARGB format (Alpha in high byte)
  return ((u32)a << 24) | ((u32)r << 16) | ((u32)g << 8) | (u32)b;
}

// Helper function for alpha blending
static u32 alpha_blend(u32 src, u32 dst) {
  // Extract color components from source (ARGB format)
  u8 src_a = (src >> 24) & 0xFF;
  u8 src_r = (src >> 16) & 0xFF;
  u8 src_g = (src >> 8) & 0xFF;
  u8 src_b = src & 0xFF;

  // Extract color components from destination
  u8 dst_a = (dst >> 24) & 0xFF;
  u8 dst_r = (dst >> 16) & 0xFF;
  u8 dst_g = (dst >> 8) & 0xFF;
  u8 dst_b = dst & 0xFF;

  // If source is completely transparent, return destination
  if (src_a == 0)
    return dst;

  // If source is completely opaque, return source
  if (src_a == 255)
    return src;

  // If destination is completely transparent, return source with its alpha
  if (dst_a == 0)
    return src;

  // Alpha blending calculations
  float src_alpha = src_a / 255.0f;
  float dst_alpha = dst_a / 255.0f;
  float out_alpha = src_alpha + dst_alpha * (1.0f - src_alpha);

  if (out_alpha < 0.001f)
    return 0; // Fully transparent

  // Calculate blended RGB components
  u8 out_r = (u8)((src_r * src_alpha + dst_r * dst_alpha * (1.0f - src_alpha)) /
                  out_alpha);
  u8 out_g = (u8)((src_g * src_alpha + dst_g * dst_alpha * (1.0f - src_alpha)) /
                  out_alpha);
  u8 out_b = (u8)((src_b * src_alpha + dst_b * dst_alpha * (1.0f - src_alpha)) /
                  out_alpha);
  u8 out_a = (u8)(out_alpha * 255.0f);

  // Combine back to ARGB format
  return (out_a << 24) | (out_r << 16) | (out_g << 8) | out_b;
}

// Faster alpha blending with integer math (optimized version)
static u32 alpha_blend_fast(u32 src, u32 dst) {
  u8 src_a = (src >> 24) & 0xFF;

  // Early exit for common cases
  if (src_a == 0)
    return dst;
  if (src_a == 255)
    return src;

  u8 src_r = (src >> 16) & 0xFF;
  u8 src_g = (src >> 8) & 0xFF;
  u8 src_b = src & 0xFF;

  u8 dst_r = (dst >> 16) & 0xFF;
  u8 dst_g = (dst >> 8) & 0xFF;
  u8 dst_b = dst & 0xFF;

  // Integer-based alpha blending (faster than float)
  // src_a ranges from 0-255, where 0 is transparent, 255 is opaque
  u16 inv_alpha = 255 - src_a;

  u16 blended_r = (src_r * src_a + dst_r * inv_alpha) / 255;
  u16 blended_g = (src_g * src_a + dst_g * inv_alpha) / 255;
  u16 blended_b = (src_b * src_a + dst_b * inv_alpha) / 255;

  // Keep destination alpha or blend alpha? For simplicity, keep dest alpha
  u8 out_a = (dst >> 24) & 0xFF;

  return (out_a << 24) | ((u8)blended_r << 16) | ((u8)blended_g << 8) |
         (u8)blended_b;
}

void cp_img(tex_t *pt, const char *path) {
  // Load image (force RGBA for consistent format)
  int req_channels = 4; // Always load as RGBA
  u8 *tmp = stbi_load(path, &pt->size.width, &pt->size.height, &pt->channels,
                      req_channels);

  ASSERT(tmp != NULL, "Failed to load image: %s", path);

  // Allocate memory for converted pixels
  usize pixel_count = pt->size.width * pt->size.height;
  pt->pixels = (u32 *)malloc(pixel_count * sizeof(u32));
  ASSERT(pt->pixels != NULL, "Failed to allocate texture memory");

  // Convert RGBA u8 to ARGB u32
  // Since we forced 4 channels, we know channels == 4
  for (usize i = 0; i < pixel_count; ++i) {
    usize base = i * 4; // 4 bytes per pixel (RGBA)
    u8 r = tmp[base + 0];
    u8 g = tmp[base + 1];
    u8 b = tmp[base + 2];
    u8 a = tmp[base + 3];
    pt->pixels[i] = rgba_to_u32(r, g, b, a);
  }

  // Free the temporary STB data
  stbi_image_free(tmp);
}

void kill_img(tex_t *pt) {
  if (pt->pixels) {
    free(pt->pixels);
    pt->pixels = NULL;
  }
}

usize vec_to_idx(u2 p, s2 prop) {
  // Remove silent error - better to crash on invalid access
  return p.y * prop.width + p.x;
}

void put_pixel(u2 p, u32 col, screen_t *ps) {
  // Add bounds check here instead
  if (p.x < ps->properties.width && p.y < ps->properties.height) {
    ps->pixels[vec_to_idx(p, ps->properties)] = col;
  }
}

void put_pixel_blend(u2 p, u32 col, screen_t *ps) {
  // Add bounds check here instead
  if (p.x < ps->properties.width && p.y < ps->properties.height) {
    usize idx = vec_to_idx(p, ps->properties);
    ps->pixels[idx] = alpha_blend_fast(col, ps->pixels[idx]);
  }
}

void draw_img(u2 pos, tex_t *pt, screen_t *ps) {
  ASSERT(pt->pixels != NULL, "Texture not loaded\n");

  // Calculate drawing region with clipping
  u32 start_x = pos.x;
  u32 start_y = pos.y;
  u32 end_x = pos.x + pt->size.width;
  u32 end_y = pos.y + pt->size.height;

  // Clamp to screen bounds (prevent out-of-bounds access)
  if (start_x >= ps->properties.width || start_y >= ps->properties.height)
    return; // Completely off-screen

  if (end_x > ps->properties.width)
    end_x = ps->properties.width;
  if (end_y > ps->properties.height)
    end_y = ps->properties.height;

  // Adjust start if partially off-screen left/top
  if (start_x < 0)
    start_x = 0;
  if (start_y < 0)
    start_y = 0;

  // Calculate source offsets
  u32 src_start_x = start_x - pos.x;
  u32 src_start_y = start_y - pos.y;

  // Draw with alpha blending instead of memcpy
  for (u32 y = start_y, src_y = src_start_y; y < end_y; ++y, ++src_y) {
    for (u32 x = start_x, src_x = src_start_x; x < end_x; ++x, ++src_x) {
      usize src_idx = src_y * pt->size.width + src_x;
      u32 src_pixel = pt->pixels[src_idx];

      // Skip completely transparent pixels for optimization
      if ((src_pixel >> 24) == 0)
        continue;

      usize dst_idx = y * ps->properties.width + x;
      ps->pixels[dst_idx] = alpha_blend_fast(src_pixel, ps->pixels[dst_idx]);
    }
  }
}

tex_t downscale_img(int times, tex_t t) {
  // Create new texture for downscaled result
  tex_t result;
  result.size.width = t.size.width / times;
  result.size.height = t.size.height / times;
  result.channels = t.channels;
  result.pixels =
      (u32 *)malloc(result.size.width * result.size.height * sizeof(u32));

  for (int x = 0; x < result.size.width; ++x) {
    for (int y = 0; y < result.size.height; ++y) {
      u64 r_sum = 0, g_sum = 0, b_sum = 0, a_sum = 0;

      // Average a block of times×times pixels
      for (int i = 0; i < times; ++i) {
        for (int j = 0; j < times; ++j) {
          int src_x = x * times + i;
          int src_y = y * times + j;

          // Use original texture size for source coordinates
          usize idx = src_y * t.size.width + src_x;
          u32 pixel = t.pixels[idx];

          // Extract color components
          u8 r = (pixel >> 16) & 0xFF;
          u8 g = (pixel >> 8) & 0xFF;
          u8 b = pixel & 0xFF;
          u8 a = (pixel >> 24) & 0xFF;

          r_sum += r;
          g_sum += g;
          b_sum += b;
          a_sum += a;
        }
      }

      // Calculate average
      u32 pixel_count = times * times;
      u8 r_avg = r_sum / pixel_count;
      u8 g_avg = g_sum / pixel_count;
      u8 b_avg = b_sum / pixel_count;
      u8 a_avg = a_sum / pixel_count;

      // Combine back to ARGB format
      usize dst_idx = y * result.size.width + x;
      result.pixels[dst_idx] =
          (a_avg << 24) | (r_avg << 16) | (g_avg << 8) | b_avg;
    }
  }

  return result;
}

typedef struct {
  const char *icon; // png

} app_t;

typedef struct {
  app_t *handle;
  usize size;

  u32 cols;
  u32 rows;
} apps_t;

void draw_bg(screen_t *ps) {
  const char *path = "./res/gfx/bg.png";

  tex_t bg = {0};

  cp_img(&bg, path);
  draw_img((u2){0, 0}, &bg, ps);
}

apps_t apps = {0};
tex_t brew = {0}; // Zero initialize!

tex_t tile = {0};

void init_apps() {
  int gc = 6;
  apps.handle = malloc(gc * sizeof(app_t));
  ASSERT(apps.handle, "apps");
  apps.size = gc;

  apps.rows = 3;
  apps.cols = 5;
}

void init_tiles() {
  const char *path = "./res/gfx/tile.png";
  cp_img(&tile, path);
}

void draw_tile(int x, int y, screen_t *ps) { draw_img((u2){x, y}, &tile, ps); }

static inline int clampi(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

static inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

void draw_tile_shadow(u2 pos, s2 size, screen_t *ps) {
  const int radius = 12;
  const int shadow_offset = 6;

  for (int y = 0; y < size.height; ++y) {
    for (int x = 0; x < size.width; ++x) {

      int dx = 0, dy = 0;

      if (x < radius)
        dx = radius - x;
      else if (x >= size.width - radius)
        dx = x - (size.width - radius - 1);

      if (y < radius)
        dy = radius - y;
      else if (y >= size.height - radius)
        dy = y - (size.height - radius - 1);

      if (dx * dx + dy * dy > radius * radius)
        continue;

      int px = pos.x + x;
      int py = pos.y + y + shadow_offset;

      if (px >= ps->properties.width || py >= ps->properties.height)
        continue;

      u8 a = 40; // soft shadow
      put_pixel_blend((u2){px, py}, rgba_to_u32(0, 0, 0, a), ps);
    }
  }
}

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

  // Load and draw
  cp_img(&brew, "./res/gfx/brew.png");
  brew = downscale_img(4, brew);

  draw_bg(ps);
  draw_tiles(ps);
  draw_img((u2){512, 512}, &brew, ps);
}

void loop(screen_t *ps, const bool *keys) {
  // Clear screen
  for (int i = 0; i < ps->properties.width * ps->properties.height; ++i) {
    ps->pixels[i] = 0xFFFFFFFF; // White background
  }

  draw_home(ps, keys);

  // Clean up
  kill_img(&brew);
}
