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

#include "app/tools/ink_processing.h"

#include "app/app.h"                // TODO avoid to include this file
#include "app/color_utils.h"
#include "app/context.h"
#include "app/document.h"
#include "app/document_undo.h"
#include "app/settings/settings.h"
#include "app/undoers/set_mask.h"
#include "raster/mask.h"

namespace app {
namespace tools {

// Ink used for tools which paint with primary/secondary
// (or foreground/background colors)
class PaintInk : public Ink {
public:
  enum Type { Merge, WithFg, WithBg, Opaque, PutAlpha, Randomized };

private:
  AlgoHLine m_proc;
  Type m_type;

public:
  PaintInk(Type type) : m_type(type) { }

  bool isPaint() const { return true; }

  void prepareInk(ToolLoop* loop)
  {
    switch (m_type) {

      case Merge:
        // Do nothing, use the default colors
        break;

      case WithFg:
      case WithBg:
        {
          int color = color_utils::color_for_layer(m_type == WithFg ?
                                                   loop->getSettings()->getFgColor():
                                                   loop->getSettings()->getBgColor(),
                                                   loop->getLayer());
          loop->setPrimaryColor(color);
          loop->setSecondaryColor(color);
        }
        break;
    }

    int depth = MID(0, loop->getSprite()->getPixelFormat(), 2);

    switch (m_type) {
      case Opaque:
        m_proc = ink_processing[INK_OPAQUE][depth];
        break;
      case PutAlpha:
        m_proc = ink_processing[INK_PUTALPHA][depth];
        break;
      case Randomized:
        m_proc = ink_processing[INK_RANDOMIZED][depth];
        break;
      default:
        m_proc = (loop->getOpacity() == 255 ?
                  ink_processing[INK_OPAQUE][depth]:
                  ink_processing[INK_TRANSPARENT][depth]);
        break;
    }
  }

  void inkHline(int x1, int y, int x2, ToolLoop* loop)
  {
    (*m_proc)(x1, y, x2, loop);
  }
};


class ShadingInk : public Ink {
private:
  AlgoHLine m_proc;

public:
  ShadingInk() { }

  bool isPaint() const { return true; }

  void prepareInk(ToolLoop* loop)
  {
    m_proc = ink_processing[INK_SHADING][MID(0, loop->getSprite()->getPixelFormat(), 2)];
  }

  void inkHline(int x1, int y, int x2, ToolLoop* loop)
  {
    (*m_proc)(x1, y, x2, loop);
  }
};


class PickInk : public Ink {
public:
  enum Target { Fg, Bg };

private:
  Target m_target;

public:
  PickInk(Target target) : m_target(target) { }

  bool isEyedropper() const { return true; }

  void prepareInk(ToolLoop* loop)
  {
    // Do nothing
  }

  void inkHline(int x1, int y, int x2, ToolLoop* loop)
  {
    // Do nothing
  }

};


class ScrollInk : public Ink {
public:

  bool isScrollMovement() const { return true; }

  void prepareInk(ToolLoop* loop)
  {
    // Do nothing
  }

  void inkHline(int x1, int y, int x2, ToolLoop* loop)
  {
    // Do nothing
  }

};


class MoveInk : public Ink {
public:
  bool isCelMovement() const { return true; }

  void prepareInk(ToolLoop* loop)
  {
    // Do nothing
  }

  void inkHline(int x1, int y, int x2, ToolLoop* loop)
  {
    // Do nothing
  }
};


class EraserInk : public Ink {
public:
  enum Type { Eraser, ReplaceFgWithBg, ReplaceBgWithFg };

private:
  AlgoHLine m_proc;
  Type m_type;

public:
  EraserInk(Type type) : m_type(type) { }

  bool isPaint() const { return true; }
  bool isEffect() const { return true; }

