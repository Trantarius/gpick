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

#include "ColorWheel.h"
#include "Color.h"
#include "ColorWheelType.h"
#include <iostream>
enum {
	HUE_CHANGED, SATURATION_VALUE_CHANGED, LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = {0};
struct ColorPoint {
	double hue;
	double lightness;
	double saturation;
};
struct GtkColorWheelPrivate {
	ColorPoint cpoint[10];
	uint32_t n_cpoint;
	ColorPoint *grab_active;
	bool grab_block;
	ColorPoint *selected;
	int active_color;
	float radius;
	float circle_width;
	float block_size;
	bool block_editable;
	const ColorWheelType *color_wheel_type;
	cairo_surface_t *cache_color_wheel;
#if GTK_MAJOR_VERSION >= 3
	GdkDevice *pointer_grab;
#endif
};
#define GET_PRIVATE(obj) reinterpret_cast<GtkColorWheelPrivate *>(gtk_color_wheel_get_instance_private(GTK_COLOR_WHEEL(obj)))
G_DEFINE_TYPE_WITH_CODE(GtkColorWheel, gtk_color_wheel, GTK_TYPE_DRAWING_AREA, G_ADD_PRIVATE(GtkColorWheel));
static GtkWindowClass *parent_class = nullptr;
static gboolean button_release(GtkWidget *color_wheel, GdkEventButton *event);
static gboolean button_press(GtkWidget *color_wheel, GdkEventButton *event);
static gboolean motion_notify(GtkWidget *widget, GdkEventMotion *event);
static uint32_t get_color_index(GtkColorWheelPrivate *ns, ColorPoint *cp);
#if GTK_MAJOR_VERSION >= 3
static gboolean draw(GtkWidget *widget, cairo_t *cr);
#else
static gboolean expose(GtkWidget *color_wheel, GdkEventExpose *event);
#endif
static void finalize(GObject *color_wheel_obj)
{
	GtkColorWheelPrivate *ns = GET_PRIVATE(color_wheel_obj);
	if (ns->cache_color_wheel){
		cairo_surface_destroy(ns->cache_color_wheel);
		ns->cache_color_wheel = 0;
	}
	G_OBJECT_CLASS(parent_class)->finalize(color_wheel_obj);
}
static void gtk_color_wheel_class_init(GtkColorWheelClass *color_wheel_class)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(color_wheel_class);
	obj_class->finalize = finalize;
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(color_wheel_class);
	widget_class->button_release_event = button_release;
	widget_class->button_press_event = button_press;
	widget_class->motion_notify_event = motion_notify;
#if GTK_MAJOR_VERSION >= 3
	widget_class->draw = draw;
#else
	widget_class->expose_event = expose;
#endif
	parent_class = (GtkWindowClass*)g_type_class_peek_parent(G_OBJECT_CLASS(color_wheel_class));
	signals[HUE_CHANGED] = g_signal_new("hue_changed", G_OBJECT_CLASS_TYPE(obj_class), G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET(GtkColorWheelClass, hue_changed), nullptr, nullptr, g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);
	signals[SATURATION_VALUE_CHANGED] = g_signal_new("saturation_value_changed", G_OBJECT_CLASS_TYPE(obj_class), G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET(GtkColorWheelClass, saturation_value_changed), nullptr, nullptr, g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);
}
static void gtk_color_wheel_init(GtkColorWheel *color_wheel)
{
	gtk_widget_add_events(GTK_WIDGET(color_wheel), GDK_2BUTTON_PRESS | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_FOCUS_CHANGE_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);
}
GtkWidget* gtk_color_wheel_new()
{
	GtkWidget* widget = (GtkWidget*) g_object_new(GTK_TYPE_COLOR_WHEEL, nullptr);
	GtkColorWheelPrivate *ns = GET_PRIVATE(widget);
	ns->active_color = 1;
	ns->radius = 80;
	ns->circle_width = 14;
	ns->block_size = static_cast<float>(2 * (ns->radius - ns->circle_width) * std::sin(math::PI / 4) - 8);
#if GTK_MAJOR_VERSION >= 3
	gtk_widget_set_size_request(GTK_WIDGET(widget), static_cast<int>(std::ceil(ns->radius * 2)), static_cast<int>(std::ceil(ns->radius * 2)));
#else
	gtk_widget_set_size_request(GTK_WIDGET(widget), static_cast<int>(std::ceil(ns->radius * 2 + widget->style->xthickness * 2)), static_cast<int>(std::ceil(ns->radius * 2 + widget->style->ythickness * 2)));
#endif
	ns->n_cpoint = 0;
	ns->grab_active = 0;
	ns->grab_block = false;
	ns->selected = &ns->cpoint[0];
	ns->block_editable = true;
	ns->color_wheel_type = &color_wheel_types_get()[0];
	ns->cache_color_wheel = 0;
#if GTK_MAJOR_VERSION >= 3
	ns->pointer_grab = nullptr;
#endif
	gtk_widget_set_can_focus(widget, true);
	return widget;
}
void gtk_color_wheel_set_color_wheel_type(GtkColorWheel *color_wheel, const ColorWheelType *color_wheel_type)
{
	GtkColorWheelPrivate *ns = GET_PRIVATE(color_wheel);
	if (ns->color_wheel_type != color_wheel_type){
		ns->color_wheel_type = color_wheel_type;
		if (ns->cache_color_wheel){
			cairo_surface_destroy(ns->cache_color_wheel);
			ns->cache_color_wheel = 0;
		}
		gtk_widget_queue_draw(GTK_WIDGET(color_wheel));
	}
}
void gtk_color_wheel_set_block_editable(GtkColorWheel* color_wheel, bool editable)
{
	GtkColorWheelPrivate *ns = GET_PRIVATE(color_wheel);
	ns->block_editable = editable;
}
bool gtk_color_wheel_get_block_editable(GtkColorWheel* color_wheel)
{
	GtkColorWheelPrivate *ns = GET_PRIVATE(color_wheel);
	return ns->block_editable;
}
void gtk_color_wheel_set_selected(GtkColorWheel* color_wheel, guint32 index)
{
	GtkColorWheelPrivate *ns = GET_PRIVATE(color_wheel);
	if (index < 10){
		ns->selected = &ns->cpoint[index];
		gtk_widget_queue_draw(GTK_WIDGET(color_wheel));
	}
}
void gtk_color_wheel_set_n_colors(GtkColorWheel* color_wheel, guint32 number_of_colors)
{
	GtkColorWheelPrivate *ns = GET_PRIVATE(color_wheel);
	if (number_of_colors <= 10){
		if (ns->n_cpoint != number_of_colors){
			ns->n_cpoint = number_of_colors;
			if (ns->selected){
				uint32_t index = get_color_index(ns, ns->selected);
				if (index >= number_of_colors){
					ns->selected = &ns->cpoint[0];
				}
			}
			gtk_widget_queue_draw(GTK_WIDGET(color_wheel));
		}
	}
}
void gtk_color_wheel_set_hue(GtkColorWheel* color_wheel, guint32 index, double hue)
{
	GtkColorWheelPrivate *ns = GET_PRIVATE(color_wheel);
	if (index < 10){
		ns->cpoint[index].hue = hue;
		gtk_widget_queue_draw(GTK_WIDGET(color_wheel));
	}
}
void gtk_color_wheel_set_saturation(GtkColorWheel* color_wheel, guint32 index, double saturation)
{
	GtkColorWheelPrivate *ns = GET_PRIVATE(color_wheel);
	if (index < 10){
		ns->cpoint[index].saturation = saturation;
		if (&ns->cpoint[index] == ns->selected)
			gtk_widget_queue_draw(GTK_WIDGET(color_wheel));
	}
}
void gtk_color_wheel_set_value(GtkColorWheel* color_wheel, guint32 index, double value)
{
	GtkColorWheelPrivate *ns = GET_PRIVATE(color_wheel);
	if (index < 10){
		ns->cpoint[index].lightness = value;
		if (&ns->cpoint[index] == ns->selected)
			gtk_widget_queue_draw(GTK_WIDGET(color_wheel));
	}
}
double gtk_color_wheel_get_hue(GtkColorWheel* color_wheel, guint32 index)
{
	GtkColorWheelPrivate *ns = GET_PRIVATE(color_wheel);
	if (index < 10){
		return ns->cpoint[index].hue;
	}
	return 0;
}
double gtk_color_wheel_get_saturation(GtkColorWheel* color_wheel, guint32 index)
{
	GtkColorWheelPrivate *ns = GET_PRIVATE(color_wheel);
	if (index < 10){
		return ns->cpoint[index].saturation;
	}
	return 0;
}
double gtk_color_wheel_get_value(GtkColorWheel* color_wheel, guint32 index)
{
	GtkColorWheelPrivate *ns = GET_PRIVATE(color_wheel);
	if (index < 10){
		return ns->cpoint[index].lightness;
	}
	return 0;
}
static void draw_dot(cairo_t *cr, double x, double y, double size)
{
	cairo_arc(cr, x, y, size - 1, 0, 2 * math::PI);
	cairo_set_source_rgba(cr, 1, 1, 1, 0.5);
	cairo_set_line_width(cr, 2);
	cairo_stroke(cr);
	cairo_arc(cr, x, y, size, 0, 2 * math::PI);
	cairo_set_source_rgba(cr, 0, 0, 0, 1);
	cairo_set_line_width(cr, 1);
	cairo_stroke(cr);
}
static void draw_sat_val_block(cairo_t *cr, double pos_x, double pos_y, double size, double hue)
{
	cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, static_cast<int>(std::ceil(size)), static_cast<int>(std::ceil(size)));
	unsigned char *data = cairo_image_surface_get_data(surface);
	int stride = cairo_image_surface_get_stride(surface);
	int surface_width = cairo_image_surface_get_width(surface);
	int surface_height = cairo_image_surface_get_height(surface);
	Color c;
	uint8_t *line_data;
	for (int y = 0; y < surface_height; ++y){
		line_data = data + stride * y;
		for (int x = 0; x < surface_width; ++x){
			c.hsv.hue = static_cast<float>(hue);
			c.hsv.saturation = static_cast<float>(x / size);
			c.hsv.value = static_cast<float>(y / size);
			c = c.hsvToRgb();
			line_data[2] = static_cast<uint8_t>(c.rgb.red * 255);
			line_data[1] = static_cast<uint8_t>(c.rgb.green * 255);
			line_data[0] = static_cast<uint8_t>(c.rgb.blue * 255);
			line_data[3] = 0xFF;
			line_data += 4;
		}
	}
	cairo_surface_mark_dirty(surface);
	cairo_save(cr);
	cairo_set_source_surface(cr, surface, pos_x - size / 2, pos_y - size / 2);
	cairo_surface_destroy(surface);
	cairo_rectangle(cr, pos_x - size / 2, pos_y - size / 2, size, size);
	cairo_fill(cr);
	cairo_restore(cr);
}
static void draw_wheel(GtkColorWheelPrivate *ns, cairo_t *cr, double radius, double width, const ColorWheelType *wheel)
{
	cairo_surface_t *surface;
	double inner_radius = radius - width;
	if (ns->cache_color_wheel){
		surface = ns->cache_color_wheel;
	}else{
		surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, static_cast<int>(std::ceil(radius * 2)), static_cast<int>(std::ceil(radius * 2)));
		if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS){
			std::cerr << "ColorWheel image surface allocation failed" << std::endl;
			return;
		}
		unsigned char *data = cairo_image_surface_get_data(surface);
		int stride = cairo_image_surface_get_stride(surface);
		int surface_width = cairo_image_surface_get_width(surface);
		int surface_height = cairo_image_surface_get_height(surface);
		double radius_sq = radius * radius + 2 * radius + 1;
		double inner_radius_sq = inner_radius * inner_radius - 2 * inner_radius + 1;
		Color c;
		uint8_t *line_data;
		for (int y = 0; y < surface_height; ++y){
			line_data = data + stride * y;
			for (int x = 0; x < surface_width; ++x){
				int dx = -(x - surface_width / 2);
				int dy = y - surface_height / 2;
				int dist = dx * dx + dy * dy;
				if ((dist >= inner_radius_sq) && (dist <= radius_sq)){
					double angle = atan2((double)dx, (double)dy) + math::PI;
					wheel->hue_to_hsl(angle / (math::PI * 2), &c);
					c = c.hslToRgb();
					line_data[2] = static_cast<uint8_t>(c.rgb.red * 255);
					line_data[1] = static_cast<uint8_t>(c.rgb.green * 255);
					line_data[0] = static_cast<uint8_t>(c.rgb.blue * 255);
					line_data[3] = 0xFF;
				}
				line_data += 4;
			}
		}
		ns->cache_color_wheel = surface;
	}
	cairo_surface_mark_dirty(surface);
	cairo_save(cr);
	cairo_set_source_surface(cr, surface, 0, 0);
	cairo_set_line_width(cr, width);
	cairo_new_path(cr);
	cairo_arc(cr, radius, radius, (inner_radius + radius) / 2, 0, math::PI * 2);
	cairo_stroke(cr);
	cairo_restore(cr);
}
static bool is_inside_block(GtkColorWheelPrivate *ns, gint x, gint y)
{
	double size = ns->block_size;
	if ((x >= ns->radius - size / 2) && (x <= ns->radius + size / 2)){
		if ((y >= ns->radius - size / 2) && (y <= ns->radius + size / 2)){
		return true;
		}
	}
	return false;
}
static ColorPoint* get_cpoint_at(GtkColorWheelPrivate *ns, gint x, gint y)
{
	double dx, dy;
	for (uint32_t i = 0; i != ns->n_cpoint; i++){
		dx = ns->radius + (ns->radius - ns->circle_width / 2) * sin(ns->cpoint[i].hue * math::PI * 2) - x;
		dy = ns->radius - (ns->radius - ns->circle_width / 2) * cos(ns->cpoint[i].hue * math::PI * 2) - y;
		if (sqrt(dx * dx + dy * dy) < 16){
			return &ns->cpoint[i];
		}
	}
	return 0;
}
static uint32_t get_color_index(GtkColorWheelPrivate *ns, ColorPoint *cp)
{
	return (uint64_t(cp) - uint64_t(ns)) / sizeof(ColorPoint);
}
int gtk_color_wheel_get_at(GtkColorWheel *color_wheel, int x, int y)
{
	GtkColorWheelPrivate *ns = GET_PRIVATE(color_wheel);
	ColorPoint *cp = get_cpoint_at(ns, x, y);
	if (cp){
		return get_color_index(ns, cp);
	}
	if (is_inside_block(ns, x, y)) return -1;
	return -2;
}
static void offset_xy(GtkWidget *widget, gint &x, gint &y)
{
#if GTK_MAJOR_VERSION < 3
	x = x - widget->style->xthickness;
	y = y - widget->style->ythickness;
#endif
}
static gboolean motion_notify(GtkWidget *widget, GdkEventMotion *event)
{
	GtkColorWheelPrivate *ns = GET_PRIVATE(widget);
	int x = static_cast<int>(event->x), y = static_cast<int>(event->y);
	offset_xy(widget, x, y);
	if (ns->grab_active){
		double dx = -(x - ns->radius);
		double dy = y - ns->radius;
		double angle = atan2(dx, dy) + math::PI;
		ns->grab_active->hue = angle / (math::PI * 2);
		g_signal_emit(widget, signals[HUE_CHANGED], 0, get_color_index(ns, ns->grab_active));
		gtk_widget_queue_draw(widget);
		return true;
	}else if (ns->grab_block){
		double dx = event->x - ns->radius + ns->block_size / 2;
		double dy = event->y - ns->radius + ns->block_size / 2;
		ns->selected->saturation = math::clamp(static_cast<float>(dx / ns->block_size), 0.0f, 1.0f);
		ns->selected->lightness = math::clamp(static_cast<float>(dy / ns->block_size), 0.0f, 1.0f);
		g_signal_emit(widget, signals[SATURATION_VALUE_CHANGED], 0, get_color_index(ns, ns->selected));
		gtk_widget_queue_draw(widget);
		return true;
	}
	return false;
}
static gboolean draw(GtkWidget *widget, cairo_t *cr)
{
	GtkColorWheelPrivate *ns = GET_PRIVATE(widget);
	draw_wheel(ns, cr, ns->radius, ns->circle_width, ns->color_wheel_type);
	if (ns->selected){
		double block_size = 2 * (ns->radius - ns->circle_width) * sin(math::PI / 4) - 6;
		Color hsl;
		ns->color_wheel_type->hue_to_hsl(ns->selected->hue, &hsl);
		draw_sat_val_block(cr, ns->radius, ns->radius, block_size, hsl.hsl.hue);
		draw_dot(cr, ns->radius - block_size / 2 + block_size * ns->selected->saturation, ns->radius - block_size / 2 + block_size * ns->selected->lightness, 4);
	}
	for (uint32_t i = 0; i != ns->n_cpoint; i++){
		draw_dot(cr, ns->radius + (ns->radius - ns->circle_width / 2) * sin(ns->cpoint[i].hue * math::PI * 2), ns->radius - (ns->radius - ns->circle_width / 2) * cos(ns->cpoint[i].hue * math::PI * 2), (&ns->cpoint[i] == ns->selected) ? 7 : 4);
	}
	return FALSE;
}
#if GTK_MAJOR_VERSION < 3
static gboolean expose(GtkWidget *widget, GdkEventExpose *event)
{
	cairo_t *cr;
	GtkColorWheelPrivate *ns = GET_PRIVATE(widget);
	cr = gdk_cairo_create(gtk_widget_get_window(widget));
	cairo_rectangle(cr, event->area.x, event->area.y, event->area.width, event->area.height);
	cairo_clip(cr);
	cairo_translate(cr, widget->style->xthickness + 0.5, widget->style->ythickness + 0.5);
	gboolean result = draw(widget, cr);
	cairo_destroy(cr);
	return result;
}
#endif
static gboolean button_press(GtkWidget *widget, GdkEventButton *event)
{
	GtkColorWheelPrivate *ns = GET_PRIVATE(widget);
	int x = static_cast<int>(event->x), y = static_cast<int>(event->y);
	offset_xy(widget, x, y);
	gtk_widget_grab_focus(widget);
	ColorPoint *p;
	if (is_inside_block(ns, x, y)){
		if ((event->type == GDK_BUTTON_PRESS) && (event->button == 1)){
			if (ns->block_editable && ns->selected){
				ns->grab_block = true;
				GdkCursor *cursor = gdk_cursor_new_for_display(gtk_widget_get_display(widget), GDK_CROSS);
#if GTK_MAJOR_VERSION >= 3
				ns->pointer_grab = event->device;
				gdk_seat_grab(gdk_device_get_seat(event->device), gtk_widget_get_window(widget), GDK_SEAT_CAPABILITY_ALL, false, cursor, nullptr, nullptr, nullptr);
				g_object_unref(cursor);
#else
				gdk_pointer_grab(gtk_widget_get_window(widget), false, GdkEventMask(GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK), nullptr, cursor, GDK_CURRENT_TIME);
				gdk_cursor_unref(cursor);
#endif
				return true;
			}
		}
	}else if ((p = get_cpoint_at(ns, x, y))){
		if ((event->type == GDK_BUTTON_PRESS) && (event->button == 1)){
			ns->grab_active = p;
			ns->selected = p;
			GdkCursor *cursor = gdk_cursor_new_for_display(gtk_widget_get_display(widget), GDK_CROSS);
#if GTK_MAJOR_VERSION >= 3
			ns->pointer_grab = event->device;
			gdk_seat_grab(gdk_device_get_seat(event->device), gtk_widget_get_window(widget), GDK_SEAT_CAPABILITY_ALL, false, cursor, nullptr, nullptr, nullptr);
			g_object_unref(cursor);
#else
			gdk_pointer_grab(gtk_widget_get_window(widget), false, GdkEventMask(GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK), nullptr, cursor, GDK_CURRENT_TIME);
			gdk_cursor_unref(cursor);
#endif
			return true;
		}
	}
	return false;
}
static gboolean button_release(GtkWidget *widget, GdkEventButton *event)
{
	GtkColorWheelPrivate *ns = GET_PRIVATE(widget);
#if GTK_MAJOR_VERSION >= 3
	if (ns->pointer_grab){
		gdk_seat_ungrab(gdk_device_get_seat(ns->pointer_grab));
		ns->pointer_grab = nullptr;
	}
#else
	gdk_pointer_ungrab(GDK_CURRENT_TIME);
#endif
	ns->grab_active = 0;
	ns->grab_block = false;
	return false;
}
