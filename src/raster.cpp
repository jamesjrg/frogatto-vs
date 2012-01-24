
/*
 Copyright (C) 2007 by David White <dave@whitevine.net>
 Part of the Silver Tree Project
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License version 2 or later.
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY.
 
 See the COPYING file for more details.
 */
#if defined(TARGET_OS_HARMATTAN) || defined(TARGET_PANDORA) || defined(TARGET_TEGRA)
#include <GLES/gl.h>
#ifdef TARGET_PANDORA
#include <GLES/glues.h>
#endif
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif
#include <SDL.h>

#include "asserts.hpp"
#include "foreach.hpp"
#include "preferences.hpp"
#include "raster.hpp"
#include "raster_distortion.hpp"
#include "rectangle_rotator.hpp"

#include <boost/shared_array.hpp>
#include <iostream>
#include <cmath>

namespace graphics
{

bool set_video_mode(int w, int h)
{
#ifdef TARGET_OS_HARMATTAN
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
	return set_video_mode(w,h,0,SDL_OPENGLES | SDL_FULLSCREEN);
#else
	return set_video_mode(w,h,0,SDL_OPENGL|(preferences::resizable() ? SDL_RESIZABLE : 0)|(preferences::fullscreen() ? SDL_FULLSCREEN : 0)) != NULL;
#endif
}

SDL_Surface* set_video_mode(int w, int h, int bitsperpixel, int flags)
{
	graphics::texture::unbuild_all();
	SDL_Surface* result = SDL_SetVideoMode(w,h,bitsperpixel,flags);
	graphics::texture::rebuild_all();

	glShadeModel(GL_SMOOTH);
	glEnable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	return result;
}
	
	/* unavoidable global variable to store global clip
	 rectangle changes */
	std::vector<boost::shared_array<GLint> > clip_rectangles;
	
	std::vector<GLfloat>& global_vertex_array()
	{
		static std::vector<GLfloat> v;
		return v;
	}
	
	std::vector<GLfloat>& global_texcoords_array()
	{
		static std::vector<GLfloat> v;
		return v;
	}

	std::vector<GLbyte>& global_vertex_color_array()
	{
		static std::vector<GLbyte> v;
		return v;
	}
	
#ifdef SDL_VIDEO_OPENGL_ES
#define glOrtho glOrthof
#endif
	
	void prepare_raster()
	{
		//	int real_w, real_h;
		//	bool rotated;
		
#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
		//	real_w = 320;
		//	real_h = 480;
		//	rotated = true;
#else
		const SDL_Surface* fb = SDL_GetVideoSurface();
		if(fb == NULL) {
			std::cerr << "Framebuffer was null in prepare_raster\n";
			return;
		}
		//	real_w = fb->w;
		//	real_h = fb->h;
		//	rotated = false;
#endif
		
		glViewport(0, 0, preferences::actual_screen_width(), preferences::actual_screen_height());
//		glClearColor(0.0, 0.0, 0.0, 0.0);
//		glClear(GL_COLOR_BUFFER_BIT);
		glShadeModel(GL_FLAT);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		
		if(preferences::screen_rotated()) {
			//		glOrtho(0, 640, 960, 0, -1.0, 1.0);
			glOrtho(0, screen_height(), screen_width(), 0, -1.0, 1.0);
		} else {
			glOrtho(0, screen_width(), screen_height(), 0, -1.0, 1.0);
		}
		
		//glOrtho(0, real_w, real_h, 0, -1.0, 1.0);
		if(preferences::screen_rotated()) {
			// Rotate 90 degrees ccw, then move real_h pixels down
			// This has to be in opposite order since A(); B(); means A(B(x))
			glTranslatef(screen_height(), 0.0f, 0.0f);
			glRotatef(90.0f, 0.0f, 0.0f, 1.0f);
			//glTranslatef(0.0f, 0.5f, 0.0f);
			//glScalef(0.5f, 0.5f, 1.0f);
		}
		
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_LIGHTING);
		glDisable(GL_LIGHT0);
		
