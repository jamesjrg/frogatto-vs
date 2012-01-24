#include <SDL.h>
#ifndef SDL_VIDEO_OPENGL_ES
#include <GL/glew.h>
#endif

#include <math.h>

#include <iostream>
#include <map>

#include "background.hpp"
#include "color_utils.hpp"
#include "filesystem.hpp"
#include "foreach.hpp"
#include "formatter.hpp"
#include "level.hpp"
#include "preferences.hpp"
#include "raster.hpp"
#include "surface_palette.hpp"
#include "wml_node.hpp"
#include "wml_parser.hpp"
#include "wml_utils.hpp"

namespace {
//a cache key with background name and palette ID.
typedef std::pair<std::string, int> cache_key;
std::map<cache_key, boost::shared_ptr<background> > cache;
}

boost::shared_ptr<background> background::get(const std::string& name, int palette_id)
{
	const cache_key id(name, palette_id);

	boost::shared_ptr<background>& obj = cache[id];
	if(!obj) {
		obj.reset(new background(wml::parse_wml_from_file("data/backgrounds/" + name + ".cfg"), palette_id));
		obj->id_ = name;
	}

	return obj;
}

std::vector<std::string> background::get_available_backgrounds()
{
	std::vector<std::string> files;
	sys::get_files_in_dir("data/backgrounds/", &files);

	std::vector<std::string> result;
	foreach(const std::string& fname, files) {
		if(fname.size() > 4 && std::equal(fname.end() - 4, fname.end(), ".cfg")) {
			result.push_back(std::string(fname.begin(), fname.end() - 4));
		}
	}

	return result;
}

background::background(const wml::const_node_ptr& node, int palette) : palette_(palette)
{
	top_ = string_to_color(node->attr("top"));
	bot_ = string_to_color(node->attr("bottom"));

	if(palette_ != -1) {
		top_ = graphics::map_palette(top_, palette);
		bot_ = graphics::map_palette(bot_, palette);
	}

	width_ = wml::get_int(node, "width");
	height_ = wml::get_int(node, "height");

	FOREACH_WML_CHILD(layer_node, node, "layer") {
		layer bg;
		bg.image = (*layer_node)["image"];
		bg.image_formula = layer_node->attr("image_formula");
		bg.xscale = wml::get_int(layer_node, "xscale", 100);
		bg.yscale = wml::get_int(layer_node, "yscale", 100);
		bg.xspeed = wml::get_int(layer_node, "xspeed", 0);
		bg.xpad = wml::get_int(layer_node, "xpad", 0);
		bg.xoffset = wml::get_int(layer_node, "xoffset", 0);
		bg.yoffset = wml::get_int(layer_node, "yoffset", 0);
		bg.scale = wml::get_int(layer_node, "scale", 1);
		bg.blend = wml::get_bool(layer_node, "blend", true);
		if(bg.scale < 1) {
			bg.scale = 1;
		}

#ifndef SDL_VIDEO_OPENGL_ES
		std::string blend_mode = layer_node->attr("mode");
		if(GLEW_EXT_blend_minmax) {
			if(blend_mode == "GL_MAX") {
				bg.mode = GL_MAX;
			} else if(blend_mode == "GL_MIN") {
				bg.mode = GL_MIN;
			} else {
				bg.mode = GL_FUNC_ADD;
			}
		}
#endif
		
		std::fill(bg.color, bg.color + 4, 0.0);
		bg.color[0] = wml::get_attr<GLfloat>(layer_node, "red", 1.0);
		bg.color[1] = wml::get_attr<GLfloat>(layer_node, "green", 1.0);
		bg.color[2] = wml::get_attr<GLfloat>(layer_node, "blue", 1.0);
		bg.color[3] = wml::get_attr<GLfloat>(layer_node, "alpha", 1.0);

		if(layer_node->has_attr("color_above")) {
			bg.color_above.reset(new SDL_Color);
			*bg.color_above = string_to_color(layer_node->attr("color_above"));
			if(palette_ != -1) {
				*bg.color_above = graphics::map_palette(*bg.color_above, palette);
			}
		}

		if(layer_node->has_attr("color_below")) {
			bg.color_below.reset(new SDL_Color);
			*bg.color_below = string_to_color(layer_node->attr("color_below"));
			if(palette_ != -1) {
				*bg.color_below = graphics::map_palette(*bg.color_below, palette);
			}
		}

		bg.y1 = wml::get_attr<int>(layer_node, "y1");
		bg.y2 = wml::get_attr<int>(layer_node, "y2");

		bg.foreground = wml::get_bool(layer_node, "foreground", false);
		bg.tile_upwards = wml::get_bool(layer_node, "tile_upwards", false);
		bg.tile_downwards = wml::get_bool(layer_node, "tile_downwards", false);
		layers_.push_back(bg);
	}
}

