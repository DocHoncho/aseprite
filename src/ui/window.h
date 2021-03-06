// Aseprite UI Library
// Copyright (C) 2001-2013  David Capello
//
// This source file is distributed under MIT license,
// please read LICENSE.txt for more information.

#ifndef UI_WINDOW_H_INCLUDED
#define UI_WINDOW_H_INCLUDED

#include "base/compiler_specific.h"
#include "base/signal.h"
#include "gfx/point.h"
#include "ui/close_event.h"
#include "ui/event.h"
#include "ui/hit_test_event.h"
#include "ui/widget.h"

namespace ui {

  class Window : public Widget
  {
  public:
    Window(bool isDesktop, const base::string& text);
    ~Window();

    Widget* getKiller();

    void setAutoRemap(bool state);
    void setMoveable(bool state);
    void setSizeable(bool state);
    void setOnTop(bool state);
    void setWantFocus(bool state);

    void remapWindow();
    void centerWindow();
    void positionWindow(int x, int y);
    void moveWindow(const gfx::Rect& rect);

    void openWindow();
    void openWindowInForeground();
    void closeWindow(Widget* killer);

    bool isTopLevel();
    bool isForeground() const { return m_isForeground; }
    bool isDesktop() const { return m_isDesktop; }
    bool isOnTop() const { return m_isOnTop; }
    bool isWantFocus() const { return m_isWantFocus; }
    bool isMoveable() const { return m_isMoveable; }

    HitTest hitTest(const gfx::Point& point);

    void removeDecorativeWidgets();

    // Signals
    Signal1<void, CloseEvent&> Close;

  protected:
    virtual bool onProcessMessage(Message* msg) OVERRIDE;
    virtual void onResize(ResizeEvent& ev) OVERRIDE;
    virtual void onPreferredSize(PreferredSizeEvent& ev) OVERRIDE;
    virtual void onPaint(PaintEvent& ev) OVERRIDE;
    virtual void onBroadcastMouseMessage(WidgetsList& targets) OVERRIDE;
    virtual void onSetText() OVERRIDE;

    // New events
    virtual void onClose(CloseEvent& ev);
    virtual void onHitTest(HitTestEvent& ev);

  private:
    void windowSetPosition(const gfx::Rect& rect);
    int getAction(int x, int y);
    void limitSize(int* w, int* h);
    void moveWindow(const gfx::Rect& rect, bool use_blit);

    Widget* m_killer;
    bool m_isDesktop : 1;
    bool m_isMoveable : 1;
    bool m_isSizeable : 1;
    bool m_isOnTop : 1;
    bool m_isWantFocus : 1;
    bool m_isForeground : 1;
    bool m_isAutoRemap : 1;
    int m_hitTest;
  };

} // namespace ui

#endif
