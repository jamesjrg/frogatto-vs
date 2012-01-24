#include <SDL.h>
#ifndef SDL_VIDEO_OPENGL_ES
#include <GL/glew.h>
#endif

#include <iostream>
#include <math.h>

#include "preferences.hpp"
#include "asserts.hpp"
#include "color_utils.hpp"
#include "foreach.hpp"
#include "formatter.hpp"
#include "formula.hpp"
#include "level.hpp"
#include "raster.hpp"
#include "string_utils.hpp"
#include "water.hpp"
#include "wml_node.hpp"
#include "wml_utils.hpp"
#include "color_utils.hpp"

#if defined(TARGET_OS_HARMATTAN) || defined(TARGET_PANDORA) || defined(TARGET_TEGRA)
#include <EGL/egl.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#define glBlendEquationOES          preferences::glBlendEquationOES
#endif

namespace {
	const int WaterZorder = 15;
}

water::water()
  : zorder_(WaterZorder)
{
}

water::water(wml::const_node_ptr water_node) :
  zorder_(wml::get_int(water_node, "zorder", WaterZorder)),
  current_x_formula_(game_logic::formula::create_optional_formula(water_node->attr("current_x_formula"))),
  current_y_formula_(game_logic::formula::create_optional_formula(water_node->attr("current_y_formula")))
{
	FOREACH_WML_CHILD(area_node, water_node, "area") {
		const rect r(area_node->attr("rect"));
		std::vector<std::string> str = util::split(area_node->attr("color"));
		unsigned char color[4];
		for(int n = 0; n != 4; ++n) {
			if(n < str.size()) {
				color[n] = atoi(str[n].c_str());
			} else {
				color[n] = 0;
			}
		}

		variant obj;
		obj.serialize_from_string(area_node->attr("object"));
		areas_.push_back(area(r, color, obj));
	}
}

wml::node_ptr water::write() const
{
	wml::node_ptr result(new wml::node("water"));
	result->set_attr("zorder", formatter() << zorder_);
	foreach(const area& a, areas_) {
		wml::node_ptr node(new wml::node("area"));
		node->set_attr("rect", a.rect_.to_string());
		node->set_attr("color", formatter()
		               << static_cast<int>(a.color_[0]) << ","
		               << static_cast<int>(a.color_[1]) << ","
		               << static_cast<int>(a.color_[2]) << ","
		               << static_cast<int>(a.color_[3]));

		std::string obj;
		a.obj_.serialize_to_string(obj);
		node->set_attr("object", obj);

		result->add_child(node);
	}

	return result;
}

void water::add_rect(const rect& r, const unsigned char* color, variant obj)
{
	areas_.push_back(area(r, color, obj));
}

void water::delete_rect(const rect& r)
{
	for(std::vector<area>::iterator i = areas_.begin(); i != areas_.end(); ) {
		if(r == i->rect_) {
			i = areas_.erase(i);
		} else {
			++i;
		}
	}
}

void water::begin_drawing()
{
	foreach(area& a, areas_) {
		graphics::add_raster_distortion(&a.distortion_);
	}
}

void water::end_drawing() const
{
	foreach(const area& a, areas_) {
		graphics::remove_raster_distortion(&a.distortion_);
	}
}

/*
void water::set_surface_detection_rects(int zorder)
{
	const int offset = get_offset(zorder);
	foreach(area& a, areas_) {
		//detect drawing at the surface of the water.
		a.draw_detection_buf_.resize(a.rect_.w());
		memset(&a.draw_detection_buf_[0], 0, a.draw_detection_buf_.size());
		graphics::set_draw_detection_rect(rect(a.rect_.x(), a.rect_.y() + offset, a.rect_.w(), 1), &a.draw_detection_buf_[0]);
	}
}
*/

bool water::draw(int x, int y, int w, int h) const
{
	glShadeModel(GL_SMOOTH);

	bool result = false;
	foreach(const area& a, areas_) {
		if(draw_area(a, x, y, w, h)) {
			result = true;
		}
	}

	end_drawing();
	glShadeModel(GL_FLAT);

	return result;
}

