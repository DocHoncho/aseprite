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

#include <allegro/color.h>
#include <allegro/draw.h>
#include <allegro/gfx.h>

#include "app/modules/gfx.h"
#include "app/util/thmbnail.h"
#include "raster/blend.h"
#include "raster/cel.h"
#include "raster/image.h"
#include "raster/layer.h"
#include "raster/palette.h"
#include "raster/primitives.h"
#include "raster/sprite.h"
#include "raster/stock.h"

#define THUMBNAIL_W     32
#define THUMBNAIL_H     32

namespace app {

struct Thumbnail {
  const Cel *cel;
  BITMAP* bmp;

  Thumbnail(const Cel *cel, BITMAP* bmp)
    : cel(cel), bmp(bmp) {
  }

  ~Thumbnail() {
    destroy_bitmap(bmp);
  }
};

typedef std::vector<Thumbnail*> ThumbnailsList;

static ThumbnailsList* thumbnails = NULL;

static void thumbnail_render(BITMAP* bmp, const Image* image, bool has_alpha, const Palette* palette);

void destroy_thumbnails()
{
  if (thumbnails) {
    for (ThumbnailsList::iterator it = thumbnails->begin(); it != thumbnails->end(); ++it)
      delete *it;

    thumbnails->clear();
    delete thumbnails;
    thumbnails = NULL;
  }
}

BITMAP* generate_thumbnail(const Layer* layer, const Cel* cel, const Sprite *sprite)
{
  Thumbnail* thumbnail;
  BITMAP* bmp;

  if (!thumbnails)
    thumbnails = new ThumbnailsList();

  // Find the thumbnail
  for (ThumbnailsList::iterator it = thumbnails->begin(); it != thumbnails->end(); ++it) {
    thumbnail = *it;
    if (thumbnail->cel == cel)
      return thumbnail->bmp;
  }

  bmp = create_bitmap(THUMBNAIL_W, THUMBNAIL_H);
  if (!bmp)
    return NULL;

  thumbnail_render(bmp,
                   sprite->getStock()->getImage(cel->getImage()),
                   !layer->isBackground(),
                   sprite->getPalette(cel->getFrame()));

  thumbnail = new Thumbnail(cel, bmp);
  if (!thumbnail) {
    destroy_bitmap(bmp);
    return NULL;
  }

  thumbnails->push_back(thumbnail);
  return thumbnail->bmp;
}

static void thumbnail_render(BITMAP* bmp, const Image* image, bool has_alpha, const Palette* palette)
{
  register int c, x, y;
  int w, h, x1, y1;
  double sx, sy, scale;

  ASSERT(image != NULL);

  sx = (double)image->getWidth() / (double)bmp->w;
  sy = (double)image->getHeight() / (double)bmp->h;
  scale = MAX(sx, sy);

  w = image->getWidth() / scale;
  h = image->getHeight() / scale;
  w = MIN(bmp->w, w);
  h = MIN(bmp->h, h);

  x1 = bmp->w/2 - w/2;
  y1 = bmp->h/2 - h/2;
  x1 = MAX(0, x1);
  y1 = MAX(0, y1);

  /* with alpha blending */
  if (has_alpha) {
    register int c2;

    rectgrid(bmp, 0, 0, bmp->w-1, bmp->h-1,
             bmp->w/4, bmp->h/4);

    switch (image->getPixelFormat()) {
      case IMAGE_RGB:
        for (y=0; y<h; y++)
          for (x=0; x<w; x++) {
            c = get_pixel(image, x*scale, y*scale);
            c2 = getpixel(bmp, x1+x, y1+y);
            c = rgba_blend_normal(rgba(getr(c2), getg(c2), getb(c2), 255), c, 255);

            putpixel(bmp, x1+x, y1+y, makecol(rgba_getr(c),
                                              rgba_getg(c),
                                              rgba_getb(c)));
          }
        break;
      case IMAGE_GRAYSCALE:
        for (y=0; y<h; y++)
          for (x=0; x<w; x++) {
            c = get_pixel(image, x*scale, y*scale);
            c2 = getpixel(bmp, x1+x, y1+y);
            c = graya_blend_normal(graya(getr(c2), 255), c, 255);

            putpixel(bmp, x1+x, y1+y, makecol(graya_getv(c),
                                              graya_getv(c),
                                              graya_getv(c)));
          }
        break;
      case IMAGE_INDEXED: {
        for (y=0; y<h; y++)
          for (x=0; x<w; x++) {
            c = get_pixel(image, x*scale, y*scale);
            if (c != 0) {
              ASSERT(c >= 0 && c < palette->size());

              c = palette->getEntry(MID(0, c, palette->size()-1));
              putpixel(bmp, x1+x, y1+y, makecol(rgba_getr(c),
                                                rgba_getg(c),
                                                rgba_getb(c)));
            }
          }
        break;
      }
    }
  }
  /* without alpha blending */
  else {
    clear_to_color(bmp, makecol(128, 128, 128));

    switch (image->getPixelFormat()) {
      case IMAGE_RGB:
        for (y=0; y<h; y++)
          for (x=0; x<w; x++) {
            c = get_pixel(image, x*scale, y*scale);
            putpixel(bmp, x1+x, y1+y, makecol(rgba_getr(c),
                                              rgba_getg(c),
                                              rgba_getb(c)));
          }
        break;
      case IMAGE_GRAYSCALE:
        for (y=0; y<h; y++)
          for (x=0; x<w; x++) {
            c = get_pixel(image, x*scale, y*scale);
            putpixel(bmp, x1+x, y1+y, makecol(graya_getv(c),
                                              graya_getv(c),
                                              graya_getv(c)));
          }
        break;
      case IMAGE_INDEXED: {
        for (y=0; y<h; y++)
          for (x=0; x<w; x++) {
            c = get_pixel(image, x*scale, y*scale);

            ASSERT(c >= 0 && c < palette->size());

            c = palette->getEntry(MID(0, c, palette->size()-1));
            putpixel(bmp, x1+x, y1+y, makecol(rgba_getr(c),
                                              rgba_getg(c),
                                              rgba_getb(c)));
          }
        break;
      }
    }
  }
}

} // namespace app
