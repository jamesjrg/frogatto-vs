#ifndef BORDER_WIDGET_HPP_INCLUDED
#define BORDER_WIDGET_HPP_INCLUDED

#include "color_utils.hpp"
#include "widget.hpp"

namespace gui {

//a widget which draws a border around another widget it holds as its child.
class border_widget : public widget
{
public:
	border_widget(widget_ptr child, graphics::color col, int border_size=2);
	void set_color(const graphics::color& col);
private:
	void handle_draw() const;
	bool handle_event(const SDL_Event& event, bool claimed);

	widget_ptr child_;
	graphics::color color_;
	int border_size_;
};

}

#endif