wml::node_ptr background::write() const
{
	wml::node_ptr res(new wml::node("background"));
	char buf[128];
	sprintf(buf, "%02x%02x%02x", top_.r, top_.g, top_.b);
	res->set_attr("top", buf);
	sprintf(buf, "%02x%02x%02x", bot_.r, bot_.g, bot_.b);
	res->set_attr("bottom", buf);
	res->set_attr("width", formatter() << width_);
	res->set_attr("height", formatter() << height_);

	foreach(const layer& bg, layers_) {
		wml::node_ptr node(new wml::node("layer"));
		node->set_attr("image", bg.image);
		node->set_attr("xscale", formatter() << bg.xscale);
		node->set_attr("yscale", formatter() << bg.yscale);
		node->set_attr("xspeed", formatter() << bg.xspeed);
		node->set_attr("xpad", formatter() << bg.xpad);
		node->set_attr("xoffset", formatter() << bg.xoffset);
		node->set_attr("yoffset", formatter() << bg.yoffset);
		node->set_attr("y1", formatter() << bg.y1);
		node->set_attr("y2", formatter() << bg.y2);
		node->set_attr("scale", formatter() << bg.scale);
		node->set_attr("red", formatter() << bg.color[0]);
		node->set_attr("green", formatter() << bg.color[1]);
		node->set_attr("blue", formatter() << bg.color[2]);
		node->set_attr("alpha", formatter() << bg.color[3]);

		if(bg.color_above) {
			sprintf(buf, "%02x%02x%02x", bg.color_above->r, bg.color_above->g, bg.color_above->b);
			node->set_attr("color_above", buf);
		}

		if(bg.color_below) {
			sprintf(buf, "%02x%02x%02x", bg.color_below->r, bg.color_below->g, bg.color_below->b);
			node->set_attr("color_below", buf);
		}
		if(bg.foreground) {
			node->set_attr("foreground", "true");
		}

		if(bg.tile_upwards) {
			node->set_attr("tile_upwards", "true");
		}

		if(bg.tile_downwards) {
			node->set_attr("tile_downwards", "true");
		}

		res->add_child(node);
	}
	return res;
}

void background::draw(int x, int y, const rect& area, const std::vector<rect>& opaque_areas, int rotation, int cycle) const
{
	const int height = height_ + offset_.y*2;

	//set the background colors for the level. The area above 'height' is
	//painted with the top color, and the area below height is painted with
	//the bottom color. For efficiency we do this using color clearing, with
	//scissors to divide the screen into top and bottom.
	if(height < y) {
		//the entire screen is full of the bottom color
		glClearColor(bot_.r/255.0, bot_.g/255.0, bot_.b/255.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);
	} else if(height > y + graphics::screen_height()) {
		//the entire screen is full of the top color.
		glClearColor(top_.r/255.0, top_.g/255.0, top_.b/255.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);
	} else {
		//both bottom and top colors are on the screen, so draw them both,
		//using scissors to delinate their areas.
		const int dist_from_bottom = y + graphics::screen_height() - height;

		glEnable(GL_SCISSOR_TEST);

		const int scissor_scale = preferences::double_scale() ? 2 : 1;

		//the scissor test does not respect any rotations etc. We use a rotation
		//to transform the iPhone's display, which is fine normally, but
		//here we have to accomodate the iPhone being "on its side"
#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
		glScissor(dist_from_bottom/scissor_scale, 0, (graphics::screen_height() - dist_from_bottom)/scissor_scale, graphics::screen_width()/scissor_scale);
#else
		glScissor(0, dist_from_bottom, preferences::actual_screen_width(), preferences::actual_screen_width()*(1-dist_from_bottom/600));
#endif
		glClearColor(top_.r/255.0, top_.g/255.0, top_.b/255.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);

#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
		glScissor(0, 0, dist_from_bottom/scissor_scale, graphics::screen_width()/scissor_scale);
#else
		glScissor(0, 0, preferences::actual_screen_width(), dist_from_bottom);
#endif
		glClearColor(bot_.r/255.0, bot_.g/255.0, bot_.b/255.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);

		glDisable(GL_SCISSOR_TEST);
	}

	draw_layers(x, y, area, opaque_areas, rotation, cycle);
}

