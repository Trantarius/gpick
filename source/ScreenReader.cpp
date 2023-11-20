/*
 * Copyright (c) 2009-2021, Albertas Vyšniauskas
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *     * Neither the name of the software author nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ScreenReader.h"
#include <gtk/gtk.h>
#include <algorithm>
#include <iostream>
#include <libportal/portal.h>
#include <pipewire/pipewire.h>
struct ScreenReader {
	cairo_surface_t *surface;
	int maxSize;
	GdkScreen *screen;
	math::Rectangle<int> readArea;
	XdpPortal* portal=nullptr;
	XdpSession* session=nullptr;
	pw_thread_loop* pwLoop=nullptr;
	pw_context* pwContext=nullptr;
	pw_core* pwCore=nullptr;
	pw_stream* pwStream=nullptr;
};
struct ScreenReader *screen_reader_new() {
	ScreenReader *screen = new ScreenReader;
	screen->maxSize = 0;
	screen->surface = 0;
	screen->screen = 0;

	pw_init(0,nullptr);
	screen->portal = xdp_portal_new();
	screen->session=nullptr;
	GAsyncReadyCallback callback = [](GObject* source_object, GAsyncResult* res, gpointer callback_data){
		ScreenReader* screen = (ScreenReader*)callback_data;
		GError* error=nullptr;
		screen->session=xdp_portal_create_screencast_session_finish(screen->portal,res,&error);
		if(!screen->session || error){
			std::cerr<<"Failed to create XDP screencast session";
			if(error){
				std::cerr<<": "<<error->message;
				g_error_free(error);
			}
			std::cerr<<std::endl;
		}else{
			GAsyncReadyCallback start_callback = [](GObject* source_object, GAsyncResult* res, gpointer start_callback_data){
				ScreenReader* screen = (ScreenReader*)start_callback_data;
				GError* error=nullptr;
				gboolean success = xdp_session_start_finish(screen->session,res,&error);
				if(!success || error){
					std::cerr<<"Failed to start XDP screencast session";
					if(error){
						std::cerr<<": "<<error->message;
						g_error_free(error);
					}
					std::cerr<<std::endl;
				}else{
					screen->pwLoop = pw_thread_loop_new_full(nullptr,nullptr,new spa_dict());
					screen->pwContext = pw_context_new(pw_thread_loop_get_loop(screen->pwLoop),nullptr,0);
					screen->pwCore = pw_context_connect_fd(screen->pwContext,xdp_session_open_pipewire_remote(screen->session),nullptr,0);
					screen->pwStream = pw_stream_new(screen->pwCore,nullptr,nullptr);
					pw_stream_connect(screen->pwStream,PW_DIRECTION_INPUT,PW_ID_ANY,PW_STREAM_FLAG_AUTOCONNECT,nullptr,0);
					pw_stream_set_active(screen->pwStream,true);
					pw_thread_loop_start(screen->pwLoop);
				}
			};
			xdp_session_start(screen->session,nullptr,nullptr,start_callback,screen);
		}
	};

	xdp_portal_create_screencast_session(screen->portal, XdpOutputType::XDP_OUTPUT_MONITOR,
	XdpScreencastFlags::XDP_SCREENCAST_FLAG_NONE, XdpCursorMode::XDP_CURSOR_MODE_METADATA,
	XdpPersistMode::XDP_PERSIST_MODE_TRANSIENT, nullptr, nullptr, callback, screen);

	return screen;
}
void screen_reader_destroy(ScreenReader *screen) {
	if (screen->surface) cairo_surface_destroy(screen->surface);
	pw_context_destroy(screen->pwContext);
	pw_thread_loop_stop(screen->pwLoop);
	pw_thread_loop_destroy(screen->pwLoop);
	g_object_unref(screen->portal);
	delete screen;
}
void screen_reader_add_rect(ScreenReader *screen, GdkScreen *gdkScreen, math::Rectangle<int> &rect) {
	if (screen->screen && (screen->screen == gdkScreen)) {
		screen->readArea += rect;
	} else {
		screen->readArea += rect;
		screen->screen = gdkScreen;
	}
}
void screen_reader_reset_rect(ScreenReader *screen) {
	screen->readArea = math::Rectangle<int>();
	screen->screen = NULL;
}
void screen_reader_update_surface(ScreenReader *screen, math::Rectangle<int> *updateRect) {
	if (!screen->screen) return;
	int left = screen->readArea.getX();
	int top = screen->readArea.getY();
	int width = screen->readArea.getWidth();
	int height = screen->readArea.getHeight();
	if (width > screen->maxSize || height > screen->maxSize) {
		if (screen->surface) cairo_surface_destroy(screen->surface);
		screen->maxSize = (std::max(width, height) / 150 + 1) * 150;
		screen->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, screen->maxSize, screen->maxSize);
	}
	GdkWindow *rootWindow = gdk_screen_get_root_window(screen->screen);
	cairo_t *rootCairo = gdk_cairo_create(rootWindow);
	cairo_surface_t *rootSurface = cairo_get_target(rootCairo);
	if (cairo_surface_status(rootSurface) != CAIRO_STATUS_SUCCESS) {
		std::cerr << "can not get root window surface" << std::endl;
		return;
	}
	cairo_surface_mark_dirty_rectangle(rootSurface, left, top, width, height);
	cairo_t *cr = cairo_create(screen->surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_surface(cr, rootSurface, -left, -top);
	cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_fill(cr);
	cairo_destroy(cr);
	cairo_destroy(rootCairo);
	*updateRect = screen->readArea;

	if(!screen->pwStream) return;
	pw_thread_loop_lock(screen->pwLoop);
	pw_buffer* buff = pw_stream_dequeue_buffer(screen->pwStream);
	std::cout<<pw_stream_state_as_string(pw_stream_get_state(screen->pwStream,nullptr))<<std::endl;
	std::cout<<buff<<std::endl;
	pw_thread_loop_unlock(screen->pwLoop);

}
cairo_surface_t *screen_reader_get_surface(ScreenReader *screen) {
	return screen->surface;
}
