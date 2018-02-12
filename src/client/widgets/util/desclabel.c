/**
 * @file    widgets/util/desclabel.c
 * @author  Armin Luntzer (armin.luntzer@univie.ac.at)
 *
 * @copyright GPLv2
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * @brief create labels with descriptions
 *
 */

#include <desclabel.h>

/**
 * @brief create a label with a description in smaller text
 */

GtkWidget *gui_create_desclabel(const gchar *text, const gchar *desc)
{
	GtkWidget *box;
	GtkWidget *label;

	gchar *str;


	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	label = gtk_label_new(text);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);

	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);

	label = gtk_label_new(NULL);
	str = g_strconcat ("<span size='small'>", desc, "</span>", NULL);
	gtk_label_set_markup(GTK_LABEL(label), str);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);

	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);

	return box;
}