  void prepareInk(ToolLoop* loop)
  {
    switch (m_type) {

      case Eraser:
        m_proc = ink_processing[INK_OPAQUE][MID(0, loop->getSprite()->getPixelFormat(), 2)];

        // TODO app_get_color_to_clear_layer should receive the context as parameter
        loop->setPrimaryColor(app_get_color_to_clear_layer(loop->getLayer()));
        loop->setSecondaryColor(app_get_color_to_clear_layer(loop->getLayer()));
        break;

      case ReplaceFgWithBg:
        m_proc = ink_processing[INK_REPLACE][MID(0, loop->getSprite()->getPixelFormat(), 2)];

        loop->setPrimaryColor(color_utils::color_for_layer(loop->getSettings()->getFgColor(),
                                                           loop->getLayer()));
        loop->setSecondaryColor(color_utils::color_for_layer(loop->getSettings()->getBgColor(),
                                                             loop->getLayer()));
        break;

      case ReplaceBgWithFg:
        m_proc = ink_processing[INK_REPLACE][MID(0, loop->getSprite()->getPixelFormat(), 2)];

        loop->setPrimaryColor(color_utils::color_for_layer(loop->getSettings()->getBgColor(),
                                                           loop->getLayer()));
        loop->setSecondaryColor(color_utils::color_for_layer(loop->getSettings()->getFgColor(),
                                                             loop->getLayer()));
        break;
    }
  }

  void inkHline(int x1, int y, int x2, ToolLoop* loop)
  {
    (*m_proc)(x1, y, x2, loop);
  }
};


class BlurInk : public Ink {
  AlgoHLine m_proc;

public:
  bool isPaint() const { return true; }
  bool isEffect() const { return true; }

  void prepareInk(ToolLoop* loop)
  {
    m_proc = ink_processing[INK_BLUR][MID(0, loop->getSprite()->getPixelFormat(), 2)];
  }

  void inkHline(int x1, int y, int x2, ToolLoop* loop)
  {
    (*m_proc)(x1, y, x2, loop);
  }
};


class JumbleInk : public Ink {
  AlgoHLine m_proc;

public:
  bool isPaint() const { return true; }
  bool isEffect() const { return true; }

  void prepareInk(ToolLoop* loop)
  {
    m_proc = ink_processing[INK_JUMBLE][MID(0, loop->getSprite()->getPixelFormat(), 2)];
  }

  void inkHline(int x1, int y, int x2, ToolLoop* loop)
  {
    (*m_proc)(x1, y, x2, loop);
  }
};


// Ink used for selection tools (like Rectangle Marquee, Lasso, Magic Wand, etc.)
class SelectionInk : public Ink {
  bool m_modify_selection;

public:
  SelectionInk() { m_modify_selection = false; }

  bool isSelection() const { return true; }

  void inkHline(int x1, int y, int x2, ToolLoop* loop)
  {
    if (m_modify_selection) {
      Point offset = loop->getOffset();

      if (loop->getMouseButton() == ToolLoop::Left)
        loop->getMask()->add(x1-offset.x, y-offset.y, x2-x1+1, 1);
      else if (loop->getMouseButton() == ToolLoop::Right)
        loop->getMask()->subtract(x1-offset.x, y-offset.y, x2-x1+1, 1);
    }
    else
      draw_hline(loop->getDstImage(), x1, y, x2, loop->getPrimaryColor());
  }

  void setFinalStep(ToolLoop* loop, bool state)
  {
    m_modify_selection = state;

    if (state) {
      DocumentUndo* undo = loop->getDocument()->getUndo();
      if (undo->isEnabled())
        undo->pushUndoer(new undoers::SetMask(undo->getObjects(), loop->getDocument()));

      loop->getMask()->freeze();
      loop->getMask()->reserve(0, 0, loop->getSprite()->getWidth(), loop->getSprite()->getHeight());
    }
    else {
      loop->getMask()->unfreeze();
      loop->getDocument()->setTransformation(Transformation(loop->getMask()->getBounds()));
      loop->getDocument()->setMaskVisible(true);
    }
  }

};


} // namespace tools
} // namespace app
