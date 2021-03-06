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

#include "app/app_menus.h"
#include "app/commands/command.h"
#include "app/commands/commands.h"
#include "app/commands/params.h"
#include "app/console.h"
#include "app/context_access.h"
#include "app/document.h"
#include "app/document_api.h"
#include "app/document_event.h"
#include "app/document_observer.h"
#include "app/document_undo.h"
#include "app/modules/editors.h"
#include "app/modules/gfx.h"
#include "app/modules/gui.h"
#include "app/ui/editor/editor.h"
#include "app/ui/skin/skin_theme.h"
#include "app/ui_context.h"
#include "app/undo_transaction.h"
#include "app/util/celmove.h"
#include "app/util/thmbnail.h"
#include "base/compiler_specific.h"
#include "base/memory.h"
#include "gfx/point.h"
#include "gfx/rect.h"
#include "raster/raster.h"
#include "ui/ui.h"

#include <allegro.h>
#include <vector>

/*
   Animator Editor

                        Frames ...

   --------------------+-----+-----+-----+---
            Layers     |msecs|msecs|msecs|...
   --------------------+-----+-----+-----+---
    [1] [2] Layer 1    | Cel | Cel | Cel |...
   --------------------+-----+-----+-----+---
    [1] [2] Background | Cel | Cel | Cel |...
   --------------------+-----+-----+-----+---

   [1] Eye-icon
   [2] Padlock-icon
 */

// Size of the thumbnail in the screen (width x height), the really
// size of the thumbnail bitmap is specified in the
// 'generate_thumbnail' routine.
#define THUMBSIZE       (32*jguiscale())

// Height of the headers.
#define HDRSIZE         (3 + text_height(this->getFont())*2 + 3 + 3)

// Width of the frames.
#define FRMSIZE         (3 + THUMBSIZE + 3)

// Height of the layers.
#define LAYSIZE         (3 + MAX(text_height(this->getFont()), THUMBSIZE) + 4)

// Space between icons and other information in the layer.
#define ICONSEP         (2*jguiscale())

// Space between the icon-bitmap and the edge of the surrounding button.
#define ICONBORDER      (4*jguiscale())

