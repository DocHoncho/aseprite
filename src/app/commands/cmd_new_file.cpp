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

#include "app/app.h"
#include "app/color.h"
#include "app/color_utils.h"
#include "app/commands/command.h"
#include "app/console.h"
#include "app/document.h"
#include "app/find_widget.h"
#include "app/ini_file.h"
#include "app/load_widget.h"
#include "app/modules/editors.h"
#include "app/modules/palettes.h"
#include "app/ui/color_bar.h"
#include "app/ui/workspace.h"
#include "app/ui_context.h"
#include "app/util/clipboard.h"
#include "base/unique_ptr.h"
#include "raster/cel.h"
#include "raster/image.h"
#include "raster/layer.h"
#include "raster/palette.h"
#include "raster/primitives.h"
#include "raster/sprite.h"
#include "raster/stock.h"
#include "ui/ui.h"

#include <allegro/config.h>
#include <allegro/unicode.h>

using namespace ui;

namespace app {

class NewFileCommand : public Command {
public:
  NewFileCommand();
  Command* clone() { return new NewFileCommand(*this); }

protected:
  void onExecute(Context* context);
};

static int _sprite_counter = 0;

NewFileCommand::NewFileCommand()
  : Command("NewFile",
            "New File",
            CmdRecordableFlag)
{
}

/**
 * Shows the "New Sprite" dialog.
 */
void NewFileCommand::onExecute(Context* context)
{
  PixelFormat format;
  int w, h, bg, ncolors;
  char buf[1024];
  app::Color bg_table[] = {
    app::Color::fromMask(),
    app::Color::fromRgb(0, 0, 0),
    app::Color::fromRgb(255, 255, 255),
    app::Color::fromRgb(255, 0, 255),
    ColorBar::instance()->getBgColor()
  };

  // Load the window widget
  base::UniquePtr<Window> window(app::load_widget<Window>("new_sprite.xml", "new_sprite"));
  Widget* width = app::find_widget<Widget>(window, "width");
  Widget* height = app::find_widget<Widget>(window, "height");
  Widget* radio1 = app::find_widget<Widget>(window, "radio1");
  Widget* radio2 = app::find_widget<Widget>(window, "radio2");
  Widget* radio3 = app::find_widget<Widget>(window, "radio3");
  Widget* colors = app::find_widget<Widget>(window, "colors");
  ListBox* bg_box = app::find_widget<ListBox>(window, "bg_box");
  Widget* ok = app::find_widget<Widget>(window, "ok_button");

  // Default values: Indexed, 320x240, Background color
  format = static_cast<PixelFormat>(get_config_int("NewSprite", "Type", IMAGE_INDEXED));
  // Invalid format in config file.
  if (format != IMAGE_RGB && format != IMAGE_INDEXED && format != IMAGE_GRAYSCALE) {
    format = IMAGE_INDEXED;
  }
  w = get_config_int("NewSprite", "Width", 320);
  h = get_config_int("NewSprite", "Height", 240);
  bg = get_config_int("NewSprite", "Background", 4); // Default = Background color
  ncolors = get_config_int("NewSprite", "Colors", 256);

  // If the clipboard contains an image, we can show the size of the
  // clipboard as default image size.
  gfx::Size clipboardSize;
  if (clipboard::get_image_size(clipboardSize)) {
    w = clipboardSize.w;
    h = clipboardSize.h;
  }

  width->setTextf("%d", MAX(1, w));
  height->setTextf("%d", MAX(1, h));
  colors->setTextf("%d", MID(2, ncolors, 256));

  // Select image-type
  switch (format) {
    case IMAGE_RGB:       radio1->setSelected(true); break;
    case IMAGE_GRAYSCALE: radio2->setSelected(true); break;
    case IMAGE_INDEXED:   radio3->setSelected(true); break;
  }

  // Select background color
  bg_box->selectIndex(bg);

  // Open the window
  window->openWindowInForeground();

  if (window->getKiller() == ok) {
    bool ok = false;

    // Get the options
    if (radio1->isSelected())      format = IMAGE_RGB;
    else if (radio2->isSelected()) format = IMAGE_GRAYSCALE;
    else if (radio3->isSelected()) format = IMAGE_INDEXED;

    w = width->getTextInt();
    h = height->getTextInt();
    ncolors = colors->getTextInt();
    bg = bg_box->getSelectedIndex();

    w = MID(1, w, 65535);
    h = MID(1, h, 65535);
    ncolors = MID(2, ncolors, 256);

    // Select the color
    app::Color color = app::Color::fromMask();

    if (bg >= 0 && bg <= 4) {
      color = bg_table[bg];
      ok = true;
    }

    if (ok) {
      // Save the configuration
      set_config_int("NewSprite", "Type", format);
      set_config_int("NewSprite", "Width", w);
      set_config_int("NewSprite", "Height", h);
      set_config_int("NewSprite", "Background", bg);

      // Create the new sprite
      ASSERT(format == IMAGE_RGB || format == IMAGE_GRAYSCALE || format == IMAGE_INDEXED);
      ASSERT(w > 0 && h > 0);

      base::UniquePtr<Document> document(
        Document::createBasicDocument(format, w, h,
                                      (format == IMAGE_INDEXED ? ncolors: 256)));
      Sprite* sprite(document->getSprite());

      get_default_palette()->copyColorsTo(sprite->getPalette(FrameNumber(0)));

      usprintf(buf, "Sprite-%04d", ++_sprite_counter);
      document->setFilename(buf);

      // If the background color isn't transparent, we have to
      // convert the `Layer 1' in a `Background'
      if (color.getType() != app::Color::MaskType) {
        Sprite* sprite = document->getSprite();
        Layer* layer = sprite->getFolder()->getFirstLayer();

        if (layer && layer->isImage()) {
          LayerImage* layerImage = static_cast<LayerImage*>(layer);
          layerImage->configureAsBackground();

          Image* image = sprite->getStock()->getImage(layerImage->getCel(FrameNumber(0))->getImage());
          raster::clear_image(image, color_utils::color_for_image(color, format));
        }
      }

      // Show the sprite to the user
      context->addDocument(document.release());
    }
  }
}

Command* CommandFactory::createNewFileCommand()
{
  return new NewFileCommand;
}

} // namespace app