namespace {
graphics::blit_queue blit_queue;

void calculate_draw_areas(rect area, std::vector<rect>::const_iterator opaque1, std::vector<rect>::const_iterator opaque2, std::vector<rect>* areas) {
	if(opaque1 == opaque2) {
		areas->push_back(area);
		return;
	}

	rect sub_areas[4];
	for(; opaque1 != opaque2; ++opaque1) {
		const int result = rect_difference(area, *opaque1, sub_areas);
		if(result == -1) {
			continue;
		}

		if(result != 1) {
			for(int n = 0; n < result; ++n) {

				calculate_draw_areas(sub_areas[n], opaque1+1, opaque2, areas);
			}

			return;
		}

		area = sub_areas[0];
	}

	areas->push_back(area);
}

}

void background::draw_layers(int x, int y, const rect& area_ref, const std::vector<rect>& opaque_areas, int rotation, int cycle) const
{
	static std::vector<rect> areas;
	areas.clear();
	calculate_draw_areas(area_ref, opaque_areas.begin(), opaque_areas.end(), &areas);

	for(std::vector<layer>::const_iterator i = layers_.begin(); i != layers_.end(); ++i) {
		const layer& bg = *i;
		if(bg.foreground == false) {

			for(std::vector<rect>::const_iterator a = areas.begin(); a != areas.end(); ++a) {
				draw_layer(x, y, *a, rotation, bg, cycle);
			}

			if(!blit_queue.empty() && (i+1 == layers_.end() || i->texture != (i+1)->texture || (i+1)->foreground || i->blend != (i+1)->blend)) {
				if(bg.blend == false) {
					glDisable(GL_BLEND);
				}
				blit_queue.set_texture(bg.texture.get_id());
				blit_queue.do_blit();
				blit_queue.clear();
				if(bg.blend == false) {
					glEnable(GL_BLEND);
				}
			}

		}
	}
}

void background::draw_foreground(double xpos, double ypos, int rotation, int cycle) const
{
	foreach(const layer& bg, layers_) {
		if(bg.foreground) {
			draw_layer(xpos, ypos, rect(xpos, ypos, graphics::screen_width(), graphics::screen_height()), rotation, bg, cycle);
			if(!blit_queue.empty()) {
				blit_queue.set_texture(bg.texture.get_id());
				blit_queue.do_blit();
				blit_queue.clear();
			}
		}
	}
}

void background::set_offset(const point& offset)
{
	offset_ = offset;
}

