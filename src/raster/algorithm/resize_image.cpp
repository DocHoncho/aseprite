/* Aseprite
 * Copyright (C) 2001-2013  David Capello
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "raster/algorithm/resize_image.h"

#include "raster/image.h"
#include "raster/image_bits.h"
#include "raster/palette.h"
#include "raster/rgbmap.h"

namespace raster {
namespace algorithm {

//! Basic linear interpolation
/*!
  \param t Position to interpolate at
  \param v0, v1 Data to interpolate
*/
inline double lerp(double t, double v0, double v1) { return v0 * (1 - t) + v1 * t; }

//! Basic linear interpolation of RGBA data
/*!
  \param t Position to interpolate at
  \param v0, v1 Color data to interpolate
  \sa lerp()
*/
inline color_t lerp_rgba(double t, color_t v0, color_t v1)
{
  return rgba(
    (int)lerp(t, rgba_getr(v0), rgba_getr(v1)),
    (int)lerp(t, rgba_getg(v0), rgba_getg(v1)),
    (int)lerp(t, rgba_getb(v0), rgba_getb(v1)),
    (int)lerp(t, rgba_geta(v0), rgba_geta(v1)));
}

/*! Basic linear interpolation of grayscale w/ alpha data
  \param t Position to interpolate at
  \param v0, v0 Color data to interpolate
*/
inline color_t lerp_graya(double t, color_t v0, color_t v1)
{
  return graya(
    lerp(t, graya_getv(v0), graya_getv(v1)),
    lerp(t, graya_geta(v0), graya_geta(v1)));
}

//! Basic linear interpolation of RGB color data
/*!
  \param t Position to interpolate at
  \param v0, v1 Color data to interpolate
  \sa lerp()

  Assumes 0 is transparent color, all other values are treated as fully opaque (255).
*/
inline color_t lerp_rgb(double t, color_t v0, color_t v1)
{
  return rgba(
    (int)lerp(t, rgba_getr(v0), rgba_getr(v1)),
    (int)lerp(t, rgba_getg(v0), rgba_getg(v1)),
    (int)lerp(t, rgba_getb(v0), rgba_getb(v1)),
    (int)lerp(t, (v0 == 0 ? 0 : 255), (v1 == 0 ? 0 : 255)));
}

//! Perform bilinear interpolation of RGBA colors
/*!
  \param x, y Position to interpolate at
  \param color_data Array of color_t values to interpolate
*/
inline color_t bilerp_rgba(double x, double y, color_t color_data[4])
{
  return lerp_rgba(y, lerp_rgba(x, color_data[0], color_data[1]), lerp_rgba(x, color_data[2], color_data[3]));
}

//! Perform bilinear interpolation of graya colors
/*!
  \param x, y Position to interpolate at
  \param color_data Array of color_t values to interpolate
*/
inline color_t bilerp_graya(double x, double y, color_t color_data[4])
{
  return lerp_graya(y, lerp_graya(x, color_data[0], color_data[1]), lerp_graya(x, color_data[2], color_data[2]));
}

//! Perform bilinear interpolation of RGB colors.
/*!
  \param x, y Position to interpolate at
  \param color_data Array of color_t values to interpolate

  The primary difference between this and bilerp_rgba is it handles alpha channel differently,
  assuming alpha=255 unless the color is transparent (0)

  The color_data array should be regular RGBA (color_t) values, caller is responsible for remapping result.
*/
inline color_t bilerp_rgb(double x, double y, color_t color_data[4])
{
  return lerp_rgb(y, lerp_rgb(x, color_data[0], color_data[1]), lerp_rgb(x, color_data[2], color_data[3]));
}

