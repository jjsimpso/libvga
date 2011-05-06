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
 *
 *  This doesn't do anything interesting.  I considered not even having this,
 *  but in the end the consistency with the VGA Font stuff gives a valuable
 *  level of elegance.
 *
 */

#ifndef __VGA_PALETTE_H__
#define __VGA_PALETTE_H__

#include <stdio.h>
#include <gtk/gtk.h>
#include <math.h>
#include <string.h>

#define PAL_REGS	256
#define TO_GDK_RGB(vga_rgb)     (guint16) floor((vga_rgb * 1040.23809) + 0.5)

typedef enum
{
	PAL_DEFAULT, PAL_WHITE, PAL_BLACK, PAL_GREYSCALE
} StockPalette;

typedef struct _VGAPalette VGAPalette;

/* 
 * The VGA Palette structure here contains 256 registers.  However, only
 * only 64 of them are commonly used in VGA applications.
 */
struct _VGAPalette
{
	GdkColor color[PAL_REGS];
};


VGAPalette *	vga_palette_new			(void);
VGAPalette *	vga_palette_stock		(StockPalette id);
void		vga_palette_destroy		(VGAPalette * pal);
VGAPalette *	vga_palette_dup			(VGAPalette * pal);
VGAPalette * 	vga_palette_copy_from		(VGAPalette * pal,
							VGAPalette * srcpal);
gboolean	vga_palette_load		(VGAPalette * pal,
							guchar * data,
							int size);
gboolean	vga_palette_load_from_file	(VGAPalette * pal,
							guchar * fname);
void		vga_palette_set_reg		(VGAPalette * pal, guchar reg,
							guchar r, guchar g,
							guchar b);
void		vga_palette_load_default	(VGAPalette * pal);
void		vga_palette_morph_to_step	(VGAPalette * pal,
							VGAPalette * srcpal);

#endif	/* __VGA_PALETTE_H__ */
