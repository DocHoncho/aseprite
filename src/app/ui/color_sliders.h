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

#ifndef APP_UI_COLOR_SLIDERS_H_INCLUDED
#define APP_UI_COLOR_SLIDERS_H_INCLUDED

#include "app/color.h"
#include "base/compiler_specific.h"
#include "base/signal.h"
#include "ui/event.h"
#include "ui/grid.h"
#include "ui/widget.h"
#include "raster/palette.h"

#include <vector>

namespace ui {
  class Label;
  class Slider;
  class Entry;
}

namespace app {

  class ColorSlidersChangeEvent;

  class ColorSliders : public ui::Widget {
  public:
    enum Channel { Red, Green, Blue,
                   Hue, Saturation, Value,
                   Gray };

    ColorSliders();
    ~ColorSliders();

    void setColor(const app::Color& color);

    // Signals
    Signal1<void, ColorSlidersChangeEvent&> ColorChange;

  protected:
    void onPreferredSize(ui::PreferredSizeEvent& ev);

    // For derived classes
    void addSlider(Channel channel, const char* labelText, int min, int max);
    void setSliderValue(int sliderIndex, int value);
    int getSliderValue(int sliderIndex) const;

    virtual void onSetColor(const app::Color& color) = 0;
    virtual app::Color getColorFromSliders() = 0;

    void setQuantizeToIndexed(bool quantize);

  private:
    void onSliderChange(int i);
    void onEntryChange(int i);
    void onControlChange(int i);

    void updateEntryText(int entryIndex);
    void updateSlidersBgColor(const app::Color& color);
    void updateSliderBgColor(ui::Slider* slider, const app::Color& color);
    void updateSliderQuantizeToIndexed(ui::Slider* slider, bool quantize);

    std::vector<ui::Label*> m_label;
    std::vector<ui::Slider*> m_slider;
    std::vector<ui::Entry*> m_entry;
    std::vector<Channel> m_channel;
    ui::Grid m_grid;
  };

  //////////////////////////////////////////////////////////////////////
  // Derived-classes

  class RgbSliders : public ColorSliders
  {
  public:
    RgbSliders();

  private:
    virtual void onSetColor(const app::Color& color) OVERRIDE;
    virtual app::Color getColorFromSliders() OVERRIDE;
  };

  class HsvSliders : public ColorSliders
  {
  public:
    HsvSliders();

  private:
    virtual void onSetColor(const app::Color& color) OVERRIDE;
    virtual app::Color getColorFromSliders() OVERRIDE;
  };

  class GraySlider : public ColorSliders
  {
  public:
    GraySlider();

  private:
    virtual void onSetColor(const app::Color& color) OVERRIDE;
    virtual app::Color getColorFromSliders() OVERRIDE;
  };

  //////////////////////////////////////////////////////////////////////
  // Events

  class ColorSlidersChangeEvent : public ui::Event
  {
  public:
    ColorSlidersChangeEvent(const app::Color& color, ColorSliders::Channel channel, ui::Component* source)
      : Event(source)
      , m_color(color)
      , m_channel(channel) { }

    app::Color getColor() const { return m_color; }

    ColorSliders::Channel getModifiedChannel() const { return m_channel; }

  private:
    app::Color m_color;
    ColorSliders::Channel m_channel;
  };

} // namespace app

#endif