void resize_image(const Image* src, Image* dst, ResizeMethod method, const Palette* pal, const RgbMap* rgbmap)
{
  switch (method) {

    // TODO optimize this
    case RESIZE_METHOD_NEAREST_NEIGHBOR:
    {
      int o_width = src->getWidth(), o_height = src->getHeight();
      int n_width = dst->getWidth(), n_height = dst->getHeight();
      double x_ratio = o_width / (double)n_width;
      double y_ratio = o_height / (double)n_height;
      double px, py;
      int i;

      for (int y = 0; y < n_height; y++) {
        for (int x = 0; x < n_width; x++) {
          px = floor(x * x_ratio);
          py = floor(y * y_ratio);
          i = (int)(py * o_width + px);
          dst->putPixel(x, y, src->getPixel(i % o_width, i / o_width));
        }
      }
      break;
    }

    // TODO optimize this
    case RESIZE_METHOD_BILINEAR: {
      int o_width = src->getWidth(), o_height = src->getHeight();
      int n_width = dst->getWidth(), n_height = dst->getHeight();
      color_t color_data[4];
      double n_px, n_py;
      double s_px, s_py;
      int s_px0, s_py0, s_px1, s_py1;
      color_t c;

      for (int y = 0; y < n_height; y++) {
        for (int x = 0; x < n_width; x++) {
          // Find current point in new image as a percentage
          n_px = (0.5 + x) / n_width;
          n_py = (0.5 + y) / n_height;
          // Calculate position in source image
          s_px = n_px * o_width - 0.5;
          s_py = n_py * o_height - 0.5;

          if (s_px < 0.0) s_px = 0.0;
          if (s_py < 0.0) s_py = 0.0;

          // Calculate coordinates for pixels from source image
          s_px0 = floor(s_px);
          s_py0 = floor(s_py);

          if (s_px0 >= o_width - 1)
            s_px0 = s_px1 = o_width - 1;
          else
            s_px1 = s_px0 + 1;

          if (s_py0 >= o_height - 1)
            s_py0 = s_py1 = o_height - 1;
          else
            s_py1 = s_py0 + 1;

          color_data[0] = src->getPixel(s_px0, s_py0);
          color_data[1] = src->getPixel(s_px1, s_py0);
          color_data[2] = src->getPixel(s_px0, s_py1);
          color_data[3] = src->getPixel(s_px1, s_py1);

          switch (dst->getPixelFormat()) {
          case IMAGE_RGB:
            c = bilerp_rgba(s_px - s_px0, s_py - s_py0, color_data);
            break;
          case IMAGE_GRAYSCALE:
            c = bilerp_graya(s_px - s_px0, s_py - s_py0, color_data);
            break;
          case IMAGE_INDEXED:
            // Grab actual RGBA values from the palette
            color_data[0] = pal->getEntry(src->getPixel(s_px0, s_py0));
            color_data[1] = pal->getEntry(src->getPixel(s_px1, s_py0));
            color_data[2] = pal->getEntry(src->getPixel(s_px0, s_py1));
            color_data[3] = pal->getEntry(src->getPixel(s_px1, s_py1));

            // Interpolate, then remap color
            c = bilerp_rgb(s_px - s_px0, s_py - s_py0, color_data);
            c = rgba_geta(c) > 127 ? rgbmap->mapColor(rgba_getr(c),
                                                      rgba_getg(c),
                                                      rgba_getb(c)) : 0;
          }

          dst->putPixel(x, y, c);
        }
      }
    }
    break;
  }
}

void fixup_image_transparent_colors(Image* image)
{
  int x, y, u, v;

  switch (image->getPixelFormat()) {

    case IMAGE_RGB: {
      int r, g, b, count;
      LockImageBits<RgbTraits> bits(image);
      LockImageBits<RgbTraits>::iterator it = bits.begin();

      for (y=0; y<image->getHeight(); ++y) {
        for (x=0; x<image->getWidth(); ++x, ++it) {
          uint32_t c = *it;

          // if this is a completelly-transparent pixel...
          if (rgba_geta(c) == 0) {
            count = 0;
            r = g = b = 0;

            LockImageBits<RgbTraits>::iterator it2 =
              bits.begin_area(gfx::Rect(x-1, y-1, 3, 3));

            for (v=y-1; v<=y+1; ++v) {
              for (u=x-1; u<=x+1; ++u, ++it2) {
                if ((u >= 0) && (v >= 0) && (u < image->getWidth()) && (v < image->getHeight())) {
                  c = *it2;
                  if (rgba_geta(c) > 0) {
                    r += rgba_getr(c);
                    g += rgba_getg(c);
                    b += rgba_getb(c);
                    ++count;
                  }
                }
              }
            }

            if (count > 0) {
              r /= count;
              g /= count;
              b /= count;
              *it = rgba(r, g, b, 0);
            }
          }
        }
      }
      break;
    }

    case IMAGE_GRAYSCALE: {
      int k, count;
      LockImageBits<GrayscaleTraits> bits(image);
      LockImageBits<GrayscaleTraits>::iterator it = bits.begin();

      for (y=0; y<image->getHeight(); ++y) {
        for (x=0; x<image->getWidth(); ++x, ++it) {
          uint16_t c = *it;

          // If this is a completelly-transparent pixel...
          if (graya_geta(c) == 0) {
            count = 0;
            k = 0;

            LockImageBits<GrayscaleTraits>::iterator it2 =
              bits.begin_area(gfx::Rect(x-1, y-1, 3, 3));

            for (v=y-1; v<=y+1; ++v) {
              for (u=x-1; u<=x+1; ++u, ++it2) {
                if ((u >= 0) && (v >= 0) && (u < image->getWidth()) && (v < image->getHeight())) {
                  c = *it2;
                  if (graya_geta(c) > 0) {
                    k += graya_getv(c);
                    ++count;
                  }
                }
              }
            }

            if (count > 0) {
              k /= count;
              *it = graya(k, 0);
            }
          }
        }
      }
      break;
    }

  }
}

} // namespace algorithm
} // namespace raster
