/* ase -- allegro-sprite-editor: the ultimate sprites factory
 * Copyright (C) 2007  David A. Capello
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

#include "config.h"

#ifndef USE_PRECOMPILED_HEADER

#include <allegro.h>

#include "jinete.h"

#include "core/app.h"
#include "core/cfg.h"
#include "modules/gui.h"
#include "modules/sprites.h"
#include "raster/sprite.h"

#endif

bool command_enabled_duplicate_sprite(const char *argument)
{
  return current_sprite != NULL;
}

void command_execute_duplicate_sprite(const char *argument)
{
  JWidget window, src_name, dst_name, flatten;
  Sprite *sprite = current_sprite;
  Sprite *sprite_copy;
  char buf[1024];

  /* load the window widget */
  window = load_widget("dupspr.jid", "duplicate_sprite");
  if (!window)
    return;

  src_name = jwidget_find_name(window, "src_name");
  dst_name = jwidget_find_name(window, "dst_name");
  flatten = jwidget_find_name(window, "flatten");

  jwidget_set_text(src_name, get_filename(sprite->filename));

  sprintf(buf, "%s %s", sprite->filename, _("Copy"));
  jwidget_set_text(dst_name, buf);

  if (get_config_bool("DuplicateSprite", "Flatten", FALSE))
    jwidget_select(flatten);

  /* open the window */
  jwindow_open_fg(window);

  if (jwindow_get_killer(window) == jwidget_find_name(window, "ok")) {
    set_config_bool("DuplicateSprite", "Flatten",
		    jwidget_is_selected(flatten));
    
    if (jwidget_is_selected(flatten))
      sprite_copy = sprite_new_flatten_copy(sprite);
    else
      sprite_copy = sprite_new_copy(sprite);

    if (sprite_copy != NULL) {
      sprite_set_filename(sprite_copy, jwidget_get_text(dst_name));

      sprite_mount(sprite_copy);
      set_current_sprite(sprite_copy);
      sprite_show(sprite_copy);
    }
  }

  jwidget_free(window);
}