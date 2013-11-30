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
#include "app/commands/command.h"
#include "app/context.h"
#include "app/find_widget.h"
#include "app/load_widget.h"
#include "app/modules/editors.h"
#include "app/settings/document_settings.h"
#include "app/settings/settings.h"
#include "app/ui/color_button.h"
#include "app/ui_context.h"
#include "app/ui/status_bar.h"
#include "ui/window.h"

#include <allegro/unicode.h>

namespace app {

using namespace ui;
using namespace gfx;

class ShowGridCommand : public Command {
public:
  ShowGridCommand()
    : Command("ShowGrid",
              "Show Grid",
              CmdUIOnlyFlag)
  {
  }

  Command* clone() const { return new ShowGridCommand(*this); }

protected:
  bool onChecked(Context* context)
  {
    IDocumentSettings* docSettings = context->getSettings()->getDocumentSettings(context->getActiveDocument());

    return docSettings->getGridVisible();
  }

  void onExecute(Context* context)
  {
    IDocumentSettings* docSettings = context->getSettings()->getDocumentSettings(context->getActiveDocument());

    docSettings->setGridVisible(docSettings->getGridVisible() ? false: true);
  }
};

class SnapToGridCommand : public Command {
public:
  SnapToGridCommand()
    : Command("SnapToGrid",
              "Snap to Grid",
              CmdUIOnlyFlag)
  {
  }

  Command* clone() const { return new SnapToGridCommand(*this); }

protected:
  bool onChecked(Context* context)
  {
    IDocumentSettings* docSettings = context->getSettings()->getDocumentSettings(context->getActiveDocument());

    return docSettings->getSnapToGrid();
  }

  void onExecute(Context* context)
  {
    IDocumentSettings* docSettings = context->getSettings()->getDocumentSettings(context->getActiveDocument());
    char buf[512];

    docSettings->setSnapToGrid(docSettings->getSnapToGrid() ? false: true);

    usprintf(buf, "Snap to grid: %s",
             (docSettings->getSnapToGrid() ? "On": "Off"));

    StatusBar::instance()->setStatusText(250, buf);
  }
};

class GridSettingsCommand : public Command {
public:
  GridSettingsCommand();
  Command* clone() const { return new GridSettingsCommand(*this); }

protected:
  bool onEnabled(Context* context);
  void onExecute(Context* context);
};

GridSettingsCommand::GridSettingsCommand()
  : Command("GridSettings",
            "Grid Settings",
            CmdUIOnlyFlag)
{
}

bool GridSettingsCommand::onEnabled(Context* context)
{
  return true;
}

void GridSettingsCommand::onExecute(Context* context)
{
  base::UniquePtr<Window> window(app::load_widget<Window>("grid_settings.xml", "grid_settings"));
  Widget* button_ok = app::find_widget<Widget>(window, "ok");
  Widget* grid_x = app::find_widget<Widget>(window, "grid_x");
  Widget* grid_y = app::find_widget<Widget>(window, "grid_y");
  Widget* grid_w = app::find_widget<Widget>(window, "grid_w");
  Widget* grid_h = app::find_widget<Widget>(window, "grid_h");
  ColorButton* grid_color = app::find_widget<ColorButton>(window, "grid_color");
  Widget* grid_opacity = app::find_widget<Widget>(window, "grid_opacity");
  ColorButton* pixel_grid_color = app::find_widget<ColorButton>(window, "pixel_grid_color");
  Widget* pixel_grid_opacity = app::find_widget<Widget>(window, "pixel_grid_opacity");

  IDocumentSettings* docSettings = context->getSettings()->getDocumentSettings(context->getActiveDocument());
  Rect bounds = docSettings->getGridBounds();

  grid_x->setTextf("%d", bounds.x);
  grid_y->setTextf("%d", bounds.y);
  grid_w->setTextf("%d", bounds.w);
  grid_h->setTextf("%d", bounds.h);
  grid_color->setColor(docSettings->getGridColor());
  grid_opacity->setTextf("%d", docSettings->getGridOpacity());
  pixel_grid_color->setColor(docSettings->getPixelGridColor());
  pixel_grid_opacity->setTextf("%d", docSettings->getPixelGridOpacity());

  window->openWindowInForeground();

  if (window->getKiller() == button_ok) {
    bounds.x = grid_x->getTextInt();
    bounds.y = grid_y->getTextInt();
    bounds.w = grid_w->getTextInt();
    bounds.h = grid_h->getTextInt();
    bounds.w = MAX(bounds.w, 1);
    bounds.h = MAX(bounds.h, 1);

    docSettings->setGridBounds(bounds);
    docSettings->setGridColor(grid_color->getColor());
    docSettings->setGridOpacity(grid_opacity->getTextInt());
    docSettings->setPixelGridColor(pixel_grid_color->getColor());
    docSettings->setPixelGridOpacity(pixel_grid_opacity->getTextInt());
  }
}

Command* CommandFactory::createShowGridCommand()
{
  return new ShowGridCommand;
}

Command* CommandFactory::createSnapToGridCommand()
{
  return new SnapToGridCommand;
}

Command* CommandFactory::createGridSettingsCommand()
{
  return new GridSettingsCommand;
}

} // namespace app