void water::add_wave(const point& p, double xvelocity, double height, double length, double delta_height, double delta_length)
{
	foreach(area& a, areas_) {
		if(point_in_rect(p, a.rect_)) {
			std::pair<int, int> bounds(a.rect_.x(), a.rect_.x2());
			for(int n = 0; n != a.surface_segments_.size(); ++n) {
				if(p.x >= a.surface_segments_[n].first && p.x <= a.surface_segments_[n].second) {
					bounds = a.surface_segments_[n];
					break;
				}
			}
			wave wv = { p.x, xvelocity, height, length, delta_height, delta_length, bounds.first, bounds.second };
			a.waves_.push_back(wv);
			return;
		}
	}
}

bool water::draw_area(const water::area& a, int x, int y, int w, int h) const
{
	const graphics::color waterline_color(250, 240, 205, 255);
	const graphics::color shallowwater_color(0, 51, 61, 140);
	const graphics::color deepwater_color(0, 51, 61, 153);
	const SDL_Rect waterline_rect = {a.rect_.x(), a.rect_.y(), a.rect_.w(), 2};
	const SDL_Rect underwater_rect = {a.rect_.x(), a.rect_.y(), a.rect_.w(), a.rect_.h()};

	unsigned char water_color[] = {a.color_[0], a.color_[1], a.color_[2], a.color_[3]};
	
	glBlendFunc(GL_ONE, GL_ONE);
	#if defined(TARGET_OS_HARMATTAN) || defined(TARGET_PANDORA) || defined(TARGET_TEGRA)
	if (glBlendEquationOES)
		glBlendEquationOES(GL_FUNC_REVERSE_SUBTRACT_OES);
	#elif defined(GL_OES_blend_subtract)
	glBlendEquationOES(GL_FUNC_REVERSE_SUBTRACT_OES);
	#else
	if(GLEW_EXT_blend_equation_separate && (GLEW_ARB_imaging || GLEW_VERSION_1_4)) {
		glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
	} else {
		const int max_color = std::max(water_color[0], std::max(water_color[1], water_color[2]));
		water_color[0] = (max_color - water_color[0])/8;
		water_color[1] = (max_color - water_color[1])/8;
		water_color[2] = (max_color - water_color[2])/8;
	}
	#endif
	glDisable(GL_TEXTURE_2D);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	
	GLfloat vertices[] = {
		waterline_rect.x, waterline_rect.y, //shallow water colored
		waterline_rect.x + waterline_rect.w, waterline_rect.y,
		
		waterline_rect.x, waterline_rect.y + 100, //deep water colored
		waterline_rect.x + waterline_rect.w, waterline_rect.y + 100,
		waterline_rect.x, underwater_rect.y + underwater_rect.h,
		waterline_rect.x + waterline_rect.w, underwater_rect.y + underwater_rect.h
	};

#if defined(TARGET_TEGRA)	
	glColor4ub (0,0,0,255); // tegra linux drivers have some issues
#else
	glColor4ub(water_color[0], water_color[1], water_color[2], water_color[3]);
#endif	
	glVertexPointer(2, GL_FLOAT, 0, vertices);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, sizeof(vertices)/sizeof(GLfloat)/2);
	#if defined(TARGET_OS_HARMATTAN) || defined(TARGET_PANDORA) || defined(TARGET_TEGRA)
	if (glBlendEquationOES)
		glBlendEquationOES(GL_FUNC_ADD_OES);
	#elif defined(GL_OES_blend_subtract)
	glBlendEquationOES(GL_FUNC_ADD_OES);
	#else
	if (GLEW_EXT_blend_equation_separate && (GLEW_ARB_imaging || GLEW_VERSION_1_4))
		glBlendEquation(GL_FUNC_ADD);
	#endif
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glLineWidth(2.0);

	typedef std::pair<int, int> Segment;

	glEnableClientState(GL_COLOR_ARRAY);

	const int EndSegmentSize = 20;

	foreach(const Segment& seg, a.surface_segments_) {
		glColor4f(1.0, 1.0, 1.0, 1.0);
		GLfloat varray[] = {
			seg.first - EndSegmentSize, waterline_rect.y,
			seg.first, waterline_rect.y,
			seg.second, waterline_rect.y,
			seg.second + EndSegmentSize, waterline_rect.y,
		};
		static const unsigned char vcolors[] = {
			255, 255, 255, 0,
			255, 255, 255, 255,
			255, 255, 255, 255,
			255, 255, 255, 0,
		};
		glVertexPointer(2, GL_FLOAT, 0, varray);
		glColorPointer(4, GL_UNSIGNED_BYTE, 0, vcolors);
		glDrawArrays(GL_LINE_STRIP, 0, 4);
	
		//draw a second line, in a different color, just below the first
		glColor4f(0.0, 0.9, 0.75, 0.5);
		GLfloat varray2[] = {
			seg.first - EndSegmentSize, waterline_rect.y+2,
			seg.first, waterline_rect.y+2,
			seg.second, waterline_rect.y+2,
			seg.second + EndSegmentSize, waterline_rect.y+2,
		};
		static const unsigned char vcolors2[] = {
			0, 230, 200, 0,
			0, 230, 200, 128,
			0, 230, 200, 128,
			0, 230, 200, 0,
		};
		glVertexPointer(2, GL_FLOAT, 0, varray2);
		glColorPointer(4, GL_UNSIGNED_BYTE, 0, vcolors2);
		glDrawArrays(GL_LINE_STRIP, 0, 4);
	}

	glDisableClientState(GL_COLOR_ARRAY);

	glColor4f(1.0, 1.0, 1.0, 1.0);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnable(GL_TEXTURE_2D);

	return true;
}