namespace app {

using namespace app::skin;
using namespace gfx;
using namespace raster;
using namespace ui;

enum {
  A_PART_NOTHING,
  A_PART_SEPARATOR,
  A_PART_HEADER_LAYER,
  A_PART_HEADER_FRAME,
  A_PART_LAYER,
  A_PART_LAYER_EYE_ICON,
  A_PART_LAYER_LOCK_ICON,
  A_PART_CEL
};

class AnimationEditor : public Widget
                      , public DocumentObserver {
public:
  enum State {
    STATE_STANDBY,
    STATE_SCROLLING,
    STATE_MOVING_SEPARATOR,
    STATE_MOVING_LAYER,
    STATE_MOVING_CEL,
    STATE_MOVING_FRAME,
  };

  AnimationEditor(Context* context);
  ~AnimationEditor();

  Sprite* getSprite() { return m_sprite; }
  Layer* getLayer() { return m_layer; }
  FrameNumber getFrame() { return m_frame; }

  void setLayer(Layer* layer) {
    m_layer = layer;
    if (current_editor)
      current_editor->setLayer(m_layer);
  }
  void setFrame(FrameNumber frame) {
    m_frame = frame;
    if (current_editor)
      current_editor->setFrame(m_frame);
  }

  void centerCurrentCel();
  State getState() const { return m_state; }

protected:
  bool onProcessMessage(Message* msg) OVERRIDE;
  void onPreferredSize(PreferredSizeEvent& ev) OVERRIDE;

  // DocumentObserver impl.
  void onAddLayer(DocumentEvent& ev) OVERRIDE;
  void onRemoveLayer(DocumentEvent& ev) OVERRIDE;
  void onAddFrame(DocumentEvent& ev) OVERRIDE;
  void onRemoveFrame(DocumentEvent& ev) OVERRIDE;
  void onTotalFramesChanged(DocumentEvent& ev) OVERRIDE;

private:
  void setCursor(int x, int y);
  void getDrawableLayers(const gfx::Rect& clip, int* first_layer, int* last_layer);
  void getDrawableFrames(const gfx::Rect& clip, FrameNumber* first_frame, FrameNumber* last_frame);
  void drawHeader(const gfx::Rect& clip);
  void drawHeaderFrame(const gfx::Rect& clip, FrameNumber frame);
  void drawHeaderPart(const gfx::Rect& clip, int x1, int y1, int x2, int y2,
                      bool is_hot, bool is_clk,
                      const char* line1, int align1,
                      const char* line2, int align2);
  void drawSeparator(const gfx::Rect& clip);
  void drawLayer(const gfx::Rect& clip, int layer_index);
  void drawLayerPadding();
  void drawCel(const gfx::Rect& clip, int layer_index, FrameNumber frame, Cel* cel);
  bool drawPart(int part, int layer, FrameNumber frame);
  void regenerateLayers();
  void hotThis(int hot_part, int hot_layer, FrameNumber hotFrame);
  void centerCel(int layer, FrameNumber frame);
  void showCel(int layer, FrameNumber frame);
  void showCurrentCel();
  void cleanClk();
  void setScroll(int x, int y, bool use_refresh_region);
  int getLayerIndex(const Layer* layer);

  Context* m_context;
  Document* m_document;
  Sprite* m_sprite;
  Layer* m_layer;
  FrameNumber m_frame;
  State m_state;
  std::vector<Layer*> m_layers;
  int m_scroll_x;
  int m_scroll_y;
  int m_separator_x;
  int m_separator_w;
  // The 'hot' part is where the mouse is on top of
  int m_hot_part;
  int m_hot_layer;
  FrameNumber m_hot_frame;
  // The 'clk' part is where the mouse's button was pressed (maybe for a drag & drop operation)
  int m_clk_part;
  int m_clk_layer;
  FrameNumber m_clk_frame;
  // Keys
  bool m_space_pressed;
};

static AnimationEditor* current_anieditor = NULL;

static void icon_rect(BITMAP* icon_normal, BITMAP* icon_selected, int x1, int y1, int x2, int y2,
                      bool is_selected, bool is_hot, bool is_clk);

bool animation_editor_is_movingcel()
{
  return
    current_anieditor != NULL &&
    current_anieditor->getState() == AnimationEditor::STATE_MOVING_CEL;
}

// Shows the animation editor for the current sprite.
void switch_between_animation_and_sprite_editor(Context* context)
{
  const Document* document;
  const Sprite* sprite;
  {
    const ContextReader reader(context);
    document = reader.document();
    sprite = reader.sprite();
  }

  // Create the window & the animation-editor
  {
    base::UniquePtr<Window> window(new Window(true, ""));
    AnimationEditor anieditor(context);

    window->addChild(&anieditor);
    window->remapWindow();

    anieditor.centerCurrentCel();

    // Show the window
    window->openWindowInForeground();
  }

  // Destroy thumbnails
  destroy_thumbnails();
}

//////////////////////////////////////////////////////////////////////
// The Animation Editor

AnimationEditor::AnimationEditor(Context* context)
  : Widget(kGenericWidget)
  , m_context(context)
{
  DocumentLocation location = context->getActiveLocation();

  m_document = location.document();
  m_sprite = location.sprite();
  m_layer = location.layer();
  m_frame = location.frame();
  m_state = STATE_STANDBY;
  m_scroll_x = 0;
  m_scroll_y = 0;
  m_separator_x = 100 * jguiscale();
  m_separator_w = 1;
  m_hot_part = A_PART_NOTHING;
  m_clk_part = A_PART_NOTHING;
  m_space_pressed = false;

  this->setFocusStop(true);

  regenerateLayers();

  current_anieditor = this;
  m_document->addObserver(this);
}

AnimationEditor::~AnimationEditor()
{
  current_anieditor = NULL;
  m_document->removeObserver(this);
}

bool AnimationEditor::onProcessMessage(Message* msg)
{
  switch (msg->type()) {

    case kPaintMessage: {
      gfx::Rect clip = static_cast<PaintMessage*>(msg)->rect();
      int layer, first_layer, last_layer;
      FrameNumber frame, first_frame, last_frame;

      getDrawableLayers(clip, &first_layer, &last_layer);
      getDrawableFrames(clip, &first_frame, &last_frame);

      // Draw the header for layers.
      drawHeader(clip);

      // Draw the header for each visible frame.
      for (frame=first_frame; frame<=last_frame; ++frame)
        drawHeaderFrame(clip, frame);

      // Draw the separator.
      drawSeparator(clip);

      // Draw each visible layer.
      for (layer=first_layer; layer<=last_layer; layer++) {
        drawLayer(clip, layer);

        // Get the first CelIterator to be drawn (it is the first cel with cel->frame >= first_frame)
        CelIterator it, end;
        Layer* layerPtr = m_layers[layer];
        if (layerPtr->isImage()) {
          it = static_cast<LayerImage*>(layerPtr)->getCelBegin();
          end = static_cast<LayerImage*>(layerPtr)->getCelEnd();
          for (; it != end && (*it)->getFrame() < first_frame; ++it)
            ;
        }

        // Draw every visible cel for each layer.
        for (frame=first_frame; frame<=last_frame; ++frame) {
          Cel* cel = (layerPtr->isImage() && it != end && (*it)->getFrame() == frame ? *it: NULL);

          drawCel(clip, layer, frame, cel);

          if (cel)
            ++it;               // Go to next cel
        }
      }

      drawLayerPadding();

      return true;
    }

    case kTimerMessage:
      break;

    case kMouseEnterMessage:
      if (key[KEY_SPACE]) m_space_pressed = true;
      break;

    case kMouseLeaveMessage:
      if (m_space_pressed) m_space_pressed = false;
      break;

    case kMouseDownMessage:
      if (static_cast<MouseMessage*>(msg)->middle() || m_space_pressed) {
        captureMouse();
        m_state = STATE_SCROLLING;
        return true;
      }

      // Clicked-part = hot-part.
      m_clk_part = m_hot_part;
      m_clk_layer = m_hot_layer;
      m_clk_frame = m_hot_frame;

      switch (m_hot_part) {
        case A_PART_NOTHING:
          // Do nothing.
          break;
        case A_PART_SEPARATOR:
          captureMouse();
          m_state = STATE_MOVING_SEPARATOR;
          break;
        case A_PART_HEADER_LAYER:
          // Do nothing.
          break;
        case A_PART_HEADER_FRAME:
          setFrame(m_clk_frame);
          invalidate(); // TODO Replace this by redrawing old current frame and new current frame
          captureMouse();
          m_state = STATE_MOVING_FRAME;
          break;
        case A_PART_LAYER: {
          const DocumentReader document(const_cast<Document*>(m_document));
          const Sprite* sprite = m_sprite;
          int old_layer = getLayerIndex(m_layer);
          FrameNumber frame = m_frame;

          // Did the user select another layer?
          if (old_layer != m_clk_layer) {
            setLayer(m_layers[m_clk_layer]);

            jmouse_hide();
            // Redraw the old & new selected cel.
            drawPart(A_PART_CEL, old_layer, frame);
            drawPart(A_PART_CEL, m_clk_layer, frame);
            // Redraw the old selected layer.
            drawPart(A_PART_LAYER, old_layer, frame);
            jmouse_show();
          }

          // Change the scroll to show the new selected cel.
          showCel(m_clk_layer, m_frame);
          captureMouse();
          m_state = STATE_MOVING_LAYER;
          break;
        }
        case A_PART_LAYER_EYE_ICON:
          captureMouse();
          break;
        case A_PART_LAYER_LOCK_ICON:
          captureMouse();
          break;
        case A_PART_CEL: {
          const DocumentReader document(const_cast<Document*>(m_document));
          const Sprite* sprite = document->getSprite();
          int old_layer = getLayerIndex(m_layer);
          FrameNumber old_frame = m_frame;

          // Select the new clicked-part.
          if (old_layer != m_clk_layer ||
              old_frame != m_clk_frame) {
            setLayer(m_layers[m_clk_layer]);
            setFrame(m_clk_frame);

            jmouse_hide();
            // Redraw the old & new selected layer.
            if (old_layer != m_clk_layer) {
              drawPart(A_PART_LAYER, old_layer, old_frame);
              drawPart(A_PART_LAYER, m_clk_layer, m_clk_frame);
            }
            // Redraw the old selected cel.
            drawPart(A_PART_CEL, old_layer, old_frame);
            jmouse_show();
          }

          // Change the scroll to show the new selected cel.
          showCel(m_clk_layer, m_frame);

          // Capture the mouse (to move the cel).
          captureMouse();
          m_state = STATE_MOVING_CEL;
          break;
        }
      }

      // Redraw the new selected part (header, layer or cel).
      jmouse_hide();
      drawPart(m_clk_part,
               m_clk_layer,
               m_clk_frame);
      jmouse_show();
      break;

    case kMouseMoveMessage: {
      int hot_part = A_PART_NOTHING;
      int hot_layer = -1;
      FrameNumber hot_frame(-1);
      gfx::Point mousePos = static_cast<MouseMessage*>(msg)->position();
      int mx = mousePos.x - getBounds().x;
      int my = mousePos.y - getBounds().y;

      if (hasCapture()) {
        if (m_state == STATE_SCROLLING) {
          setScroll(m_scroll_x+jmouse_x(1)-jmouse_x(0),
                    m_scroll_y+jmouse_y(1)-jmouse_y(0), true);

          jmouse_control_infinite_scroll(getBounds());
          return true;
        }
        // If the mouse pressed the mouse's button in the separator,
        // we shouldn't change the hot (so the separator can be
        // tracked to the mouse's released).
        else if (m_clk_part == A_PART_SEPARATOR) {
          hot_part = m_clk_part;
          m_separator_x = mx;
          invalidate();
          return true;
        }
      }

      // Is the mouse on the separator.
      if (mx > m_separator_x-4 && mx < m_separator_x+4)  {
        hot_part = A_PART_SEPARATOR;
      }
      // Is the mouse on the headers?
      else if (my < HDRSIZE) {
        // Is on the layers' header?
        if (mx < m_separator_x)
          hot_part = A_PART_HEADER_LAYER;
        // Is on a frame header?
        else {
          hot_part = A_PART_HEADER_FRAME;
          hot_frame = FrameNumber((mx
                                   - m_separator_x
                                   - m_separator_w
                                   + m_scroll_x) / FRMSIZE);
        }
      }
      else {
        hot_layer = (my
                     - HDRSIZE
                     + m_scroll_y) / LAYSIZE;

        // Is the mouse on a layer's label?
        if (mx < m_separator_x) {
          SkinTheme* theme = static_cast<SkinTheme*>(getTheme());
          BITMAP* icon1 = theme->get_part(PART_LAYER_VISIBLE);
          BITMAP* icon2 = theme->get_part(PART_LAYER_EDITABLE);
          int x1, y1, x2, y2, y_mid;

          x1 = 0;
          y1 = HDRSIZE + LAYSIZE*hot_layer - m_scroll_y;
          x2 = x1 + m_separator_x - 1;
          y2 = y1 + LAYSIZE - 1;
          y_mid = (y1+y2) / 2;

          if (mx >= x1+2 &&
              mx <= x1+ICONSEP+ICONBORDER+icon1->w+ICONBORDER-1 &&
              my >= y_mid-icon1->h/2-ICONBORDER &&
              my <= y_mid+icon1->h/2+ICONBORDER) {
            hot_part = A_PART_LAYER_EYE_ICON;
          }
          else if (mx >= x1+ICONSEP+ICONBORDER+icon1->w+ICONBORDER &&
                   mx <= x1+ICONSEP+ICONBORDER+icon1->w+ICONBORDER+ICONBORDER+icon2->w+ICONBORDER-1 &&
                   my >= y_mid-icon2->h/2-ICONBORDER &&
                   my <= y_mid+icon2->h/2+ICONBORDER) {
            hot_part = A_PART_LAYER_LOCK_ICON;
          }
          else
            hot_part = A_PART_LAYER;
        }
        else {
          hot_part = A_PART_CEL;
          hot_frame = FrameNumber((mx
                                   - m_separator_x
                                   - m_separator_w
                                   + m_scroll_x) / FRMSIZE);
        }
      }

      // Set the new 'hot' thing.
      hotThis(hot_part, hot_layer, hot_frame);
      return true;
    }

    case kMouseUpMessage:
      if (hasCapture()) {
        MouseMessage* mouseMsg = static_cast<MouseMessage*>(msg);

        releaseMouse();

        if (m_state == STATE_SCROLLING) {
          m_state = STATE_STANDBY;
          return true;
        }

        switch (m_hot_part) {
          case A_PART_NOTHING:
            // Do nothing.
            break;
          case A_PART_SEPARATOR:
            // Do nothing.
            break;
          case A_PART_HEADER_LAYER:
            // Do nothing.
            break;
          case A_PART_HEADER_FRAME:
            // Show the frame pop-up menu.
            if (mouseMsg->right()) {
              if (m_clk_frame == m_hot_frame) {
                Menu* popup_menu = AppMenus::instance()->getFramePopupMenu();
                if (popup_menu != NULL) {
                  gfx::Point mousePos = mouseMsg->position();
                  popup_menu->showPopup(mousePos.x, mousePos.y);

                  destroy_thumbnails();
                  invalidate();
                }
              }
            }
            // Show the frame's properties dialog.
            else if (mouseMsg->left()) {
              if (m_clk_frame == m_hot_frame) {
                // Execute FrameProperties command for current frame.
                Command* command = CommandsModule::instance()
                  ->getCommandByName(CommandId::FrameProperties);
                Params params;
                params.set("frame", "current");

                UIContext::instance()->executeCommand(command, &params);
              }
              else {
                const ContextReader reader(m_context);

                if (m_hot_frame >= 0 &&
                    m_hot_frame < m_sprite->getTotalFrames() &&
                    m_hot_frame != m_clk_frame+1) {
                  {
                    ContextWriter writer(reader);
                    UndoTransaction undoTransaction(m_context, "Move Frame");
                    m_document->getApi().moveFrameBefore(writer.sprite(), m_clk_frame, m_hot_frame);
                    undoTransaction.commit();
                  }
                  invalidate();
                }
              }
            }
            break;
          case A_PART_LAYER:
            // Show the layer pop-up menu.
            if (mouseMsg->right()) {
              if (m_clk_layer == m_hot_layer) {
                Menu* popup_menu = AppMenus::instance()->getLayerPopupMenu();
                if (popup_menu != NULL) {
                  gfx::Point mousePos = mouseMsg->position();
                  popup_menu->showPopup(mousePos.x, mousePos.y);

                  destroy_thumbnails();
                  invalidate();
                  regenerateLayers();
                }
              }
            }
            // Move a layer.
            else if (mouseMsg->left()) {
              if (m_hot_layer >= 0 &&
                  m_hot_layer < (int)m_layers.size() &&
                  m_hot_layer != m_clk_layer &&
                  m_hot_layer != m_clk_layer+1) {
                if (!m_layers[m_clk_layer]->isBackground()) {
                  // Move the clicked-layer after the hot-layer.
                  try {
                    const ContextReader reader(m_context);
                    ContextWriter writer(reader);

                    UndoTransaction undoTransaction(m_context, "Move Layer");
                    m_document->getApi().restackLayerAfter(m_layers[m_clk_layer],
                                                           m_layers[m_hot_layer]);
                    undoTransaction.commit();

                    // Select the new layer.
                    setLayer(m_layers[m_clk_layer]);
                  }
                  catch (LockedDocumentException& e) {
                    Console::showException(e);
                  }

                  invalidate();
                  regenerateLayers();
                }
                else {
                  Alert::show(PACKAGE "<<You can't move the `Background' layer.||&OK");
                }
              }
            }
            break;
          case A_PART_LAYER_EYE_ICON:
            // Hide/show layer.
            if (m_hot_layer == m_clk_layer &&
                m_hot_layer >= 0 &&
                m_hot_layer < (int)m_layers.size()) {
              Layer* layer = m_layers[m_clk_layer];
              ASSERT(layer != NULL);
              layer->setReadable(!layer->isReadable());
            }
            break;
          case A_PART_LAYER_LOCK_ICON:
            // Lock/unlock layer.
            if (m_hot_layer == m_clk_layer &&
                m_hot_layer >= 0 &&
                m_hot_layer < (int)m_layers.size()) {
              Layer* layer = m_layers[m_clk_layer];
              ASSERT(layer != NULL);
              layer->setWritable(!layer->isWritable());
            }
            break;
          case A_PART_CEL: {
            bool movement =
              m_clk_part == A_PART_CEL &&
              m_hot_part == A_PART_CEL &&
              (m_clk_layer != m_hot_layer ||
               m_clk_frame != m_hot_frame);

            if (movement) {
              set_frame_to_handle
                (// Source cel.
                 m_layers[m_clk_layer],
                 m_clk_frame,
                 // Destination cel.
                 m_layers[m_hot_layer],
                 m_hot_frame);
            }

            // Show the cel pop-up menu.
            if (mouseMsg->right()) {
              Menu* popup_menu = movement ? AppMenus::instance()->getCelMovementPopupMenu():
                                            AppMenus::instance()->getCelPopupMenu();
              if (popup_menu != NULL) {
                gfx::Point mousePos = mouseMsg->position();
                popup_menu->showPopup(mousePos.x, mousePos.y);

                destroy_thumbnails();
                regenerateLayers();
                invalidate();
              }
            }
            // Move the cel.
            else if (mouseMsg->left()) {
              if (movement) {
                {
                  const ContextReader reader(m_context);
                  ContextWriter writer(reader);
                  move_cel(writer);
                }

                destroy_thumbnails();
                regenerateLayers();
                invalidate();
              }
            }
            break;
          }
        }

        // Clean the clicked-part & redraw the hot-part.
        jmouse_hide();
        cleanClk();
        drawPart(m_hot_part,
                 m_hot_layer,
                 m_hot_frame);
        jmouse_show();

        // Restore the cursor.
        m_state = STATE_STANDBY;
        setCursor(mouseMsg->position().x,
                  mouseMsg->position().y);
        return true;
      }
      break;

    case kKeyDownMessage: {
      Command* command = NULL;
      Params* params = NULL;
      get_command_from_key_message(msg, &command, &params);

      // Close animation editor.
      if ((command && (strcmp(command->short_name(), CommandId::FilmEditor) == 0)) ||
          (static_cast<KeyMessage*>(msg)->scancode() == kKeyEsc)) {
        closeWindow();
        return true;
      }

      // Undo or redo.
      if (command && (strcmp(command->short_name(), CommandId::Undo) == 0 ||
                      strcmp(command->short_name(), CommandId::Redo) == 0)) {
        if (command->isEnabled(UIContext::instance())) {
          UIContext::instance()->executeCommand(command, params);

          destroy_thumbnails();
          regenerateLayers();
          showCurrentCel();
          invalidate();
        }
        return true;
      }

      // New_frame, remove_frame, new_cel, remove_cel.
      if (command != NULL) {
        if (strcmp(command->short_name(), CommandId::NewFrame) == 0 ||
            strcmp(command->short_name(), CommandId::RemoveCel) == 0 ||
            strcmp(command->short_name(), CommandId::RemoveFrame) == 0 ||
            strcmp(command->short_name(), CommandId::GotoFirstFrame) == 0 ||
            strcmp(command->short_name(), CommandId::GotoPreviousFrame) == 0 ||
            strcmp(command->short_name(), CommandId::GotoPreviousLayer) == 0 ||
            strcmp(command->short_name(), CommandId::GotoNextFrame) == 0 ||
            strcmp(command->short_name(), CommandId::GotoNextLayer) == 0 ||
            strcmp(command->short_name(), CommandId::GotoLastFrame) == 0) {
          // execute the command
          UIContext::instance()->executeCommand(command, params);

          showCurrentCel();
          invalidate();
          return true;
        }

        if (strcmp(command->short_name(), CommandId::NewLayer) == 0 ||
            strcmp(command->short_name(), CommandId::RemoveLayer) == 0) {
          // execute the command
          UIContext::instance()->executeCommand(command);

          regenerateLayers();
          showCurrentCel();
          invalidate();
          return true;
        }
      }

      switch (static_cast<KeyMessage*>(msg)->scancode()) {
        case kKeySpace:
          m_space_pressed = true;
          setCursor(jmouse_x(0), jmouse_y(0));
          return true;
      }

      break;
    }

    case kKeyUpMessage:
      switch (static_cast<KeyMessage*>(msg)->scancode()) {

        case kKeySpace:
          if (m_space_pressed) {
            // We have to clear all the KEY_SPACE in buffer.
            clear_keybuf();

            m_space_pressed = false;
            setCursor(jmouse_x(0), jmouse_y(0));
            return true;
          }
          break;
      }
      break;

    case kMouseWheelMessage: {
      int dz = jmouse_z(1) - jmouse_z(0);
      int dx = 0;
      int dy = 0;

      if (msg->ctrlPressed())
        dx = dz * FRMSIZE;
      else
        dy = dz * LAYSIZE;

      if (msg->shiftPressed()) {
        dx *= 3;
        dy *= 3;
      }

      setScroll(m_scroll_x+dx,
                m_scroll_y+dy, true);
      break;
    }

    case kSetCursorMessage: {
      gfx::Point mousePos = static_cast<MouseMessage*>(msg)->position();
      setCursor(mousePos.x, mousePos.y);
      return true;
    }

  }

  return Widget::onProcessMessage(msg);
}

void AnimationEditor::onPreferredSize(PreferredSizeEvent& ev)
{
  // This doesn't matter, the AniEditor'll use the entire screen anyway.
  ev.setPreferredSize(Size(32, 32));
}


void AnimationEditor::onAddLayer(DocumentEvent& ev)
{
  ASSERT(ev.layer() != NULL);
  setLayer(ev.layer());
}

void AnimationEditor::onRemoveLayer(DocumentEvent& ev)
{
  Sprite* sprite = ev.sprite();
  Layer* layer = ev.layer();

  // If the layer that was removed is the selected one
  if (layer == getLayer()) {
    LayerFolder* parent = layer->getParent();
    Layer* layer_select = NULL;

    // Select previous layer, or next layer, or the parent (if it is
    // not the main layer of sprite set).
    if (layer->getPrevious())
      layer_select = layer->getPrevious();
    else if (layer->getNext())
      layer_select = layer->getNext();
    else if (parent != sprite->getFolder())
      layer_select = parent;

    setLayer(layer_select);
  }
}

void AnimationEditor::onAddFrame(DocumentEvent& ev)
{
  setFrame(ev.frame());
}

void AnimationEditor::onRemoveFrame(DocumentEvent& ev)
{
  // Adjust current frame of all editors that are in a frame more
  // advanced that the removed one.
  if (getFrame() > ev.frame()) {
    setFrame(getFrame().previous());
  }
  // If the editor was in the previous "last frame" (current value of
  // getTotalFrames()), we've to adjust it to the new last frame
  // (getLastFrame())
  else if (getFrame() >= getSprite()->getTotalFrames()) {
    setFrame(getSprite()->getLastFrame());
  }
}

void AnimationEditor::onTotalFramesChanged(DocumentEvent& ev)
{
  if (getFrame() >= getSprite()->getTotalFrames()) {
    setFrame(getSprite()->getLastFrame());
  }
}

void AnimationEditor::setCursor(int x, int y)
{
  int mx = x - getBounds().x;
//int my = y - getBounds().y;

  // Is the mouse in the separator.
  if (mx > m_separator_x-2 && mx < m_separator_x+2)  {
    jmouse_set_cursor(kSizeLCursor);
  }
  // Scrolling.
  else if (m_state == STATE_SCROLLING ||
           m_space_pressed) {
    jmouse_set_cursor(kScrollCursor);
  }
  // Moving a frame.
  else if (m_state == STATE_MOVING_FRAME &&
           m_clk_part == A_PART_HEADER_FRAME &&
           m_hot_part == A_PART_HEADER_FRAME &&
           m_clk_frame != m_hot_frame) {
    jmouse_set_cursor(kMoveCursor);
  }
  // Moving a layer.
  else if (m_state == STATE_MOVING_LAYER &&
           m_clk_part == A_PART_LAYER &&
           m_hot_part == A_PART_LAYER &&
           m_clk_layer != m_hot_layer) {
    if (m_layers[m_clk_layer]->isBackground())
      jmouse_set_cursor(kForbiddenCursor);
    else
      jmouse_set_cursor(kMoveCursor);
  }
  // Moving a cel.
  else if (m_state == STATE_MOVING_CEL &&
           m_clk_part == A_PART_CEL &&
           m_hot_part == A_PART_CEL &&
           (m_clk_frame != m_hot_frame ||
            m_clk_layer != m_hot_layer)) {
    jmouse_set_cursor(kMoveCursor);
  }
  // Normal state.
  else {
    jmouse_set_cursor(kArrowCursor);
  }
}

void AnimationEditor::getDrawableLayers(const gfx::Rect& clip, int* first_layer, int* last_layer)
{
  *first_layer = 0;
  *last_layer = m_layers.size()-1;
}

void AnimationEditor::getDrawableFrames(const gfx::Rect& clip, FrameNumber* first_frame, FrameNumber* last_frame)
{
  *first_frame = FrameNumber(0);
  *last_frame = m_sprite->getLastFrame();
}

void AnimationEditor::drawHeader(const gfx::Rect& clip)
{
  // bool is_hot = (m_hot_part == A_PART_HEADER_LAYER);
  // bool is_clk = (m_clk_part == A_PART_HEADER_LAYER);
  int x1, y1, x2, y2;

  x1 = getBounds().x;
  y1 = getBounds().y;
  x2 = x1 + m_separator_x - 1;
  y2 = y1 + HDRSIZE - 1;

  // Draw the header for the layers.
  drawHeaderPart(clip, x1, y1, x2, y2,
                 // is_hot, is_clk,
                 false, false,
                 "Frames >>", 1,
                 "Layers", -1);
}

void AnimationEditor::drawHeaderFrame(const gfx::Rect& clip, FrameNumber frame)
{
  SkinTheme* theme = static_cast<SkinTheme*>(getTheme());
  bool is_hot = (m_hot_part == A_PART_HEADER_FRAME &&
                 m_hot_frame == frame);
  bool is_clk = (m_clk_part == A_PART_HEADER_FRAME &&
                 m_clk_frame == frame);
  int x1, y1, x2, y2;
  int cx1, cy1, cx2, cy2;
  char buf1[256];
  char buf2[256];

  get_clip_rect(ji_screen, &cx1, &cy1, &cx2, &cy2);

  x1 = getBounds().x + m_separator_x + m_separator_w
    + FRMSIZE*frame - m_scroll_x;
  y1 = getBounds().y;
  x2 = x1 + FRMSIZE - 1;
  y2 = y1 + HDRSIZE - 1;

  add_clip_rect(ji_screen,
                getBounds().x + m_separator_x + m_separator_w,
                y1, getBounds().x2()-1, y2);

  // Draw the header for the layers.
  usprintf(buf1, "%d", frame+1);
  usprintf(buf2, "%d", m_sprite->getFrameDuration(frame));
  drawHeaderPart(clip, x1, y1, x2, y2,
                 is_hot, is_clk,
                 buf1, 0,
                 buf2, 0);

  // If this header wasn't clicked but there are another frame's
  // header clicked, we have to draw some indicators to show that the
  // user can move frames.
  if (is_hot && !is_clk &&
      m_clk_part == A_PART_HEADER_FRAME) {
    rectfill(ji_screen, x1+1, y1+1, x1+4, y2-1,
             to_system(theme->getColor(ThemeColor::Selected)));
  }

  // Padding in the right side.
  if (frame == m_sprite->getTotalFrames()-1) {
    if (x2+1 <= getBounds().x2()-1) {
      // Right side.
      vline(ji_screen, x2+1, y1, y2,
            to_system(theme->getColor(ThemeColor::Text)));
      if (x2+2 <= getBounds().x2()-1)
        rectfill(ji_screen, x2+2, y1, getBounds().x2()-1, y2,
                 to_system(theme->getColor(ThemeColor::Face)));
    }
  }

  set_clip_rect(ji_screen, cx1, cy1, cx2, cy2);
}

void AnimationEditor::drawHeaderPart(const gfx::Rect& clip, int x1, int y1, int x2, int y2,
                                     bool is_hot, bool is_clk,
                                     const char *line1, int align1,
                                     const char *line2, int align2)
{
  SkinTheme* theme = static_cast<SkinTheme*>(getTheme());
  ui::Color fg, face;
  int x;

  if (!clip.intersects(gfx::Rect(x1, y1, x2-x1, y2-y1)))
    return;

  fg = theme->getColor(!is_hot && is_clk ? ThemeColor::Background: ThemeColor::Text);
  face = theme->getColor(is_hot ? ThemeColor::HotFace: (is_clk ? ThemeColor::Selected:
                                                                 ThemeColor::Face));

  // Fill the background of the part.
  rectfill(ji_screen, x1, y1, x2, y2, to_system(face));

  // Draw the text inside this header.
  if (line1 != NULL) {
    if (align1 < 0)
      x = x1+3;
    else if (align1 == 0)
      x = (x1+x2)/2 - text_length(getFont(), line1)/2;
    else
      x = x2 - 3 - text_length(getFont(), line1);

    jdraw_text(ji_screen, getFont(), line1, x, y1+3,
               fg, face, true, jguiscale());
  }

  if (line2 != NULL) {
    if (align2 < 0)
      x = x1+3;
    else if (align2 == 0)
      x = (x1+x2)/2 - text_length(this->getFont(), line2)/2;
    else
      x = x2 - 3 - text_length(this->getFont(), line2);

    jdraw_text(ji_screen, this->getFont(), line2,
               x, y1+3+ji_font_get_size(this->getFont())+3,
               fg, face, true, jguiscale());
  }
}

void AnimationEditor::drawSeparator(const gfx::Rect& clip)
{
  SkinTheme* theme = static_cast<SkinTheme*>(getTheme());
  bool is_hot = (m_hot_part == A_PART_SEPARATOR);
  int x1, y1, x2, y2;

  x1 = getBounds().x + m_separator_x;
  y1 = getBounds().y;
  x2 = getBounds().x + m_separator_x + m_separator_w - 1;
  y2 = getBounds().y2() - 1;

  if (!clip.intersects(gfx::Rect(x1, y1, x2-x1, y2-y1)))
    return;

  vline(ji_screen, x1, y1, y2,
        to_system(is_hot ? theme->getColor(ThemeColor::Selected):
                           theme->getColor(ThemeColor::Text)));
}

void AnimationEditor::drawLayer(const gfx::Rect& clip, int layer_index)
{
  Layer* layer = m_layers[layer_index];
  SkinTheme* theme = static_cast<SkinTheme*>(this->getTheme());
  bool selected_layer = (layer == m_layer);
  bool is_hot = (m_hot_part == A_PART_LAYER && m_hot_layer == layer_index);
  bool is_clk = (m_clk_part == A_PART_LAYER && m_clk_layer == layer_index);
  ui::Color bg = theme->getColor(selected_layer ? ThemeColor::Selected:
                                 (is_hot ? ThemeColor::HotFace:
                                  (is_clk ? ThemeColor::Selected:
                                            ThemeColor::Face)));
  ui::Color fg = theme->getColor(selected_layer ? ThemeColor::Background: ThemeColor::Text);
  BITMAP* icon1 = theme->get_part(layer->isReadable() ? PART_LAYER_VISIBLE: PART_LAYER_HIDDEN);
  BITMAP* icon2 = theme->get_part(layer->isWritable() ? PART_LAYER_EDITABLE: PART_LAYER_LOCKED);
  BITMAP* icon1_selected = theme->get_part(layer->isReadable() ? PART_LAYER_VISIBLE_SELECTED: PART_LAYER_HIDDEN_SELECTED);
  BITMAP* icon2_selected = theme->get_part(layer->isWritable() ? PART_LAYER_EDITABLE_SELECTED: PART_LAYER_LOCKED_SELECTED);
  int x1, y1, x2, y2, y_mid;
  int cx1, cy1, cx2, cy2;
  int u;

  get_clip_rect(ji_screen, &cx1, &cy1, &cx2, &cy2);

  x1 = getBounds().x;
  y1 = getBounds().y + HDRSIZE + LAYSIZE*layer_index - m_scroll_y;
  x2 = x1 + m_separator_x - 1;
  y2 = y1 + LAYSIZE - 1;
  y_mid = (y1+y2) / 2;

  add_clip_rect(ji_screen,
                getBounds().x,
                getBounds().y + HDRSIZE,
                getBounds().x + m_separator_x - 1,
                getBounds().y2()-1);

  rectfill(ji_screen, x1, y1, x2, y2-1, to_system(bg));
  hline(ji_screen, x1, y2, x2,
        to_system(theme->getColor(ThemeColor::Text)));

  // If this layer wasn't clicked but there are another layer clicked,
  // we have to draw some indicators to show that the user can move
  // layers.
  if (is_hot && !is_clk &&
      m_clk_part == A_PART_LAYER) {
    rectfill(ji_screen, x1+1, y1+1, x2-1, y1+5,
             to_system(theme->getColor(ThemeColor::Selected)));
  }

  // u = the position where to put the next element (like eye-icon,
  //     lock-icon, layer-name)
  u = x1+ICONSEP;

  // Draw the eye (readable flag).
  icon_rect(icon1, icon1_selected,
            u,
            y_mid-icon1->h/2-ICONBORDER,
            u+ICONBORDER+icon1->w+ICONBORDER-1,
            y_mid-icon1->h/2+icon1->h+ICONBORDER-1,
            selected_layer,
            (m_hot_part == A_PART_LAYER_EYE_ICON &&
             m_hot_layer == layer_index),
            (m_clk_part == A_PART_LAYER_EYE_ICON &&
             m_clk_layer == layer_index));

  u += u+ICONBORDER+icon1->w+ICONBORDER;

  // Draw the padlock (writable flag).
  icon_rect(icon2, icon2_selected,
            u,
            y_mid-icon1->h/2-ICONBORDER,
            u+ICONBORDER+icon2->w+ICONBORDER-1,
            y_mid-icon1->h/2+icon1->h-1+ICONBORDER,
            selected_layer,
            (m_hot_part == A_PART_LAYER_LOCK_ICON &&
             m_hot_layer == layer_index),
            (m_clk_part == A_PART_LAYER_LOCK_ICON &&
             m_clk_layer == layer_index));

  u += ICONBORDER+icon2->w+ICONBORDER+ICONSEP;

  // Draw the layer's name.
  jdraw_text(ji_screen, this->getFont(), layer->getName().c_str(),
             u, y_mid - ji_font_get_size(this->getFont())/2,
             fg, bg, true, jguiscale());

  // The background should be underlined.
  if (layer->isBackground()) {
    hline(ji_screen,
          u,
          y_mid - ji_font_get_size(this->getFont())/2 + ji_font_get_size(this->getFont()) + 1,
          u + text_length(this->getFont(), layer->getName().c_str()),
          to_system(fg));
  }

  set_clip_rect(ji_screen, cx1, cy1, cx2, cy2);
}

void AnimationEditor::drawLayerPadding()
{
  SkinTheme* theme = static_cast<SkinTheme*>(this->getTheme());
  int layer_index = m_layers.size()-1;
  int x1, y1, x2, y2;

  x1 = getBounds().x;
  y1 = getBounds().y + HDRSIZE + LAYSIZE*layer_index - m_scroll_y;
  x2 = x1 + m_separator_x - 1;
  y2 = y1 + LAYSIZE - 1;

  // Padding in the bottom side.
  if (y2+1 <= getBounds().y2()-1) {
    ui::Color color = theme->getColor(ThemeColor::EditorFace);
    rectfill(ji_screen, x1, y2+1, x2, getBounds().y2()-1, to_system(color));
    rectfill(ji_screen,
             x2+1+m_separator_w, y2+1,
             getBounds().x2()-1, getBounds().y2()-1,
             to_system(color));
  }
}

void AnimationEditor::drawCel(const gfx::Rect& clip, int layer_index, FrameNumber frame, Cel* cel)
{
  SkinTheme* theme = static_cast<SkinTheme*>(this->getTheme());
  Layer *layer = m_layers[layer_index];
  bool selected_layer = (layer == m_layer);
  bool is_hot = (m_hot_part == A_PART_CEL &&
                 m_hot_layer == layer_index &&
                 m_hot_frame == frame);
  bool is_clk = (m_clk_part == A_PART_CEL &&
                 m_clk_layer == layer_index &&
                 m_clk_frame == frame);
  ui::Color bg = theme->getColor(is_hot ? ThemeColor::HotFace: ThemeColor::Face);
  int x1, y1, x2, y2;
  int cx1, cy1, cx2, cy2;
  BITMAP *thumbnail;

  get_clip_rect(ji_screen, &cx1, &cy1, &cx2, &cy2);

  x1 = getBounds().x + m_separator_x + m_separator_w
    + FRMSIZE*frame - m_scroll_x;
  y1 = getBounds().y + HDRSIZE
    + LAYSIZE*layer_index - m_scroll_y;
  x2 = x1 + FRMSIZE - 1;
  y2 = y1 + LAYSIZE - 1;

  add_clip_rect(ji_screen,
                getBounds().x + m_separator_x + m_separator_w,
                getBounds().y + HDRSIZE,
                getBounds().x2()-1,
                getBounds().y2()-1);

  Rect thumbnail_rect(Point(x1+3, y1+3), Point(x2-2, y2-2));

  // Draw the box for the cel.
  if (selected_layer && frame == m_frame) {
    // Current cel.
    rect(ji_screen, x1, y1, x2, y2-1, to_system(theme->getColor(ThemeColor::Selected)));
    rect(ji_screen, x1+1, y1+1, x2-1, y2-2, to_system(bg));
  }
  else {
    rectfill(ji_screen, x1, y1, x2, y2-1, to_system(bg));
  }
  hline(ji_screen, x1, y2, x2, to_system(theme->getColor(ThemeColor::Text)));

  // Empty cel?.
  if (cel == NULL ||
      // TODO why a cel can't have an associated image?
      m_sprite->getStock()->getImage(cel->getImage()) == NULL) {

    jdraw_rectfill(thumbnail_rect, bg);
    draw_emptyset_symbol(ji_screen, thumbnail_rect, theme->getColor(ThemeColor::Disabled));
  }
  else {
    thumbnail = generate_thumbnail(layer, cel, m_sprite);
    if (thumbnail != NULL) {
      stretch_blit(thumbnail, ji_screen,
                   0, 0, thumbnail->w, thumbnail->h,
                   thumbnail_rect.x, thumbnail_rect.y,
                   thumbnail_rect.w, thumbnail_rect.h);
    }
  }

  // If this cel is hot and other cel was clicked, we have to draw
  // some indicators to show that the user can move cels.
  if (is_hot && !is_clk &&
      m_clk_part == A_PART_CEL) {
    int color = to_system(theme->getColor(ThemeColor::Selected));

    rectfill(ji_screen, x1+1, y1+1, x1+FRMSIZE/3, y1+4, color);
    rectfill(ji_screen, x1+1, y1+5, x1+4, y1+FRMSIZE/3, color);

    rectfill(ji_screen, x2-FRMSIZE/3, y1+1, x2-1, y1+4, color);
    rectfill(ji_screen, x2-4, y1+5, x2-1, y1+FRMSIZE/3, color);

    rectfill(ji_screen, x1+1, y2-4, x1+FRMSIZE/3, y2-1, color);
    rectfill(ji_screen, x1+1, y2-FRMSIZE/3, x1+4, y2-5, color);

    rectfill(ji_screen, x2-FRMSIZE/3, y2-4, x2-1, y2-1, color);
    rectfill(ji_screen, x2-4, y2-FRMSIZE/3, x2-1, y2-5, color);
  }

  // Padding in the right side.
  if (frame == m_sprite->getTotalFrames()-1) {
    if (x2+1 <= getBounds().x2()-1) {
      // Right side.
      vline(ji_screen, x2+1, y1, y2, to_system(theme->getColor(ThemeColor::Text)));
      if (x2+2 <= getBounds().x2()-1)
        rectfill(ji_screen, x2+2, y1, getBounds().x2()-1, y2,
                 to_system(theme->getColor(ThemeColor::EditorFace)));
    }
  }

  set_clip_rect(ji_screen, cx1, cy1, cx2, cy2);
}

bool AnimationEditor::drawPart(int part, int layer, FrameNumber frame)
{
  switch (part) {
    case A_PART_NOTHING:
      // Do nothing.
      return true;
    case A_PART_SEPARATOR:
      drawSeparator(getBounds());
      return true;
    case A_PART_HEADER_LAYER:
      drawHeader(getBounds());
      return true;
    case A_PART_HEADER_FRAME:
      if (frame >= 0 && frame < m_sprite->getTotalFrames()) {
        drawHeaderFrame(getBounds(), frame);
        return true;
      }
      break;
    case A_PART_LAYER:
    case A_PART_LAYER_EYE_ICON:
    case A_PART_LAYER_LOCK_ICON:
      if (layer >= 0 && layer < (int)m_layers.size()) {
        drawLayer(getBounds(), layer);
        return true;
      }
      break;
    case A_PART_CEL:
      if (layer >= 0 && layer < (int)m_layers.size() &&
          frame >= 0 && frame < m_sprite->getTotalFrames()) {
        Cel* cel = (m_layers[layer]->isImage() ? static_cast<LayerImage*>(m_layers[layer])->getCel(frame): NULL);

        drawCel(getBounds(), layer, frame, cel);
        return true;
      }
      break;
  }

  return false;
}

void AnimationEditor::regenerateLayers()
{
  m_layers.clear();
  size_t nlayers = m_sprite->countLayers();
  if (nlayers > 0) {
    m_layers.resize(nlayers, NULL);
    for (size_t c=0; c<nlayers; c++)
      m_layers[c] = m_sprite->indexToLayer(LayerIndex(nlayers-c-1));
  }
}

void AnimationEditor::hotThis(int hot_part, int hot_layer, FrameNumber hot_frame)
{
  int old_hot_part;

  // If the part, layer or frame change.
  if (hot_part != m_hot_part ||
      hot_layer != m_hot_layer ||
      hot_frame != m_hot_frame) {
    jmouse_hide();

    // Clean the old 'hot' thing.
    old_hot_part = m_hot_part;
    m_hot_part = A_PART_NOTHING;

    drawPart(old_hot_part,
             m_hot_layer,
             m_hot_frame);

    // Draw the new 'hot' thing.
    m_hot_part = hot_part;
    m_hot_layer = hot_layer;
    m_hot_frame = hot_frame;

    if (!drawPart(hot_part, hot_layer, hot_frame)) {
      m_hot_part = A_PART_NOTHING;
    }

    jmouse_show();
  }
}

void AnimationEditor::centerCel(int layer, FrameNumber frame)
{
  int target_x = (getBounds().x + m_separator_x + m_separator_w + getBounds().x2())/2 - FRMSIZE/2;
  int target_y = (getBounds().y + HDRSIZE + getBounds().y2())/2 - LAYSIZE/2;
  int scroll_x = getBounds().x + m_separator_x + m_separator_w + FRMSIZE*frame - target_x;
  int scroll_y = getBounds().y + HDRSIZE + LAYSIZE*layer - target_y;

  setScroll(scroll_x, scroll_y, false);
}

void AnimationEditor::showCel(int layer, FrameNumber frame)
{
  int scroll_x, scroll_y;
  int x1, y1, x2, y2;

  x1 = getBounds().x + m_separator_x + m_separator_w + FRMSIZE*frame - m_scroll_x;
  y1 = getBounds().y + HDRSIZE + LAYSIZE*layer - m_scroll_y;
  x2 = x1 + FRMSIZE - 1;
  y2 = y1 + LAYSIZE - 1;

  scroll_x = m_scroll_x;
  scroll_y = m_scroll_y;

  if (x1 < getBounds().x + m_separator_x + m_separator_w) {
    scroll_x -= (getBounds().x + m_separator_x + m_separator_w) - (x1);
  }
  else if (x2 > getBounds().x2()-1) {
    scroll_x += (x2) - (getBounds().x2()-1);
  }

  if (y1 < getBounds().y + HDRSIZE) {
    scroll_y -= (getBounds().y + HDRSIZE) - (y1);
  }
  else if (y2 > getBounds().y2()-1) {
    scroll_y += (y2) - (getBounds().y2()-1);
  }

  if (scroll_x != m_scroll_x ||
      scroll_y != m_scroll_y)
    setScroll(scroll_x, scroll_y, true);
}

void AnimationEditor::centerCurrentCel()
{
  int layer = getLayerIndex(m_layer);
  if (layer >= 0)
    centerCel(layer, m_frame);
}

void AnimationEditor::showCurrentCel()
{
  int layer = getLayerIndex(m_layer);
  if (layer >= 0)
    showCel(layer, m_frame);
}

void AnimationEditor::cleanClk()
{
  int clk_part = m_clk_part;
  m_clk_part = A_PART_NOTHING;

  drawPart(clk_part,
           m_clk_layer,
           m_clk_frame);
}

void AnimationEditor::setScroll(int x, int y, bool use_refresh_region)
{
  int old_scroll_x = 0;
  int old_scroll_y = 0;
  int max_scroll_x;
  int max_scroll_y;
  Region region;

  if (use_refresh_region) {
    getDrawableRegion(region, kCutTopWindows);
    old_scroll_x = m_scroll_x;
    old_scroll_y = m_scroll_y;
  }

  max_scroll_x = m_sprite->getTotalFrames() * FRMSIZE - getBounds().w/2;
  max_scroll_y = m_layers.size() * LAYSIZE - getBounds().h/2;
  max_scroll_x = MAX(0, max_scroll_x);
  max_scroll_y = MAX(0, max_scroll_y);

  m_scroll_x = MID(0, x, max_scroll_x);
  m_scroll_y = MID(0, y, max_scroll_y);

  if (use_refresh_region) {
    int new_scroll_x = m_scroll_x;
    int new_scroll_y = m_scroll_y;
    int dx = old_scroll_x - new_scroll_x;
    int dy = old_scroll_y - new_scroll_y;
    Rect rect2;
    Region reg1;

    jmouse_hide();

    // Scroll layers.
    rect2 = Rect(getBounds().x,
                 getBounds().y + HDRSIZE,
                 m_separator_x,
                 getBounds().y2() - (getBounds().y + HDRSIZE));
    reg1.createIntersection(region, Region(rect2));
    scrollRegion(reg1, 0, dy);

    // Scroll header-frame.
    rect2 = Rect(getBounds().x + m_separator_x + m_separator_w,
                 getBounds().y,
                 getBounds().x2() - (getBounds().x + m_separator_x + m_separator_w),
                 HDRSIZE);
    reg1.createIntersection(region, Region(rect2));
    scrollRegion(reg1, dx, 0);

    // Scroll cels.
    rect2 = Rect(getBounds().x + m_separator_x + m_separator_w,
                 getBounds().y + HDRSIZE,
                 getBounds().x2() - (getBounds().x + m_separator_x + m_separator_w),
                 getBounds().y2() - (getBounds().y + HDRSIZE));
    reg1.createIntersection(region, Region(rect2));
    scrollRegion(reg1, dx, dy);

    jmouse_show();
  }
}

int AnimationEditor::getLayerIndex(const Layer* layer)
{
  for (size_t i=0; i<m_layers.size(); i++)
    if (m_layers[i] == layer)
      return i;

  return -1;
}

// Auxiliary routine to draw an icon in the layer-part.
static void icon_rect(BITMAP* icon_normal, BITMAP* icon_selected, int x1, int y1, int x2, int y2,
                      bool is_selected, bool is_hot, bool is_clk)
{
  SkinTheme* theme = static_cast<SkinTheme*>(ui::CurrentTheme::get());
  int icon_x = x1+ICONBORDER;
  int icon_y = (y1+y2)/2-icon_normal->h/2;

  if (is_hot && !is_selected) {
    rectfill(ji_screen,
             x1, y1, x2, y2,
             to_system(theme->getColor(ThemeColor::HotFace)));
  }

  set_alpha_blender();
  if (is_selected)
    draw_trans_sprite(ji_screen, icon_selected, icon_x, icon_y);
  else
    draw_trans_sprite(ji_screen, icon_normal, icon_x, icon_y);
}

} // namespace app