		glColor4f(1.0, 1.0, 1.0, 1.0);
	}
	
	/*void prepare_raster()
	 {
	 #if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
	 int w = 320;
	 int h = 480;
	 #else
	 const SDL_Surface* fb = SDL_GetVideoSurface();
	 if(fb == NULL) {
	 std::cerr << "Framebuffer was null in prepare_raster\n";
	 return;
	 }
	 int w = fb->w;
	 int h = fb->h;
	 #endif
	 
	 glViewport(0,0,w,h);
	 glClearColor(0.0,0.0,0.0,0.0);
	 glClear(GL_COLOR_BUFFER_BIT);
	 glShadeModel(GL_FLAT);
	 glMatrixMode(GL_PROJECTION);
	 glLoadIdentity();
	 #if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
	 glScalef(0.25f, 0.25f, 1.0f);
	 glRotatef(-90.0f, 0.0f, 0.0f, 1.0f);
	 #endif
	 #ifdef SDL_VIDEO_OPENGL_ES
	 #define glOrtho glOrthof
	 #endif
	 glOrtho(0,screen_width(),screen_height(),0,-1.0,1.0);
	 glMatrixMode(GL_MODELVIEW);
	 glLoadIdentity();
	 
	 glDisable(GL_DEPTH_TEST);
	 glDisable(GL_LIGHTING);
	 glDisable(GL_LIGHT0);
	 
	 glColor4f(1.0, 1.0, 1.0, 1.0);
	 }*/
	
	namespace {
		struct draw_detection_rect {
			rect area;
			char* buf;
		};
		
		std::vector<draw_detection_rect> draw_detection_rects_;
		rect draw_detection_rect_;
		char* draw_detection_buf_;
		
		std::vector<const raster_distortion*> distortions_;
	}
	
	void blit_texture(const texture& tex, int x, int y, GLfloat rotate)
	{
		if(!tex.valid()) {
			return;
		}

		x &= preferences::xypos_draw_mask;
		y &= preferences::xypos_draw_mask;
		
		int h = tex.height();
		int w = tex.width();
		const int w_odd = w % 2;
		const int h_odd = h % 2;
		h /= 2;
		w /= 2;
		
		glPushMatrix();
		
		glTranslatef(x+w,y+h,0.0);
		glRotatef(rotate,0.0,0.0,1.0);
		
		tex.set_as_current_texture();
		
		GLfloat varray[] = {
			-w, -h,
			-w, h+h_odd,
			w+w_odd, -h,
			w+w_odd, h+h_odd
		};
		GLfloat tcarray[] = {
			texture::get_coord_x(0.0), texture::get_coord_y(0.0),
			texture::get_coord_x(0.0), texture::get_coord_y(1.0),
			texture::get_coord_x(1.0), texture::get_coord_y(0.0),
			texture::get_coord_x(1.0), texture::get_coord_y(1.0)
		};
		glVertexPointer(2, GL_FLOAT, 0, varray);
		glTexCoordPointer(2, GL_FLOAT, 0, tcarray);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		
		glPopMatrix();
	}
	
	namespace {
		
		//function which marks the draw detection buffer with pixels drawn.
		void detect_draw(const texture& tex, int x, int y, int orig_w, int orig_h, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
		{
			if(draw_detection_rects_.empty()) {
				return;
			}
			
			rect draw_rect(x, y, std::abs(orig_w), std::abs(orig_h));
			
			foreach(const draw_detection_rect& detect, draw_detection_rects_) {
				if(rects_intersect(draw_rect, detect.area)) {
					rect r = intersection_rect(draw_rect, detect.area);
					for(int ypos = r.y(); ypos != r.y2(); ++ypos) {
						for(int xpos = r.x(); xpos != r.x2(); ++xpos) {
							const GLfloat u = (GLfloat(draw_rect.x2() - xpos)*x1 + GLfloat(xpos - draw_rect.x())*x2)/GLfloat(draw_rect.w());
							const GLfloat v = (GLfloat(draw_rect.y2() - ypos)*y1 + GLfloat(ypos - draw_rect.y())*y2)/GLfloat(draw_rect.h());
							const int texture_x = u*tex.width();
							const int texture_y = v*tex.height();
							ASSERT_GE(texture_x, 0);
							ASSERT_GE(texture_y, 0);
							ASSERT_LOG(texture_x < tex.width(), texture_x << " < " << tex.width() << " " << r.x() << " " << r.x2() << " " << xpos << " x: " << x1 << " x2: " << x2 << " u: " << u << "\n");
							ASSERT_LT(texture_x, tex.width());
							ASSERT_LT(texture_y, tex.height());
							const bool alpha = tex.is_alpha(texture_x, texture_y);
							if(!alpha) {
								const int buf_x = xpos - detect.area.x();
								const int buf_y = ypos - detect.area.y();
								const int buf_index = buf_y*detect.area.w() + buf_x;
								ASSERT_LOG(buf_index >= 0, xpos << ", " << ypos << " -> " << buf_x << ", " << buf_y << " -> " << buf_index << " in " << detect.area << "\n");
								ASSERT_GE(buf_index, 0);
								ASSERT_LT(buf_index, detect.area.w()*detect.area.h());
								detect.buf[buf_index] = true;
							}
						}
					}
				}
			}
		}
		
		void blit_texture_internal(const texture& tex, int x, int y, int w, int h, GLfloat rotate, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
		{
			if(!tex.valid()) {
				return;
			}
			
			const int w_odd = w % 2;
			const int h_odd = h % 2;
			
			w /= 2;
			h /= 2;
			glPushMatrix();
			tex.set_as_current_texture();
			glTranslatef(x+abs(w),y+abs(h),0.0);
			glRotatef(rotate,0.0,0.0,1.0);
			GLfloat varray[] = {
				-w, -h,
				-w, h+h_odd,
				w+w_odd, -h,
				w+w_odd, h+h_odd
			};
			GLfloat tcarray[] = {
				texture::get_coord_x(x1), texture::get_coord_y(y1),
				texture::get_coord_x(x1), texture::get_coord_y(y2),
				texture::get_coord_x(x2), texture::get_coord_y(y1),
				texture::get_coord_x(x2), texture::get_coord_y(y2)
			};
			glVertexPointer(2, GL_FLOAT, 0, varray);
			glTexCoordPointer(2, GL_FLOAT, 0, tcarray);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
			glPopMatrix();
		}
		
		void blit_texture_with_distortion(const texture& tex, int x, int y, int w, int h, GLfloat rotate, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2, const raster_distortion& distort)
		{
			const rect& area = distort.area();
			if(x < area.x()) {
				const int new_x = area.x();
				const GLfloat new_x1 = (x1*(x + w - new_x) + x2*(new_x - x))/w;
				
				blit_texture(tex, x, y, new_x - x, h, rotate, x1, y1, new_x1, y2);
				
				x1 = new_x1;
				w -= new_x - x;
				x = new_x;
			}
			
			if(y < area.y()) {
				const int new_y = area.y();
				const GLfloat new_y1 = (y1*(y + h - new_y) + y2*(new_y - y))/h;
				
				blit_texture(tex, x, y, w, new_y - y, rotate, x1, y1, x2, new_y1);
				
				y1 = new_y1;
				h -= new_y - y;
				y = new_y;
			}
			
			if(x + w > area.x2()) {
				const int new_w = area.x2() - x;
				const int new_xpos = x + new_w;
				const GLfloat new_x2 = (x1*(x + w - new_xpos) + x2*(new_xpos - x))/w;
				
				blit_texture(tex, new_xpos, y, x + w - new_xpos, h, rotate, new_x2, y1, x2, y2);
				
				x2 = new_x2;
				w = new_w;
			}
			
			if(y + h > area.y2()) {
				const int new_h = area.y2() - y;
				const int new_ypos = y + new_h;
				const GLfloat new_y2 = (y1*(y + h - new_ypos) + y2*(new_ypos - y))/h;
				
				blit_texture(tex, x, new_ypos, w, y + h - new_ypos, rotate, x1, new_y2, x2, y2);
				
				y2 = new_y2;
				h = new_h;
			}
			
			tex.set_as_current_texture();
			
			const int xdiff = distort.granularity_x();
			const int ydiff = distort.granularity_y();
			for(int xpos = 0; xpos < w; xpos += xdiff) {
				const int xbegin = x + xpos;
				const int xend = std::min<int>(x + w, xbegin + xdiff);
				
				const GLfloat u1 = (x1*(x+w - xbegin) + x2*(xbegin - x))/w;
				const GLfloat u2 = (x1*(x+w - xend) + x2*(xend - x))/w;
				for(int ypos = 0; ypos < h; ypos += ydiff) {
					const int ybegin = y + ypos;
					const int yend = std::min<int>(y + h, ybegin + ydiff);
					
					const GLfloat v1 = (y1*(y+h - ybegin) + y2*(ybegin - y))/h;
					const GLfloat v2 = (y1*(y+h - yend) + y2*(yend - y))/h;
					
					GLfloat points[8] = { xbegin, ybegin, xend, ybegin, xbegin, yend, xend, yend };
					GLfloat uv[8] = { u1, v1, u2, v1, u1, v2, u2, v2 };
					
					for(int n = 0; n != 4; ++n) {
						distort.distort_point(&points[n*2], &points[n*2 + 1]);
					}
					
					glVertexPointer(2, GL_FLOAT, 0, points);
					glTexCoordPointer(2, GL_FLOAT, 0, uv);
					glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
				}
			}
		}
		
		int blit_texture_translate_x = 0;
		int blit_texture_translate_y = 0;
		
	}  // namespace
	
	distortion_translation::distortion_translation()
	: x_(0), y_(0)
	{
	}
	
	distortion_translation::~distortion_translation()
	{
		if(x_ || y_) {
			foreach(const raster_distortion* distort, distortions_) {
				rect r = distort->area();
				r = rect(r.x() + x_, r.y() + y_, r.w(), r.h());
				const_cast<raster_distortion*>(distort)->set_area(r);
			}
		}
	}
	
	void distortion_translation::translate(int x, int y)
	{
		x_ += x;
		y_ += y;
		
		foreach(const raster_distortion* distort, distortions_) {
			rect r = distort->area();
			r = rect(r.x() - x, r.y() - y, r.w(), r.h());
			const_cast<raster_distortion*>(distort)->set_area(r);
		}
	}
	
	void blit_texture(const texture& tex, int x, int y, int w, int h, GLfloat rotate, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
	{
		x &= preferences::xypos_draw_mask;
		y &= preferences::xypos_draw_mask;

		if(w < 0) {
			std::swap(x1, x2);
			w *= -1;
		}
		
		if(h < 0) {
			std::swap(y1, y2);
			h *= -1;
		}
		
		detect_draw(tex, x, y, w, h, x1, y1, x2, y2);
		
		for(std::vector<const raster_distortion*>::const_iterator i = distortions_.begin(); i != distortions_.end() && rotate == 0.0; ++i) {
			const raster_distortion& distort = **i;
			if(rects_intersect(rect(x, y, w, h), distort.area())) {
				blit_texture_with_distortion(tex, x, y, w, h, rotate, x1, y1, x2, y2, distort);
				return;
			}
		}
		blit_texture_internal(tex, x, y, w, h, rotate, x1, y1, x2, y2);
	}

namespace {
const texture* blit_current_texture;
std::vector<GLfloat> blit_tcqueue;
std::vector<GLshort> blit_vqueue;
}

void queue_blit_texture(const texture& tex, int x, int y, int w, int h,
                        GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	x &= preferences::xypos_draw_mask;
	y &= preferences::xypos_draw_mask;

	if(&tex != blit_current_texture) {
		flush_blit_texture();
		blit_current_texture = &tex;
	}

	x1 = tex.translate_coord_x(x1);
	y1 = tex.translate_coord_y(y1);
	x2 = tex.translate_coord_x(x2);
	y2 = tex.translate_coord_y(y2);

	if(w < 0) {
		std::swap(x1, x2);
		w *= -1;
	}
		
	if(h < 0) {
		std::swap(y1, y2);
		h *= -1;
	}
	
	blit_tcqueue.push_back(x1);
	blit_tcqueue.push_back(y1);
	blit_tcqueue.push_back(x2);
	blit_tcqueue.push_back(y1);
	blit_tcqueue.push_back(x1);
	blit_tcqueue.push_back(y2);
	blit_tcqueue.push_back(x2);
	blit_tcqueue.push_back(y2);
	
	blit_vqueue.push_back(x);
	blit_vqueue.push_back(y);
	blit_vqueue.push_back(x + w);
	blit_vqueue.push_back(y);
	blit_vqueue.push_back(x);
	blit_vqueue.push_back(y + h);
	blit_vqueue.push_back(x + w);
	blit_vqueue.push_back(y + h);
}

void queue_blit_texture(const texture& tex, int x, int y, int w, int h, GLfloat rotate,
						GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	x &= preferences::xypos_draw_mask;
	y &= preferences::xypos_draw_mask;
	
	if(&tex != blit_current_texture) {
		flush_blit_texture();
		blit_current_texture = &tex;
	}
	
	x1 = tex.translate_coord_x(x1);
	y1 = tex.translate_coord_y(y1);
	x2 = tex.translate_coord_x(x2);
	y2 = tex.translate_coord_y(y2);
	
	if(w < 0) {
		std::swap(x1, x2);
		w *= -1;
	}
	
	if(h < 0) {
		std::swap(y1, y2);
		h *= -1;
	}
	
	blit_tcqueue.push_back(x1);
	blit_tcqueue.push_back(y1);
	blit_tcqueue.push_back(x2);
	blit_tcqueue.push_back(y1);
	blit_tcqueue.push_back(x1);
	blit_tcqueue.push_back(y2);
	blit_tcqueue.push_back(x2);
	blit_tcqueue.push_back(y2);
	
	
	
	blit_vqueue.push_back(x);
	blit_vqueue.push_back(y);
	blit_vqueue.push_back(x + w);
	blit_vqueue.push_back(y);
	blit_vqueue.push_back(x);
	blit_vqueue.push_back(y + h);
	blit_vqueue.push_back(x + w);
	blit_vqueue.push_back(y + h);

	rect r(x,y,w,h);
	GLshort* varray = &blit_vqueue[blit_vqueue.size()-8];
	rotate_rect(x+(w/2), y+(h/2), rotate, varray); 

}
	
void flush_blit_texture()
{
	if(!blit_current_texture) {
		return;
	}

	blit_current_texture->set_as_current_texture();
	glVertexPointer(2, GL_SHORT, 0, &blit_vqueue.front());
	glTexCoordPointer(2, GL_FLOAT, 0, &blit_tcqueue.front());
	glDrawArrays(GL_TRIANGLE_STRIP, 0, blit_tcqueue.size()/2);

	blit_current_texture = NULL;
	blit_tcqueue.clear();
	blit_vqueue.clear();
}

void blit_queue::clear()
{
	texture_ = 0;
	vertex_.clear();
	uv_.clear();
}

void blit_queue::do_blit() const
{
	if(vertex_.empty()) {
		return;
	}

	texture::set_current_texture(texture_);

	glVertexPointer(2, GL_SHORT, 0, &vertex_.front());
	glTexCoordPointer(2, GL_FLOAT, 0, &uv_.front());
	glDrawArrays(GL_TRIANGLE_STRIP, 0, uv_.size()/2);
}

void blit_queue::do_blit_range(short begin, short end) const
{
	if(vertex_.empty()) {
		return;
	}

	texture::set_current_texture(texture_);

	glVertexPointer(2, GL_SHORT, 0, &vertex_[begin]);
	glTexCoordPointer(2, GL_FLOAT, 0, &uv_[begin]);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, (end - begin)/2);
}

bool blit_queue::merge(const blit_queue& q, short begin, short end)
{
	if(vertex_.empty()) {
		texture_ = q.texture_;
		vertex_.insert(vertex_.end(), q.vertex_.begin()+begin, q.vertex_.begin()+end);
		uv_.insert(uv_.end(), q.uv_.begin()+begin, q.uv_.begin()+end);
		return true;
	}

	if(texture_ != q.texture_) {
		return false;
	}

	repeat_last();
	vertex_.push_back(q.vertex_[begin]);
	vertex_.push_back(q.vertex_[begin+1]);
	uv_.push_back(q.uv_[begin]);
	uv_.push_back(q.uv_[begin+1]);

	vertex_.insert(vertex_.end(), q.vertex_.begin()+begin, q.vertex_.begin()+end);
	uv_.insert(uv_.end(), q.uv_.begin()+begin, q.uv_.begin()+end);

	return true;
}
	
	void set_draw_detection_rect(const rect& rect, char* buf)
	{
		draw_detection_rect new_rect = { rect, buf };
		draw_detection_rects_.push_back(new_rect);
	}
	
	void clear_draw_detection_rect()
	{
		draw_detection_rects_.clear();
	}
	
	void add_raster_distortion(const raster_distortion* distortion)
	{
//TODO: distortions currently disabled
//		distortion->next_cycle();
//		distortions_.push_back(distortion);
	}
	
	void remove_raster_distortion(const raster_distortion* distortion)
	{
//		distortions_.erase(std::remove(distortions_.begin(), distortions_.end(), distortion), distortions_.end());
	}
	
	void clear_raster_distortion()
	{
		distortions_.clear();
	}
	
	void draw_rect(const SDL_Rect& r, const SDL_Color& color,
				   unsigned char alpha)
	{
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glColor4ub(color.r,color.g,color.b,alpha);
		GLfloat varray[] = {
			r.x, r.y,
			r.x+r.w, r.y,
			r.x, r.y+r.h,
			r.x+r.w, r.y+r.h
		};
		glVertexPointer(2, GL_FLOAT, 0, varray);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		//glRecti(r.x,r.y,r.x+r.w,r.y+r.h);
		glColor4ub(255, 255, 255, 255);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnable(GL_TEXTURE_2D);
	}
	
	void draw_rect(const rect& r, const graphics::color& color)
	{
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glColor4ub(color.r(),color.g(),color.b(),color.a());
		GLfloat varray[] = {
			r.x(), r.y(),
			r.x()+r.w(), r.y(),
			r.x(), r.y()+r.h(),
			r.x()+r.w(), r.y()+r.h()
		};
		glVertexPointer(2, GL_FLOAT, 0, varray);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		//glRecti(r.x(),r.y(),r.x()+r.w(),r.y()+r.h());
		glColor4ub(255, 255, 255, 255);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnable(GL_TEXTURE_2D);
	}
	
	
	void draw_hollow_rect(const SDL_Rect& r, const SDL_Color& color,
						  unsigned char alpha) {
		
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glColor4ub(color.r, color.g, color.b, alpha);
		GLfloat varray[] = {
			r.x, r.y,
			r.x + r.w, r.y,
			r.x + r.w, r.y + r.h,
			r.x, r.y + r.h
		};
		glVertexPointer(2, GL_FLOAT, 0, varray);
		glDrawArrays(GL_LINE_LOOP, 0, sizeof(varray)/sizeof(GLfloat)/2);
		glColor4ub(255, 255, 255, 255);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnable(GL_TEXTURE_2D);
	}

	void draw_circle(int x, int y, int radius)
	{
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		static std::vector<GLfloat> varray;
		varray.clear();
		varray.push_back(x);
		varray.push_back(y);
		for(double angle = 0; angle < 3.1459*2.0; angle += 0.1) {
			const double xpos = x + radius*cos(angle);
			const double ypos = y + radius*sin(angle);
			varray.push_back(xpos);
			varray.push_back(ypos);
		}

		//repeat the first coordinate to complete the circle.
		varray.push_back(varray[2]);
		varray.push_back(varray[3]);

		glVertexPointer(2, GL_FLOAT, 0, &varray.front());
		glDrawArrays(GL_TRIANGLE_FAN, 0, varray.size()/2);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnable(GL_TEXTURE_2D);
	}

#ifndef SDL_VIDEO_OPENGL_ES
	void coords_to_screen(GLdouble sx, GLdouble sy, GLdouble sz,
						  GLdouble* dx, GLdouble* dy, GLdouble* dz) {
		GLdouble model[16], proj[16];
		GLint view[4];
		
		glGetDoublev(GL_MODELVIEW_MATRIX, model);
		glGetDoublev(GL_PROJECTION_MATRIX, proj);
		glGetIntegerv(GL_VIEWPORT, view);
		
		gluProject(sx, sy, sz, model, proj, view, dx, dy, dz);
	}

	void push_clip(const SDL_Rect& r)
	{
		const bool was_enabled_clip = glIsEnabled(GL_SCISSOR_TEST);
		bool should_enable_clip = true;
		
		GLdouble x, y ,z;
		coords_to_screen(0, graphics::screen_height(), 0, &x, &y, &z);
		
		SDL_Rect s = r;
		s.x += static_cast<Sint16>(x);
		s.y -= static_cast<Sint16>(y);
		
		if(s.x == 0 && s.y == 0 && s.w == screen_width() && s.h == screen_height()) {
			should_enable_clip = false;
		}
		
		
		boost::shared_array<GLint> box(new GLint[4]);
		
		if(was_enabled_clip) {
			glGetIntegerv(GL_SCISSOR_BOX, box.get());
		} else {
			box[0] = 0;
			box[1] = 0;
			box[2] = screen_width();
			box[3] = screen_height();
		}
		clip_rectangles.push_back(box);
		
		if(should_enable_clip) {
			if(!was_enabled_clip) {
				glEnable(GL_SCISSOR_TEST);
			}
			glScissor(s.x, screen_height() - (s.y + s.h), s.w, s.h);
		} else if(was_enabled_clip) {
			glDisable(GL_SCISSOR_TEST);
		}
	}
	
	void pop_clip() {
		const bool was_enabled_clip = glIsEnabled(GL_SCISSOR_TEST);
		bool should_enable_clip = false;
		boost::shared_array<GLint> box;
		
		if(!clip_rectangles.empty()) {
			box = *clip_rectangles.rbegin();
			clip_rectangles.pop_back();
			
			if(box[0] != 0 || box[1] != 0 || box[2] != screen_width() || box[3] != screen_height()) {
				should_enable_clip = true;
			}
		}
		
		if(should_enable_clip) {
			if(!was_enabled_clip) {
				glEnable(GL_SCISSOR_TEST);
			}
			glScissor(box[0], box[1], box[2], box[3]);
		} else if(was_enabled_clip) {
			glDisable(GL_SCISSOR_TEST);
		}
	}
#endif
	
	namespace {
		int zoom_level = 1;
	}
	
	int screen_width()
	{
		return preferences::virtual_screen_width()*zoom_level;
		/*
		 SDL_Surface* surf = SDL_GetVideoSurface();
		 if(surf) {
		 return SDL_GetVideoSurface()->w;
		 } else {
		 return 1024;
		 }*/
	}
	
	int screen_height()
	{
		return preferences::virtual_screen_height()*zoom_level;
		/*
		 SDL_Surface* surf = SDL_GetVideoSurface();
		 if(surf) {
		 return SDL_GetVideoSurface()->h;
		 } else {
		 return 768;
		 }*/
	}
	
	void zoom_in()
	{
		--zoom_level;
		if(zoom_level < 1) {
			zoom_level = 1;
		}
	}
	
	void zoom_out()
	{
		++zoom_level;
		if(zoom_level > 5) {
			zoom_level = 5;
		}
	}
	
	void zoom_default()
	{
		zoom_level = 1;
	}
	
	const SDL_Color& color_black()
	{
		static SDL_Color res = {0,0,0,0};
		return res;
	}
	
	const SDL_Color& color_white()
	{
		static SDL_Color res = {0xFF,0xFF,0xFF,0};
		return res;
	}
	
	const SDL_Color& color_red()
	{
		static SDL_Color res = {0xFF,0,0,0};
		return res;
	}
	
	const SDL_Color& color_green()
	{
		static SDL_Color res = {0,0xFF,0,0};
		return res;
	}
	
	const SDL_Color& color_blue()
	{
		static SDL_Color res = {0,0,0xFF,0};
		return res;
	}
	
	const SDL_Color& color_yellow()
	{
		static SDL_Color res = {0xFF,0xFF,0,0};
		return res;
	}
	
}
