/*
 * Copyright 2010 Rob Kendrick <rjek@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * Compatibility functions for older GTK versions (implementation)
 */

#include <stdint.h>

#include "gtk/compat.h"

#if !GTK_CHECK_VERSION(2,16,0)
#include "gtk/sexy_icon_entry.c"
#endif

void nsgtk_widget_set_can_focus(GtkWidget *widget, gboolean can_focus)
{
  #if GTK_CHECK_VERSION(2,22,0)
	gtk_widget_set_can_focus(widget, can_focus);
  #else
	if (can_focus == TRUE)
		GTK_WIDGET_SET_FLAGS(widget, GTK_CAN_FOCUS);
	else
		GTK_WIDGET_UNSET_FLAGS(widget, GTK_CAN_FOCUS);
  #endif
}

gboolean nsgtk_widget_has_focus(GtkWidget *widget)
{
  #if GTK_CHECK_VERSION(2,20,0)
	return gtk_widget_has_focus(widget);
  #else
	return GTK_WIDGET_HAS_FOCUS(widget);
  #endif
}

gboolean nsgtk_widget_get_visible(GtkWidget *widget)
{
  #if GTK_CHECK_VERSION(2,20,0)
	return gtk_widget_get_visible(widget);
  #else
	return GTK_WIDGET_VISIBLE(widget);
  #endif
}

gboolean nsgtk_widget_get_realized(GtkWidget *widget)
{
  #if GTK_CHECK_VERSION(2,20,0)
	return gtk_widget_get_realized(widget);
  #else
	return GTK_WIDGET_REALIZED(widget);
  #endif
}

gboolean nsgtk_widget_get_mapped(GtkWidget *widget)
{
  #if GTK_CHECK_VERSION(2,20,0)
	return gtk_widget_get_mapped(widget);
  #else
	return GTK_WIDGET_MAPPED(widget);
  #endif
}

gboolean nsgtk_widget_is_drawable(GtkWidget *widget)
{
  #if GTK_CHECK_VERSION(2,18,0)
	return gtk_widget_is_drawable(widget);
  #else
	return GTK_WIDGET_DRAWABLE(widget);
  #endif
}

GtkStateType nsgtk_widget_get_state(GtkWidget *widget)
{
  #if GTK_CHECK_VERSION(2,18,0)
	return gtk_widget_get_state(widget);
  #else
	return GTK_WIDGET_STATE(widget);
  #endif
}

void nsgtk_dialog_set_has_separator(GtkDialog *dialog, gboolean setting)
{
  #if GTK_CHECK_VERSION(2,21,8)
	/* Deprecated */
  #else
	gtk_dialog_set_has_separator(dialog, setting);
  #endif
}

GtkWidget *nsgtk_combo_box_text_new(void)
{
  #if GTK_CHECK_VERSION(2,24,0)
	return gtk_combo_box_text_new(); 
  #else
	return gtk_combo_box_new_text();
  #endif
}

void nsgtk_combo_box_text_append_text(GtkWidget *combo_box, const gchar *text)
{
  #if GTK_CHECK_VERSION(2,24,0)
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), text);
  #else
        gtk_combo_box_append_text(GTK_COMBO_BOX(combo_box), text);
  #endif

}

gchar *nsgtk_combo_box_text_get_active_text(GtkWidget *combo_box)
{
  #if GTK_CHECK_VERSION(2,24,0)
	return gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo_box));
  #else
	return gtk_combo_box_get_active_text(GTK_COMBO_BOX(combo_box));
  #endif
}

GtkWidget *nsgtk_entry_new(void)
{
#if GTK_CHECK_VERSION(2,16,0)
	return gtk_entry_new();
#else
	return GTK_WIDGET(sexy_icon_entry_new());
#endif	
}

void nsgtk_entry_set_icon_from_pixbuf(GtkWidget *entry, GtkEntryIconPosition icon_pos, GdkPixbuf *pixbuf)
{
#if GTK_CHECK_VERSION(2,16,0)
	gtk_entry_set_icon_from_pixbuf(GTK_ENTRY(entry), icon_pos, pixbuf);
#else
	GtkImage *image = GTK_IMAGE(gtk_image_new_from_pixbuf(pixbuf));

	if (image != NULL) {
		sexy_icon_entry_set_icon(SEXY_ICON_ENTRY(entry),
					 (SexyIconEntryPosition)icon_pos,
					 image);

		g_object_unref(image);
	}

#endif
}

