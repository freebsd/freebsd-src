/*
 * Copyright 2008 Mike Lester <element3260@gmail.com>
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

#include <stdint.h>

#include "utils/utils.h"

#include "gtk/compat.h"
#include "gtk/gui.h"
#include "gtk/dialogs/about.h"


static void
nsgtk_about_dialog_credits(GtkWidget *button, gpointer data)
{
	struct browser_window *bw = data;
	nsurl *url;

	if (nsurl_create("about:credits", &url) != NSERROR_OK) {
		warn_user("NoMemory", 0);
	} else {
		browser_window_navigate(bw,
					url,
					NULL,
					BW_NAVIGATE_HISTORY,
					NULL,
					NULL,
					NULL);
		nsurl_unref(url);
	}

	gtk_widget_destroy(gtk_widget_get_toplevel(button));
}

static void
nsgtk_about_dialog_licence(GtkWidget *button, gpointer data)
{
	struct browser_window *bw = data;
	nsurl *url;

	if (nsurl_create("about:licence", &url) != NSERROR_OK) {
		warn_user("NoMemory", 0);
	} else {
		browser_window_navigate(bw,
					url,
					NULL,
					BW_NAVIGATE_HISTORY,
					NULL,
					NULL,
					NULL);
		nsurl_unref(url);
	}

	gtk_widget_destroy(gtk_widget_get_toplevel(button));
}

void nsgtk_about_dialog_init(GtkWindow *parent,
			     struct browser_window *bw,
			     const char *version)
{
	GtkWidget *dialog, *vbox, *button, *image, *label;
	gchar *name_string;
	GList *pixbufs = gtk_window_get_default_icon_list();

	name_string = g_markup_printf_escaped ("<span size=\"xx-large\" weight=\"bold\">NetSurf %s</span>", version);


	/* Create the widgets */
	dialog = gtk_dialog_new_with_buttons("About NetSurf",
					     parent,
					     GTK_DIALOG_DESTROY_WITH_PARENT,
					     NULL);

	vbox = nsgtk_vbox_new(FALSE, 8);

	gtk_box_pack_start(GTK_BOX(nsgtk_dialog_get_content_area(GTK_DIALOG(dialog))), vbox, TRUE, TRUE, 0);
	
	if (pixbufs != NULL) {
		GtkIconSet *icon_set = gtk_icon_set_new_from_pixbuf(GDK_PIXBUF(g_list_nth_data(pixbufs, 0)));

		image = gtk_image_new();

		gtk_image_set_from_icon_set (GTK_IMAGE (image),
					     icon_set, GTK_ICON_SIZE_DIALOG);

		gtk_icon_set_unref (icon_set);
		g_list_free (pixbufs);

		gtk_box_pack_start(GTK_BOX (vbox), image, FALSE, FALSE, 0);
	}
	

	label = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (label), name_string);
	g_free (name_string);
	gtk_label_set_selectable (GTK_LABEL (label), TRUE);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

        label = gtk_label_new("NetSurf is a small fast web browser");
	gtk_label_set_selectable(GTK_LABEL (label), TRUE);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	label = gtk_label_new("Copyright © 2003 - 2011 The NetSurf Developers");
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
	gtk_box_pack_start(GTK_BOX (vbox), label, FALSE, FALSE, 0);


	nsgtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

	/* Add the OK button */
	gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);

	/* Add the credits button */
	button = gtk_button_new_from_stock ("Credits");
	gtk_box_pack_end(GTK_BOX(nsgtk_dialog_get_action_area(GTK_DIALOG(dialog))),
			 button, FALSE, TRUE, 0);
	gtk_button_box_set_child_secondary (GTK_BUTTON_BOX(nsgtk_dialog_get_action_area(GTK_DIALOG(dialog))), button, TRUE);
	g_signal_connect(button, "clicked", G_CALLBACK(nsgtk_about_dialog_credits), (gpointer)bw);

	/* Add the Licence button */
	button = gtk_button_new_from_stock ("Licence");
	gtk_box_pack_end(GTK_BOX (GTK_DIALOG(nsgtk_dialog_get_action_area(GTK_DIALOG(dialog)))),
			 button, FALSE, TRUE, 0);
	gtk_button_box_set_child_secondary (GTK_BUTTON_BOX(nsgtk_dialog_get_action_area(GTK_DIALOG(dialog))), button, TRUE);
	g_signal_connect(button, "clicked", G_CALLBACK(nsgtk_about_dialog_licence), (gpointer)bw);


	/* Ensure that the dialog box is destroyed when the user responds. */
	g_signal_connect_swapped(dialog,
				  "response",
				  G_CALLBACK (gtk_widget_destroy),
				  dialog);

	/* Add the label, and show everything we've added to the dialog. */
	gtk_widget_show_all(dialog);
}