void background::draw_layer(int x, int y, const rect& area, int rotation, const background::layer& bg, int cycle) const
{
	const double ScaleImage = 2.0;
	GLshort y1 = y + (bg.yoffset+offset_.y)*ScaleImage - (y*bg.yscale)/100;
	GLshort y2 = y1 + (bg.y2 - bg.y1)*ScaleImage;

	if(!bg.tile_downwards && y2 <= y) {
		return;
	}

	if(!bg.tile_downwards && y2 <= area.y()) {
		return;
	}

	if(!bg.tile_upwards && y1 > area.y2()) {
		return;
	}

	if(!bg.texture.valid()) {
		if(palette_ == -1) {
			bg.texture = graphics::texture::get(bg.image, bg.image_formula);
		} else {
			bg.texture = graphics::texture::get_palette_mapped(bg.image, palette_);
		}

		if(bg.y2 == 0) {
			bg.y2 = bg.texture.height();
		}
	}

	if(!bg.texture.valid()) {
		return;
	}

	ASSERT_GT(bg.texture.height(), 0);
	ASSERT_GT(bg.texture.width(), 0);

	GLfloat v1 = bg.texture.translate_coord_y(double(bg.y1)/double(bg.texture.height()));
	GLfloat v2 = bg.texture.translate_coord_y(double(bg.y2)/double(bg.texture.height()));

	if(y1 < area.y()) {
		v1 += (GLfloat(area.y() - y1)/GLfloat(y2 - y1))*(v2 - v1);
		y1 = area.y();
	}

	if(bg.tile_upwards && y1 > area.y()) {
		v1 -= (GLfloat(y1 - area.y())/GLfloat(y2 - y1))*(v2 - v1);
		y1 = area.y();
	} else if(bg.color_above && y1 > area.y()) {
		glEnable(GL_SCISSOR_TEST);

		const int xpos = area.x() - x;
		const int ypos = y1 - y;
		const int width = area.w();
		const int height = y1 - area.y();
#if TARGET_OS_IPHONE
		glScissor((graphics::screen_height() - ypos)/2, (graphics::screen_width() - (xpos + width))/2, height/2, width/2);
#else
		glScissor(xpos, graphics::screen_height() - ypos, width, height);
#endif
		glClearColor(bg.color_above->r, bg.color_above->g, bg.color_above->b, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);
		glDisable(GL_SCISSOR_TEST);
	}

	if(bg.tile_downwards && y2 < area.y() + area.h()) {
		v2 += (GLfloat((area.y() + area.h()) - y2)/GLfloat(y2 - y1))*(v2 - v1);
		y2 = area.y() + area.h();
	} else if(bg.color_below && y2 < area.y() + area.h()) {
		glEnable(GL_SCISSOR_TEST);

		const int xpos = area.x() - x;
		const int ypos = area.y() + area.h() - y;
		const int width = area.w();
		const int height = area.y() + area.h() - y2;

#if TARGET_OS_IPHONE
		glScissor((graphics::screen_height() - ypos)/2, (graphics::screen_width() - (xpos + width))/2, height/2, width/2);
#else
		glScissor(xpos, graphics::screen_height() - ypos, width, height);
#endif

		glClearColor(bg.color_below->r, bg.color_below->g, bg.color_below->b, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);
		glDisable(GL_SCISSOR_TEST);
	}

	if(y2 > area.y() + area.h()) {
		v2 -= (GLfloat(y2 - (area.y() + area.h()))/GLfloat(y2 - y1))*(v2 - v1);
		y2 = area.y() + area.h();
	}

	if(y2 <= y1) {
		return;
	}

	if(v1 > v2) {
		return;
	}

	//clamp [v1, v2] into the [0.0, 1.0] range
	if(v1 > 1.0) {
		const GLfloat f = floor(v1);
		v1 -= f;
		v2 -= f;
	}

	if(v1 < 0.0) {
		const GLfloat diff = v2 - v1;
		const GLfloat f = floor(-v1);
		v1 = 1.0 - (-v1 - f);
		v2 = v1 + diff;
	}

	int screen_width = area.w();

	const double xscale = double(bg.xscale)/100.0;
	GLfloat xpos = (-GLfloat(bg.xspeed)*GLfloat(cycle)/1000 + int(GLfloat(x + bg.xoffset)*xscale))/GLfloat((bg.texture.width()+bg.xpad)*ScaleImage) + GLfloat(area.x() - x)/GLfloat((bg.texture.width()+bg.xpad)*ScaleImage);

	//clamp xpos into the [0.0, 1.0] range
	if(xpos > 0) {
		xpos -= floor(xpos);
	} else {
		while(xpos < 0) { xpos += 1.0; }
		//xpos += ceil(-xpos);
	}

	if(bg.xpad > 0) {
		xpos *= GLfloat(bg.texture.width() + bg.xpad)/GLfloat(bg.texture.width());
	}
	
	glColor4f(bg.color[0], bg.color[1], bg.color[2], bg.color[3]);

#ifndef SDL_VIDEO_OPENGL_ES
	if (GLEW_EXT_blend_minmax && (GLEW_ARB_imaging || GLEW_VERSION_1_4)) {
		glBlendEquation(bg.mode);
	}
#endif

	x = area.x();
	y = area.y();

	while(screen_width > 0) {
		const int texture_blit_width = (1.0 - xpos)*bg.texture.width()*ScaleImage;

		const int blit_width = std::min(texture_blit_width, screen_width);

		if(blit_width > 0) {
			const GLfloat xpos2 = xpos + GLfloat(blit_width)/(GLfloat(bg.texture.width())*2.0);

			const GLshort x1 = x;
			const GLshort x2 = x1 + blit_width;

			const GLfloat u1 = bg.texture.translate_coord_x(xpos);
			const GLfloat u2 = bg.texture.translate_coord_x(xpos2);

			blit_queue.repeat_last();
			blit_queue.add(x1, y1, u1, v1);
			blit_queue.repeat_last();
			blit_queue.add(x2, y1, u2, v1);
			blit_queue.add(x1, y2, u1, v2);
			blit_queue.add(x2, y2, u2, v2);
		}

		x += blit_width + bg.xpad*ScaleImage;

		xpos = 0.0;
		screen_width -= blit_width + bg.xpad*ScaleImage;
	}

	glColor4f(1.0,1.0,1.0,1.0);
#ifndef SDL_VIDEO_OPENGL_ES
	if (GLEW_EXT_blend_minmax && (GLEW_ARB_imaging || GLEW_VERSION_1_4)) {
		glBlendEquation(GL_FUNC_ADD);
	}
#endif
}