void nsgtk_entry_set_icon_from_stock(GtkWidget *entry, GtkEntryIconPosition icon_pos, const gchar *stock_id)
{
#if GTK_CHECK_VERSION(2,16,0)
	gtk_entry_set_icon_from_stock(GTK_ENTRY(entry), icon_pos, stock_id);
#else
	GtkImage *image = GTK_IMAGE(gtk_image_new_from_stock(stock_id, 
					GTK_ICON_SIZE_LARGE_TOOLBAR));

	if (image != NULL) {
		sexy_icon_entry_set_icon(SEXY_ICON_ENTRY(entry),
					 (SexyIconEntryPosition)icon_pos,
					 image);
		g_object_unref(image);
	}

#endif
}

void nsgtk_widget_override_background_color(GtkWidget *widget, GtkStateFlags state, uint16_t a, uint16_t r, uint16_t g, uint16_t b)
{
#if GTK_CHECK_VERSION(3,0,0)
	GdkRGBA colour;
	colour.alpha = (double)a / 0xffff;
	colour.red = (double)r / 0xffff;
	colour.green = (double)g / 0xffff;
	colour.blue = (double)b / 0xffff;
	gtk_widget_override_background_color(widget, state, &colour);
#else
	GdkColor colour;
	colour.pixel = a;
	colour.red = r;
	colour.green = g;
	colour.blue = b;
	gtk_widget_modify_bg(widget, state, &colour );
#endif
}

GtkAdjustment *nsgtk_layout_get_vadjustment(GtkLayout *layout)
{
#if GTK_CHECK_VERSION(3,0,0)
	return gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(layout));
#else
	return gtk_layout_get_vadjustment(layout);
#endif
}

GtkAdjustment *nsgtk_layout_get_hadjustment(GtkLayout *layout)
{
#if GTK_CHECK_VERSION(3,0,0)
	return gtk_scrollable_get_hadjustment(GTK_SCROLLABLE(layout));
#else
	return gtk_layout_get_hadjustment(layout);
#endif
}

static void nsgtk_layout_set_adjustment_step_increment(GtkAdjustment *adj,
						       int value)
{
#if GTK_CHECK_VERSION(2,14,0)
	gtk_adjustment_set_step_increment(adj, value);
#else
	adj->step_increment = value;
#endif
}

void nsgtk_layout_set_hadjustment(GtkLayout *layout, GtkAdjustment *adj) 
{
#if GTK_CHECK_VERSION(3,0,0)
	gtk_scrollable_set_hadjustment(GTK_SCROLLABLE(layout), adj);
#else
	gtk_layout_set_hadjustment(layout, adj);
#endif
	nsgtk_layout_set_adjustment_step_increment(adj, 8);
}

void nsgtk_layout_set_vadjustment(GtkLayout *layout, GtkAdjustment *adj) 
{
#if GTK_CHECK_VERSION(3,0,0)
	gtk_scrollable_set_vadjustment(GTK_SCROLLABLE(layout), adj);
#else
	gtk_layout_set_vadjustment(layout, adj);
#endif
	nsgtk_layout_set_adjustment_step_increment(adj, 8);
}

GtkWidget *nsgtk_hbox_new(gboolean homogeneous, gint spacing)
{
#if GTK_CHECK_VERSION(3,0,0)
	return gtk_box_new(GTK_ORIENTATION_HORIZONTAL, spacing);
#else
	return gtk_hbox_new(homogeneous, spacing);
#endif
}

GtkWidget *nsgtk_vbox_new(gboolean homogeneous, gint spacing)
{
#if GTK_CHECK_VERSION(3,0,0)
	return gtk_box_new(GTK_ORIENTATION_VERTICAL, spacing);
#else
	return gtk_vbox_new(homogeneous, spacing);
#endif
}

GtkStateFlags nsgtk_widget_get_state_flags(GtkWidget *widget)
{
#if GTK_CHECK_VERSION(3,0,0)
	return gtk_widget_get_state_flags(widget);
#else
#if GTK_CHECK_VERSION(2,18,0)
	return gtk_widget_get_state(widget);
#else
	return 0; /* FIXME */
#endif
#endif
}