namespace {
bool wave_dead(const water::wave& w) {
	return w.height <= 0.5 || w.length <= 0;
}
}

void water::process(const level& lvl)
{
	foreach(area& a, areas_) {
		init_area_surface_segments(lvl, a);

		a.distortion_ = graphics::water_distortion(lvl.cycle(), a.rect_);
		foreach(wave& w, a.waves_) {
			w.process();

			//if the wave has hit the edge, then turn it around.
			if(w.xpos < w.left_bound && w.xvelocity < 0) {
				w.xvelocity *= -1.0;
			}

			if(w.xpos > w.right_bound && w.xvelocity > 0) {
				w.xvelocity *= -1.0;
			}
		}

		a.waves_.erase(std::remove_if(a.waves_.begin(), a.waves_.end(), wave_dead), a.waves_.end());
	}
}

void water::wave::process() {
	xpos += xvelocity;
	height *= 0.996;
	length += delta_length;
}

void water::get_current(const entity& e, int* velocity_x, int* velocity_y) const
{
	if(velocity_x && current_x_formula_) {
		*velocity_x += current_x_formula_->execute(e).as_int();
	}

	if(velocity_y && current_y_formula_) {
		*velocity_y += current_y_formula_->execute(e).as_int();
	}
}

bool water::is_underwater(const rect& r, rect* result_water_area, variant* e) const
{
	//we don't take the vertical midpoint, because doing so can cause problems
	//when objects change their animations and flip between not being
	//underwater. Instead we take the bottom and subtract a hardcoded amount.
	//TODO: potentially review this way of determinining if something is
	//underwater.
	const point p((r.x() + r.x2())/2, r.y2() - 20);
	foreach(const area& a, areas_) {
		if(point_in_rect(p, a.rect_)) {
			if(result_water_area) {
				*result_water_area = a.rect_;
			}

			if(e) {
				*e = a.obj_;
			}
			return true;
		}
	}

	return false;
}

void water::init_area_surface_segments(const level& lvl, water::area& a)
{
	if(a.surface_segments_init_) {
		return;
	}

	a.surface_segments_init_ = true;

	bool prev_solid = true;
	int begin_segment = 0;
	for(int x = a.rect_.x(); x != a.rect_.x2(); ++x) {
		const bool solid = lvl.solid(x, a.rect_.y()) || x == a.rect_.x2()-1;
		if(solid && !prev_solid) {
			a.surface_segments_.push_back(std::make_pair(begin_segment, x));
		} else if(!solid && prev_solid) {
			begin_segment = x;
		}

		prev_solid = solid;
	}
}

water::area::area(const rect& r, const unsigned char* pcolor, variant obj)
  : rect_(r), distortion_(0, rect(0,0,0,0)), surface_segments_init_(false), obj_(obj)
{
	for(int n = 0; n != 4; ++n) {
		color_[n] = pcolor[n];
	}
}
