/**
 * @file    widgets/util/std_grid.c
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

#include <default_grid.h>


/**
 * @brief create a new grid with default margins and spacings
 */

GtkWidget *new_default_grid(void)
{
	GtkWidget *w;


	w = gtk_grid_new();

	g_object_set(w, "margin", 18, NULL);
	gtk_grid_set_column_spacing(GTK_GRID(w), 12);
	gtk_grid_set_row_spacing(GTK_GRID(w), 6);

	return w;
}
