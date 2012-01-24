#include "SDL.h"

#include <boost/shared_ptr.hpp>

#include <map>
#include <vector>
#include <sstream>

#include "asserts.hpp"
#include "custom_object_type.hpp"
#include "filesystem.hpp"
#include "formatter.hpp"
#include "frame.hpp"
#include "geometry.hpp"
#include "string_utils.hpp"
#include "surface.hpp"
#include "surface_cache.hpp"
#include "unit_test.hpp"
#include "wml_node.hpp"
#include "wml_parser.hpp"
#include "wml_utils.hpp"
#include "wml_writer.hpp"
#include "IMG_savepng.h"

namespace {
const int TextureImageSize = 1024;

struct animation_area {
	explicit animation_area(wml::node_ptr node) : anim(new frame(node)), is_particle(false)
	{
		width = 0;
		height = 0;
		foreach(const frame::frame_info& f, anim->frame_layout()) {
			width += f.area.w();
			if(f.area.h() > height) {
				height = f.area.h();
			}
		}

		src_image = node->attr("image");
		dst_image = -1;
	}
	    
	boost::shared_ptr<frame> anim;
	int width, height;

	std::string src_image;

	int dst_image;
	rect dst_area;
	bool is_particle;
};

typedef boost::shared_ptr<animation_area> animation_area_ptr;

bool operator==(const animation_area& a, const animation_area& b)
{
	return a.src_image == b.src_image && a.anim->area() == b.anim->area() && a.anim->pad() == b.anim->pad() && a.anim->num_frames() == b.anim->num_frames() && a.anim->num_frames_per_row() == b.anim->num_frames_per_row();
}

bool animation_area_height_compare(animation_area_ptr a, animation_area_ptr b)
{
	if(a->is_particle != b->is_particle) {
		return a->is_particle;
	}
	return a->height > b->height;
}

struct output_area {
	explicit output_area(int n) : image_id(n)
	{
		area = rect(0, 0, TextureImageSize, TextureImageSize);
	}
	int image_id;
	rect area;
};

rect use_output_area(const output_area& input, int width, int height, std::vector<output_area>& areas)
{
	ASSERT_LE(width, input.area.w());
	ASSERT_LE(height, input.area.h());
	rect result(input.area.x(), input.area.y(), width, height);
	if(input.area.h() > height) {
		areas.push_back(output_area(input.image_id));
		areas.back().area = rect(input.area.x(), input.area.y() + height, input.area.w(), input.area.h() - height);
	}

	if(input.area.w() > width) {
		areas.push_back(output_area(input.image_id));
		areas.back().area = rect(input.area.x() + width, input.area.y(), input.area.w() - width, height);
	}

	return result;
}

}

namespace graphics {
void set_alpha_for_transparent_colors_in_rgba_surface(SDL_Surface* s);
}

