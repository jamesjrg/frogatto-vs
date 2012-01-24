#include <boost/intrusive_ptr.hpp>

#include <string>

#include "IMG_savepng.h"
#include "level.hpp"
#include "texture_frame_buffer.hpp"
#include "unit_test.hpp"

UTILITY(render_level)
{
	if(args.size() != 2) {
		std::cerr << "render_level usage: <level> <output_file>\n";
		return;
	}

	std::string file = args[0];
	std::string output = args[1];

	boost::intrusive_ptr<level> lvl(new level(file));
	lvl->set_editor();
	lvl->finish_loading();

	const int lvl_width = lvl->boundaries().w();
	const int lvl_height = lvl->boundaries().h();

	const int seg_width = graphics::screen_width();
	const int seg_height = graphics::screen_height();

	graphics::surface level_surface(SDL_CreateRGBSurface(SDL_SWSURFACE, lvl_width, lvl_height, 24, SURFACE_MASK_RGB));

	texture_frame_buffer::init(seg_width, seg_height);

	for(int y = lvl->boundaries().y(); y < lvl->boundaries().y2(); y += seg_height) {
		for(int x = lvl->boundaries().x(); x < lvl->boundaries().x2(); x += seg_width) {
			texture_frame_buffer::render_scope scope;
			graphics::prepare_raster();
			glClearColor(0.0, 0.0, 0.0, 0.0);
			glClear(GL_COLOR_BUFFER_BIT);
			glPushMatrix();
	
			glTranslatef(-x, -y, 0);
			lvl->draw(x, y, seg_width, seg_height);
			glPopMatrix();

			SDL_GL_SwapBuffers();

			graphics::surface s(SDL_CreateRGBSurface(SDL_SWSURFACE, seg_width, seg_height, 24, SURFACE_MASK_RGB));
			glReadPixels(0, 0, seg_width, seg_height, GL_RGB, GL_UNSIGNED_BYTE, s->pixels);

			unsigned char* pixels = (unsigned char*)s->pixels;

			for(int n = 0; n != seg_height/2; ++n) {
				unsigned char* s1 = pixels + n*seg_width*3;
				unsigned char* s2 = pixels + (seg_height-n-1)*seg_width*3;
				for(int m = 0; m != seg_width*3; ++m) {
					std::swap(s1[m], s2[m]);
				}
			}

			SDL_Rect src_rect = {0, 0, seg_width, seg_height};
			SDL_Rect dst_rect = {x - lvl->boundaries().x(), y - lvl->boundaries().y(), 0, 0};
			SDL_BlitSurface(s.get(), &src_rect, level_surface.get(), &dst_rect);
		}
	}

	IMG_SavePNG(output.c_str(), level_surface.get());
}
