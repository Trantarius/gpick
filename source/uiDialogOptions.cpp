/*
 * Copyright (c) 2009-2016, Albertas Vyšniauskas
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

#include "uiDialogOptions.h"
#include "uiUtilities.h"
#include "ToolColorNaming.h"
#include "GlobalState.h"
#include "Internationalisation.h"
#include "LuaExt.h"
#include "DynvHelpers.h"
#include <string>
#include <iostream>
using namespace std;
extern "C"{
#include <lualib.h>
#include <lauxlib.h>
}

static const struct{
	const char *label;
	const char *setting;
}available_color_spaces[] = {
	{"CMYK", "picker.color_space.cmyk"},
	{"HSL", "picker.color_space.hsl"},
	{"HSV", "picker.color_space.hsv"},
	{"LAB", "picker.color_space.lab"},
	{"LCH", "picker.color_space.lch"},
	{"RGB", "picker.color_space.rgb"},
	{0, 0},
};

typedef struct DialogOptionsArgs{
	GtkWidget *minimize_to_tray;
	GtkWidget *close_to_tray;
	GtkWidget *start_in_tray;
	GtkWidget *refresh_rate;
	GtkWidget *single_instance;
	GtkWidget *default_drag_action[2];
	GtkWidget *hex_case[2];
	GtkWidget *save_restore_palette;
	GtkWidget *add_on_release;
	GtkWidget *add_to_palette;
	GtkWidget *copy_to_clipboard;
	GtkWidget *rotate_swatch;
	GtkWidget *copy_on_release;
	GtkWidget *zoom_size;
	GtkWidget *imprecision_postfix;
	GtkWidget *tool_color_naming[3];
	GtkWidget *color_spaces[6];
	GtkWidget *out_of_gamut_mask;
	GtkWidget *lab_illuminant;
	GtkWidget *lab_observer;
	struct dynvSystem *params;
	GlobalState* gs;
}DialogOptionsArgs;

int dialog_options_update(lua_State *lua, dynvSystem *settings)
{
	if (lua == nullptr || settings == nullptr) return -1;
	lua_State* L = lua;
	int status;
	int stack_top = lua_gettop(L);
	lua_getglobal(L, "gpick");
	int gpick_namespace = lua_gettop(L);
	if (lua_type(L, -1) != LUA_TNIL){
		lua_pushstring(L, "options_update");
		lua_gettable(L, gpick_namespace);
		if (lua_type(L, -1) != LUA_TNIL){
			lua_pushdynvsystem(L, settings);
			status = lua_pcall(L, 1, 0, 0);
			dynv_system_release(settings);
			if (status == 0){
				lua_settop(L, stack_top);
				return 0;
			}else{
				cerr << "gpick.options_update: " << lua_tostring(L, -1) << endl;
			}
		}else{
			cerr << "gpick.options_update: no such function \"options_update\"" << endl;
		}
	}
	lua_settop(L, stack_top);
	return -1;
}

static void calc( DialogOptionsArgs *args, bool preview, int limit)
{
	if (preview) return;
	dynv_set_bool(args->params, "main.minimize_to_tray", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(args->minimize_to_tray)));
	dynv_set_bool(args->params, "main.close_to_tray", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(args->close_to_tray)));
	dynv_set_bool(args->params, "main.start_in_tray", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(args->start_in_tray)));
	dynv_set_bool(args->params, "main.single_instance", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(args->single_instance)));
	dynv_set_bool(args->params, "main.save_restore_palette", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(args->save_restore_palette)));
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(args->default_drag_action[0])))
		dynv_set_bool(args->params, "main.dragging_moves", true);
	else
		dynv_set_bool(args->params, "main.dragging_moves", false);
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(args->hex_case[0])))
		dynv_set_string(args->params, "options.hex_case", "lower");
	else
		dynv_set_string(args->params, "options.hex_case", "upper");
	dynv_set_float(args->params, "picker.refresh_rate", gtk_spin_button_get_value(GTK_SPIN_BUTTON(args->refresh_rate)));
	dynv_set_int32(args->params, "picker.zoom_size", gtk_spin_button_get_value(GTK_SPIN_BUTTON(args->zoom_size)));
	dynv_set_bool(args->params, "picker.sampler.add_on_release", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(args->add_on_release)));
	dynv_set_bool(args->params, "picker.sampler.copy_on_release", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(args->copy_on_release)));
	dynv_set_bool(args->params, "picker.sampler.add_to_palette", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(args->add_to_palette)));
	dynv_set_bool(args->params, "picker.sampler.copy_to_clipboard", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(args->copy_to_clipboard)));
	dynv_set_bool(args->params, "picker.sampler.rotate_swatch_after_sample", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(args->rotate_swatch)));
	dynv_set_bool(args->params, "picker.out_of_gamut_mask", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(args->out_of_gamut_mask)));
	dynv_set_bool(args->params, "color_names.imprecision_postfix", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(args->imprecision_postfix)));
	const ToolColorNamingOption *color_naming_options = tool_color_naming_get_options();
	int i = 0;
	while (color_naming_options[i].name){
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(args->tool_color_naming[i]))){
			dynv_set_string(args->params, "color_names.tool_color_naming", color_naming_options[i].name);
			break;
		}
		i++;
	}
	for (int i = 0; available_color_spaces[i].label; i++){
		dynv_set_bool(args->params, available_color_spaces[i].setting, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(args->color_spaces[i])));
	}
	dynv_set_string(args->params, "picker.lab.illuminant", gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(args->lab_illuminant)));
	dynv_set_string(args->params, "picker.lab.observer", gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(args->lab_observer)));
}

void dialog_options_show(GtkWindow* parent, GlobalState* gs)
{
	DialogOptionsArgs *args = new DialogOptionsArgs;
	args->gs = gs;
	args->params = dynv_get_dynv(args->gs->getSettings(), "gpick");
	GtkWidget *table, *table_m, *widget;
	GtkWidget *dialog = gtk_dialog_new_with_buttons(_("Options"), parent, GtkDialogFlags(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, nullptr);
	gtk_window_set_default_size(GTK_WINDOW(dialog), dynv_get_int32_wd(args->params, "options.window.width", -1), dynv_get_int32_wd(args->params, "options.window.height", -1));
	gtk_dialog_set_alternative_button_order(GTK_DIALOG(dialog), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL, -1);
	GtkWidget *frame;
	GtkWidget* notebook = gtk_notebook_new();
	gint table_y, table_m_y;
	table_m = gtk_table_new(3, 1, FALSE);
	table_m_y = 0;
	frame = gtk_frame_new(_("System"));
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_table_attach(GTK_TABLE(table_m), frame, 0, 1, table_m_y, table_m_y+1, GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL), 5, 5);
	table_m_y++;
	table = gtk_table_new(5, 3, FALSE);
	table_y=0;
	gtk_container_add(GTK_CONTAINER(frame), table);
	args->single_instance = widget = gtk_check_button_new_with_mnemonic (_("_Single instance"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), dynv_get_bool_wd(args->params, "main.single_instance", false));
	gtk_table_attach(GTK_TABLE(table), widget,0,3,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,3,3);
	table_y++;
	args->save_restore_palette = widget = gtk_check_button_new_with_mnemonic (_("Save/_Restore palette"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), dynv_get_bool_wd(args->params, "main.save_restore_palette", true));
	gtk_table_attach(GTK_TABLE(table), widget,0,3,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,3,3);
	table_y++;
	frame = gtk_frame_new(_("System tray"));
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_table_attach(GTK_TABLE(table_m), frame, 0, 1, table_m_y, table_m_y+1, GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL), 5, 5);
	table_m_y++;
	table = gtk_table_new(5, 3, FALSE);
	table_y=0;
	gtk_container_add(GTK_CONTAINER(frame), table);
	args->minimize_to_tray = widget = gtk_check_button_new_with_mnemonic (_("_Minimize to system tray"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), dynv_get_bool_wd(args->params, "main.minimize_to_tray", false));
	gtk_table_attach(GTK_TABLE(table), widget,0,3,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,3,3);
	table_y++;
	args->close_to_tray = widget = gtk_check_button_new_with_mnemonic (_("_Close to system tray"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), dynv_get_bool_wd(args->params, "main.close_to_tray", false));
	gtk_table_attach(GTK_TABLE(table), widget,0,3,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,3,3);
	table_y++;
	args->start_in_tray = widget = gtk_check_button_new_with_mnemonic (_("_Start in system tray"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), dynv_get_bool_wd(args->params, "main.start_in_tray", false));
	gtk_table_attach(GTK_TABLE(table), widget,0,3,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,3,3);
	table_y++;
	frame = gtk_frame_new(_("Default drag action"));
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_table_attach(GTK_TABLE(table_m), frame, 0, 1, table_m_y, table_m_y+1, GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL), 5, 5);
	table_m_y++;
	table = gtk_table_new(5, 3, FALSE);
	table_y=0;
	gtk_container_add(GTK_CONTAINER(frame), table);
	GSList *group = nullptr;
	bool dragging_moves = dynv_get_bool_wd(args->params, "main.dragging_moves", true);
	args->default_drag_action[0] = widget = gtk_radio_button_new_with_mnemonic(group, _("M_ove"));
	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(widget));
	if (dragging_moves)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), true);
	gtk_table_attach(GTK_TABLE(table), widget,0,3,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,3,3);
	table_y++;
	args->default_drag_action[1] = widget = gtk_radio_button_new_with_mnemonic(group, _("Cop_y"));
	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(widget));
	if (dragging_moves == false)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), true);
	gtk_table_attach(GTK_TABLE(table), widget,0,3,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,3,3);
	table_y++;
	table_m_y = 0;
	frame = gtk_frame_new(_("Hex format"));
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_table_attach(GTK_TABLE(table_m), frame, 1, 2, table_m_y, table_m_y+1, GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL), 5, 5);
	table_m_y++;
	table = gtk_table_new(1, 1, FALSE);
	table_y=0;
	gtk_container_add(GTK_CONTAINER(frame), table);
	group = nullptr;
	string hex_format = dynv_get_string_wd(args->params, "options.hex_case", "upper");
	args->hex_case[0] = widget = gtk_radio_button_new_with_mnemonic(group, _("Lower case"));
	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(widget));
	if (hex_format == "lower")
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), true);
	gtk_table_attach(GTK_TABLE(table), widget,0,1,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,3,3);
	table_y++;
	args->hex_case[1] = widget = gtk_radio_button_new_with_mnemonic(group, _("Upper case"));
	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(widget));
	if (hex_format == "upper")
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), true);
	gtk_table_attach(GTK_TABLE(table), widget,0,1,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,3,3);
	table_y++;
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table_m, gtk_label_new_with_mnemonic(_("_Main")));
	table_m = gtk_table_new(3, 2, FALSE);
	table_m_y = 0;
	frame = gtk_frame_new(_("Display"));
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_table_attach(GTK_TABLE(table_m), frame, 0, 1, table_m_y, table_m_y+1, GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL), 5, 5);
	table_m_y++;
	table = gtk_table_new(5, 3, FALSE);
	table_y=0;
	gtk_container_add(GTK_CONTAINER(frame), table);
	gtk_table_attach(GTK_TABLE(table), gtk_label_mnemonic_aligned_new(_("_Refresh rate:"),0,0.5,0,0),0,1,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,3,3);
	args->refresh_rate = widget = gtk_spin_button_new_with_range(1, 60, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(args->refresh_rate), dynv_get_float_wd(args->params, "picker.refresh_rate", 30));
	gtk_table_attach(GTK_TABLE(table), widget,1,2,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,5,5);
	gtk_table_attach(GTK_TABLE(table), gtk_label_aligned_new("Hz",0,0.5,0,0),2,3,table_y,table_y+1,GTK_FILL,GTK_FILL,5,5);
	table_y++;
	gtk_table_attach(GTK_TABLE(table), gtk_label_mnemonic_aligned_new(_("_Magnified area size:"),0,0.5,0,0),0,1,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,3,3);
	args->zoom_size = widget = gtk_spin_button_new_with_range(75, 300, 15);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(args->zoom_size), dynv_get_int32_wd(args->params, "picker.zoom_size", 150));
	gtk_table_attach(GTK_TABLE(table), widget,1,3,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,5,5);
	table_y++;
	frame = gtk_frame_new(_("Floating picker click behaviour"));
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_table_attach(GTK_TABLE(table_m), frame, 0, 1, table_m_y, table_m_y+1, GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL), 5, 5);
	table_m_y++;
	table = gtk_table_new(5, 3, FALSE);
	table_y=0;
	gtk_container_add(GTK_CONTAINER(frame), table);
	args->add_on_release = widget = gtk_check_button_new_with_mnemonic(_("_Add to palette"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), dynv_get_bool_wd(args->params, "picker.sampler.add_on_release", false));
	gtk_table_attach(GTK_TABLE(table), widget,0,3,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,3,3);
	table_y++;
	args->copy_on_release = widget = gtk_check_button_new_with_mnemonic(_("_Copy to clipboard"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), dynv_get_bool_wd(args->params, "picker.sampler.copy_on_release", false));
	gtk_table_attach(GTK_TABLE(table), widget,0,3,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,3,3);
	table_y++;
	frame = gtk_frame_new(_("'Spacebar' button behaviour"));
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_table_attach(GTK_TABLE(table_m), frame, 0, 1, table_m_y, table_m_y+1, GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL), 5, 5);
	table_m_y++;
	table = gtk_table_new(5, 3, FALSE);
	table_y=0;
	gtk_container_add(GTK_CONTAINER(frame), table);
	args->add_to_palette = widget = gtk_check_button_new_with_mnemonic(_("_Add to palette"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), dynv_get_bool_wd(args->params, "picker.sampler.add_to_palette", false));
	gtk_table_attach(GTK_TABLE(table), widget,1,2,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,3,3);
	table_y++;
	args->copy_to_clipboard = widget = gtk_check_button_new_with_mnemonic(_("_Copy to clipboard"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), dynv_get_bool_wd(args->params, "picker.sampler.copy_to_clipboard", false));
	gtk_table_attach(GTK_TABLE(table), widget,1,2,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,3,3);
	table_y++;
	args->rotate_swatch = widget = gtk_check_button_new_with_mnemonic(_("_Rotate swatch"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), dynv_get_bool_wd(args->params, "picker.sampler.rotate_swatch_after_sample", false));
	gtk_table_attach(GTK_TABLE(table), widget,1,2,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,3,3);
	table_y++;
	table_m_y = 0;
	frame = gtk_frame_new(_("Enabled color spaces"));
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_table_attach(GTK_TABLE(table_m), frame, 1, 2, table_m_y, table_m_y+1, GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL), 5, 5);
	table_m_y++;
	table = gtk_table_new(5, 3, FALSE);
	table_y=0;
	gtk_container_add(GTK_CONTAINER(frame), table);
	for (int i = 0; available_color_spaces[i].label; i++){
		args->color_spaces[i] = widget = gtk_check_button_new_with_label(available_color_spaces[i].label);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), dynv_get_bool_wd(args->params, available_color_spaces[i].setting, true));
		gtk_table_attach(GTK_TABLE(table), widget, 1, 2, table_y, table_y+1, GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_FILL, 3, 3);
		table_y++;
	}
	frame = gtk_frame_new(_("Lab settings"));
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_table_attach(GTK_TABLE(table_m), frame, 1, 2, table_m_y, table_m_y+1, GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL), 5, 5);
	table_m_y++;
	table = gtk_table_new(5, 3, FALSE);
	table_y=0;
	gtk_container_add(GTK_CONTAINER(frame), table);
	{
		int selected;
		const char *option;
		gtk_table_attach(GTK_TABLE(table), gtk_label_mnemonic_aligned_new(_("_Illuminant:"),0,0.5,0,0),0,1,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,3,3);
		args->lab_illuminant = widget = gtk_combo_box_text_new();
		const char *illuminants[] = {
			"A",
			"C",
			"D50",
			"D55",
			"D65",
			"D75",
			"F2",
			"F7",
			"F11",
			0,
		};
		selected = 0;
		option = dynv_get_string_wd(args->params, "picker.lab.illuminant", "D50");
		for (int i = 0; illuminants[i]; i++){
			if (string(illuminants[i]).compare(option) == 0) selected = i;
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), illuminants[i]);
		}
		gtk_combo_box_set_active(GTK_COMBO_BOX(widget), selected);
		gtk_table_attach(GTK_TABLE(table), widget,1,3,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,5,5);
		table_y++;
		gtk_table_attach(GTK_TABLE(table), gtk_label_mnemonic_aligned_new(_("_Observer:"),0,0.5,0,0),0,1,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,3,3);
		args->lab_observer = widget = gtk_combo_box_text_new();
		const char *observers[] = {
			"2",
			"10",
			0,
		};
		selected = 0;
		option = dynv_get_string_wd(args->params, "picker.lab.observer", "2");
		for (int i = 0; observers[i]; i++){
			if (string(observers[i]).compare(option) == 0) selected = i;
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), observers[i]);
		}
		gtk_combo_box_set_active(GTK_COMBO_BOX(widget), selected);
		gtk_table_attach(GTK_TABLE(table), widget,1,3,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,5,5);
		table_y++;
	}
	frame = gtk_frame_new(_("Other settings"));
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_table_attach(GTK_TABLE(table_m), frame, 1, 2, table_m_y, table_m_y+1, GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL), 5, 5);
	table_m_y++;
	table = gtk_table_new(5, 3, FALSE);
	table_y=0;
	gtk_container_add(GTK_CONTAINER(frame), table);
	args->out_of_gamut_mask = widget = gtk_check_button_new_with_mnemonic(_("_Mask out of gamut colors"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), dynv_get_bool_wd(args->params, "picker.out_of_gamut_mask", true));
	gtk_table_attach(GTK_TABLE(table), widget, 1, 2, table_y, table_y+1, GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_FILL, 3, 3);
	table_y++;
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table_m, gtk_label_new_with_mnemonic(_("_Picker")));
	table_m = gtk_table_new(3, 1, FALSE);
	table_m_y = 0;
	frame = gtk_frame_new(_("Color name generation"));
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_table_attach(GTK_TABLE(table_m), frame, 0, 1, table_m_y, table_m_y+1, GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL), 5, 5);
	table_m_y++;
	table = gtk_table_new(5, 3, FALSE);
	table_y=0;
	gtk_container_add(GTK_CONTAINER(frame), table);
	args->imprecision_postfix = widget = gtk_check_button_new_with_mnemonic(_("_Imprecision postfix"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), dynv_get_bool_wd(args->params, "color_names.imprecision_postfix", true));
	gtk_table_attach(GTK_TABLE(table), widget,1,2,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,3,3);
	table_y++;
	frame = gtk_frame_new(_("Tool color naming"));
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_table_attach(GTK_TABLE(table_m), frame, 0, 1, table_m_y, table_m_y+1, GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL), 5, 5);
	table_m_y++;
	table = gtk_table_new(5, 3, FALSE);
	table_y=0;
	gtk_container_add(GTK_CONTAINER(frame), table);
	group = nullptr;
	ToolColorNamingType color_naming_type = tool_color_naming_name_to_type(dynv_get_string_wd(args->params, "color_names.tool_color_naming", "tool_specific"));
	const ToolColorNamingOption *color_naming_options = tool_color_naming_get_options();
	int i = 0;
	while (color_naming_options[i].name){
		args->tool_color_naming[i] = widget = gtk_radio_button_new_with_mnemonic(group, _(color_naming_options[i].label));
		group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(widget));
		if (color_naming_type == color_naming_options[i].type)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), true);
		gtk_table_attach(GTK_TABLE(table), widget,1,2,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,3,3);
		table_y++;
		i++;
	}
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table_m, gtk_label_new_with_mnemonic(_("_Color names")));
	gtk_widget_show_all(notebook);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), notebook);
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
		calc(args, false, 0);
		dialog_options_update(args->gs->getLua(), args->gs->getSettings());
	}
	gint width, height;
	gtk_window_get_size(GTK_WINDOW(dialog), &width, &height);
	dynv_set_int32(args->params, "options.window.width", width);
	dynv_set_int32(args->params, "options.window.height", height);
	dynv_system_release(args->params);
	gtk_widget_destroy(dialog);
	delete args;
}