UTILITY(object_compiler)
{
	using graphics::surface;

	int num_output_images = 0;
	std::vector<output_area> output_areas;
	output_areas.push_back(output_area(num_output_images++));

	std::map<wml::node_ptr, std::string> nodes_to_files;

	std::vector<wml::node_ptr> objects;
	std::vector<animation_area_ptr> animation_areas;
	std::map<wml::node_ptr, animation_area_ptr> nodes_to_animation_areas;

	std::vector<wml::node_ptr> animation_containing_nodes;
	std::vector<std::string> no_compile_images;

	wml::node_ptr gui_node = wml::parse_wml_from_file("data/gui.cfg");
	animation_containing_nodes.push_back(gui_node);

	std::map<std::string, wml::node_ptr> gui_nodes;
	std::vector<std::string> gui_files;
	sys::get_files_in_dir("data/gui", &gui_files);
	foreach(const std::string& gui, gui_files) {
		if(gui[0] == '.') {
			continue;
		}

		gui_nodes[gui] = wml::parse_wml_from_file("data/gui/" + gui);
		animation_containing_nodes.push_back(gui_nodes[gui]);
		if(gui_nodes[gui]->has_attr("no_compile_image")) {
			std::vector<std::string> images = util::split(gui_nodes[gui]->attr("no_compile_image"));
			no_compile_images.insert(no_compile_images.end(), images.begin(), images.end());
		}
	}

	std::vector<const_custom_object_type_ptr> types = custom_object_type::get_all();
	foreach(const_custom_object_type_ptr type, types) {
		const std::string* path = custom_object_type::get_object_path(type->id() + ".cfg");

		//skip any experimental stuff so it isn't compiled
		const std::string Experimental = "experimental";
		if(std::search(path->begin(), path->end(), Experimental.begin(), Experimental.end()) != path->end()) {
			continue;
		}

		std::cerr << "OBJECT: " << type->id() << " -> " << *path << "\n";
		wml::node_ptr obj_node =  wml::parse_wml_from_file(*path);
		obj_node = custom_object_type::merge_prototype(obj_node);
		obj_node->erase_attr("prototype");
		objects.push_back(obj_node);
		nodes_to_files[obj_node] = "data/compiled/objects/" + type->id() + ".cfg";


		if(obj_node->has_attr("no_compile_image")) {
			std::vector<std::string> images = util::split(obj_node->attr("no_compile_image"));
			no_compile_images.insert(no_compile_images.end(), images.begin(), images.end());
		}

		animation_containing_nodes.push_back(obj_node);

		for(wml::node::child_iterator i = obj_node->begin_child("particle_system"); i != obj_node->end_child("particle_system"); ++i) {
			animation_containing_nodes.push_back(i->second);
		}

		//add nested objects -- disabled for now until we find bugs in it.
		/*
		for(wml::node::child_iterator i = obj_node->begin_child("object_type"); i != obj_node->end_child("object_type"); ++i) {
			animation_containing_nodes.push_back(i->second);
		}
		*/
	}

	foreach(wml::node_ptr& node, animation_containing_nodes) {
		for(wml::node::all_child_iterator i = node->begin_children();
		    i != node->end_children(); ++i) {
			if((*i)->name() != "animation" && (*i)->name() != "framed_gui_element" && (*i)->name() != "section") {
				continue;
			}

			animation_area_ptr anim(new animation_area(*i));
			if(anim->src_image.empty() || (*i)->has_attr("palettes") || std::find(no_compile_images.begin(), no_compile_images.end(), anim->src_image) != no_compile_images.end()) {
				continue;
			}

			animation_areas.push_back(anim);

			foreach(animation_area_ptr p, animation_areas) {
				if(*p == *anim) {
					anim = p;
					break;
				}
			}

			if(node->name() == "particle_system") {
				anim->is_particle = true;
			}

			if(anim != animation_areas.back()) {
				animation_areas.pop_back();
			}
	
			nodes_to_animation_areas[*i] = anim;
		}
	}

	std::sort(animation_areas.begin(), animation_areas.end(), animation_area_height_compare);

	foreach(animation_area_ptr anim, animation_areas) {
		ASSERT_LE(anim->width, 1024);
		ASSERT_LE(anim->height, 1024);
		int match = -1;
		int match_diff = -1;
		for(int n = 0; n != output_areas.size(); ++n) {
			if(anim->width <= output_areas[n].area.w() && anim->height <= output_areas[n].area.h()) {
				const int diff = output_areas[n].area.w()*output_areas[n].area.h() - anim->width*anim->height;
				if(match == -1 || diff < match_diff) {
					match = n;
					match_diff = diff;
				}
			}
		}

		if(match == -1) {
			match = output_areas.size();
			output_areas.push_back(output_area(num_output_images++));
		}

		output_area match_area = output_areas[match];
		output_areas.erase(output_areas.begin() + match);
		rect area = use_output_area(match_area, anim->width, anim->height, output_areas);
		anim->dst_image = match_area.image_id;
		anim->dst_area = area;
	}

	std::vector<surface> surfaces;
	for(int n = 0; n != num_output_images; ++n) {
		surfaces.push_back(surface(SDL_CreateRGBSurface(SDL_SWSURFACE,TextureImageSize,TextureImageSize,32,SURFACE_MASK)));
	}

	foreach(animation_area_ptr anim, animation_areas) {
		foreach(animation_area_ptr other, animation_areas) {
			if(anim == other || anim->dst_image != other->dst_image) {
				continue;
			}

			ASSERT_LOG(rects_intersect(anim->dst_area, other->dst_area) == false, "RECTANGLES CLASH: " << anim->dst_image << " " << anim->dst_area << " vs " << other->dst_area);
		}

		ASSERT_INDEX_INTO_VECTOR(anim->dst_image, surfaces);
		surface dst = surfaces[anim->dst_image];
		surface src = graphics::surface_cache::get(anim->src_image);
		ASSERT_LOG(src.get() != NULL, "COULD NOT LOAD IMAGE: '" << anim->src_image << "'");
		int xdst = 0;
		for(int f = 0; f != anim->anim->num_frames(); ++f) {
			const frame::frame_info& info = anim->anim->frame_layout()[f];

			const int x = f%anim->anim->num_frames_per_row();
			const int y = f/anim->anim->num_frames_per_row();

			const rect& base_area = anim->anim->area();
			const int xpos = base_area.x() + (base_area.w()+anim->anim->pad())*x;
			const int ypos = base_area.y() + (base_area.h()+anim->anim->pad())*y;
			SDL_Rect blit_src = {xpos + info.x_adjust, ypos + info.y_adjust, info.area.w(), info.area.h()};
			SDL_Rect blit_dst = {anim->dst_area.x() + xdst,
			                     anim->dst_area.y(),
								 info.area.w(), info.area.h()};
			xdst += info.area.w();
			ASSERT_GE(blit_dst.x, anim->dst_area.x());
			ASSERT_GE(blit_dst.y, anim->dst_area.y());
			ASSERT_LE(blit_dst.x + blit_dst.w, anim->dst_area.x() + anim->dst_area.w());
			ASSERT_LE(blit_dst.y + blit_dst.h, anim->dst_area.y() + anim->dst_area.h());
			SDL_SetAlpha(src.get(), 0, SDL_ALPHA_OPAQUE);
			SDL_BlitSurface(src.get(), &blit_src, dst.get(), &blit_dst);
		}
	}

	for(int n = 0; n != num_output_images; ++n) {
		std::ostringstream fname;
		fname << "images/compiled-" << n << ".png";

		graphics::set_alpha_for_transparent_colors_in_rgba_surface(surfaces[n].get());

		IMG_SavePNG(fname.str().c_str(), surfaces[n].get(), -1);
	}

	typedef std::pair<wml::node_ptr, animation_area_ptr> anim_pair;
	foreach(const anim_pair& a, nodes_to_animation_areas) {
		wml::node_ptr node = a.first;
		animation_area_ptr anim = a.second;
		std::ostringstream fname;
		fname << "compiled-" << anim->dst_image << ".png";
		node->set_attr("image", fname.str());
		node->erase_attr("x");
		node->erase_attr("y");
		node->erase_attr("w");
		node->erase_attr("h");
		node->erase_attr("pad");

		const frame::frame_info& first_frame = anim->anim->frame_layout().front();
		
		rect r(anim->dst_area.x() - first_frame.x_adjust, anim->dst_area.y() - first_frame.y_adjust, anim->anim->area().w(), anim->anim->area().h());
		node->set_attr("rect", r.to_string());

		int xpos = anim->dst_area.x();

		std::vector<int> v;
		foreach(const frame::frame_info& f, anim->anim->frame_layout()) {
			ASSERT_EQ(f.area.w() + f.x_adjust + f.x2_adjust, anim->anim->area().w());
			ASSERT_EQ(f.area.h() + f.y_adjust + f.y2_adjust, anim->anim->area().h());
			v.push_back(f.x_adjust);
			v.push_back(f.y_adjust);
			v.push_back(f.x2_adjust);
			v.push_back(f.y2_adjust);
			v.push_back(xpos);
			v.push_back(anim->dst_area.y());
			v.push_back(f.area.w());
			v.push_back(f.area.h());

			xpos += f.area.w();
		}

		std::vector<std::string> vs;
		foreach(int n, v) {
			vs.push_back(formatter() << n);
		}

		node->set_attr("frame_info", util::join(vs));
	}

	for(std::map<wml::node_ptr, std::string>::iterator i = nodes_to_files.begin(); i != nodes_to_files.end(); ++i) {
		wml::node_ptr node = i->first;
		node->strip_prettiness();
		sys::write_file(i->second, wml::output(node));
	}

	sys::write_file("data/compiled/gui.cfg", wml::output(gui_node));

	for(std::map<std::string, wml::node_ptr>::iterator i = gui_nodes.begin();
	    i != gui_nodes.end(); ++i) {
		sys::write_file("data/compiled/gui/" + i->first, wml::output(i->second));
	}
}
