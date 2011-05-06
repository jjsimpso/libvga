/*
 *  Copyright (C) 2002 Nate Case 
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <stdio.h>
#include <gtk/gtk.h>

#include <gtk/gtk.h>
#include <string.h>

typedef struct _VGAFont	VGAFont;
typedef struct _RenderedChar	RenderedChar;

struct _VGAFont
{
	int width, height;		/* Pixel sizes, usually 8x16 */
	guchar * data;
};

VGAFont	*	vga_font_new		(void);
void		vga_font_destroy	(VGAFont * font);
gboolean	vga_font_set_chars	(VGAFont * font, guchar * data,
						guchar start_c, guchar end_c);
gboolean	vga_font_load		(VGAFont * font, guchar * data,
						int height, int width);
gboolean	vga_font_load_from_file	(VGAFont * font, gchar * fname);
void		vga_font_load_default	(VGAFont * font);
void		vga_font_load_default_8x8(VGAFont * font);
int		vga_font_pixels		(VGAFont * font);
GdkBitmap *	vga_font_get_bitmap	(VGAFont * font, GdkWindow * win);