GtkStyleContext *nsgtk_widget_get_style_context(GtkWidget *widget)
{
#if GTK_CHECK_VERSION(3,0,0)
	return gtk_widget_get_style_context(widget);
#else
	return widget->style;
#endif
}

const PangoFontDescription* nsgtk_style_context_get_font(GtkStyleContext *style, GtkStateFlags state)
{
#if GTK_CHECK_VERSION(3,0,0)
	return gtk_style_context_get_font(style, state);
#else
	return style->font_desc;
#endif
}

gulong nsgtk_connect_draw_event(GtkWidget *widget, GCallback callback, gpointer g)
{
#if GTK_CHECK_VERSION(3,0,0)
	return g_signal_connect(G_OBJECT(widget), "draw", callback, g);
#else
	return g_signal_connect(G_OBJECT(widget), "expose_event", callback, g);
#endif
}

void nsgdk_cursor_unref(GdkCursor *cursor)
{
#if GTK_CHECK_VERSION(3,0,0)
	g_object_unref(cursor);
#else
	gdk_cursor_unref(cursor);
#endif
}

void nsgtk_widget_modify_font(GtkWidget *widget, PangoFontDescription *font_desc)
{
#if GTK_CHECK_VERSION(3,0,0)
/* FIXME */
	return;
#else
	gtk_widget_modify_font(widget, font_desc);
#endif
}

GdkWindow *nsgtk_widget_get_window(GtkWidget *widget)
{
#if GTK_CHECK_VERSION(2,14,0)
return gtk_widget_get_window(widget);
#else
 return widget->window;
#endif
}

GtkWidget *nsgtk_dialog_get_content_area(GtkDialog *dialog)
{
#if GTK_CHECK_VERSION(2,14,0)
  return gtk_dialog_get_content_area(dialog);
#else
  return dialog->vbox;
#endif
}

GtkWidget *nsgtk_dialog_get_action_area(GtkDialog *dialog)
{
#if GTK_CHECK_VERSION(2,14,0)
  return gtk_dialog_get_action_area(dialog);
#else
  return dialog->action_area;
#endif
}

gboolean nsgtk_show_uri(GdkScreen *screen, const gchar *uri, guint32 timestamp, GError **error)
{
#if GTK_CHECK_VERSION(2,14,0)
  return gtk_show_uri(screen, uri, timestamp, error);
#else
  return FALSE; /* FIXME */
#endif
}

GdkWindow *nsgtk_layout_get_bin_window(GtkLayout *layout)
{
#if GTK_CHECK_VERSION(2,14,0)
  return gtk_layout_get_bin_window(layout);
#else
  return layout->bin_window;
#endif
}

gdouble nsgtk_adjustment_get_step_increment(GtkAdjustment *adjustment)
{
#if GTK_CHECK_VERSION(2,14,0)
  return gtk_adjustment_get_step_increment(adjustment);
#else
  return adjustment->step_increment;
#endif
}

gdouble nsgtk_adjustment_get_upper(GtkAdjustment *adjustment)
{
#if GTK_CHECK_VERSION(2,14,0)
  return gtk_adjustment_get_upper(adjustment);
#else
  return adjustment->upper;
#endif
}

gdouble nsgtk_adjustment_get_lower(GtkAdjustment *adjustment)
{
#if GTK_CHECK_VERSION(2,14,0)
  return gtk_adjustment_get_lower(adjustment);
#else
  return adjustment->lower;
#endif
}

gdouble nsgtk_adjustment_get_page_increment(GtkAdjustment *adjustment)
{
#if GTK_CHECK_VERSION(2,14,0)
  return gtk_adjustment_get_page_increment(adjustment);
#else
  return adjustment->page_increment;
#endif
}

void nsgtk_widget_get_allocation(GtkWidget *widget, GtkAllocation *allocation)
{
#if GTK_CHECK_VERSION(2,18,0)
  gtk_widget_get_allocation(widget, allocation);
#else
  allocation->x = widget->allocation.x;
  allocation->y = widget->allocation.y;
  allocation->width = widget->allocation.width;
  allocation->height = widget->allocation.height;
#endif
}
