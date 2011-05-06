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

#include "vgafont.h"
#include "def_font.h"

/**
 * vga_font_new:
 * 
 * Create a new VGA Font object.
 *
 * Returns: a new VGA font object.
 */
VGAFont * vga_font_new(void)
{
	VGAFont * font;
	
	font = g_new(VGAFont, 1);
	font->width = -1;
	font->height = -1;
	font->data = NULL;

	return font;
}


/**
 * vga_font_destroy:
 * @font: the VGA Font object
 *
 * Destroy a VGA Font object.
 */
void vga_font_destroy(VGAFont * font)
{
	g_return_if_fail(font != NULL);
	if (font->data)
		g_free(font->data);
	g_free(font);
}


/**
 * vga_font_set_chars:
 * @font: the VGA font object
 * @data: raw VGA font data for [partial] font
 * @start_c: first character @data describes
 * @end_c: last character @data describes
 *
 * Load the [partial] font into the font object.  A font must already be
 * loaded before calling this function (unless you know what you are doing).
 * The @data buffer must be large enough to account the character range
 * given.
 */
gboolean vga_font_set_chars(VGAFont * font, guchar * data, guchar start_c,
		       	guchar end_c)
{
	int bytes;
	g_return_val_if_fail(data != NULL, FALSE);
	g_return_val_if_fail(font->data != NULL, FALSE);
	g_assert(font->height > 0 && font->width > 0);
	bytes = ((font->width * font->height) / 8) * (end_c - start_c + 1);
	memcpy(font->data + start_c, data, bytes);

	return TRUE;
}

/**
 * vga_font_load:
 * @font: the VGA font object
 * @data: raw VGA font data for entire font
 * @width: width, in pixels, of the font
 * @height: height, in pixels, of the font
 *
 * Load the given font data/parameters into the font object.
 *
 * Returns: TRUE on success.
 */
gboolean vga_font_load(VGAFont * font, guchar * data, int width, int height)
{
	font->height = height;
	font->width = width;
	if (font->data)
	{
		g_free(font->data);
	}
	font->data = g_malloc(width * height * 32);
	return vga_font_set_chars(font, data, 0, 255);
}

/* Determine the dimensions of the characters given the font image length */
/* I haven't proved it mathematically but it should only be able to get one */
/* correct result with this algorithm. */
static void
determine_char_size(int fontlen, int *width, int *height)
{
	int bits_per_char = (fontlen / 256) * 8;
	int h, w;
	for (h = 1; h < 65; h++)
		for (w = 1; w < 65; w++)
		{
			if ((w * h) == bits_per_char && (2 * w == h))
			{
				*height = h;
				*width = w;
				return;
			}
		}
}

/**
 * vga_font_load_from_file:
 * @font: the VGA font object
 * @fname: the filename of the VGA font file to load.
 *
 * Load a VGA font from a raw VGA font data file.
 */
gboolean vga_font_load_from_file(VGAFont * font, gchar * fname)
{
	int filesize, height = 0, width = 0;
	FILE * f;
	guchar * buf;
	gboolean result;

	/* Determine font parameters and read file into memory buffer */
	f = fopen(fname, "rb");
	fseek(f, 0, SEEK_END);
	filesize = ftell(f);
	fseek(f, 0, SEEK_SET);
	determine_char_size(filesize, &width, &height);
	if (height == 0 || width == 0)
	{
		fclose(f);
		return FALSE;
	}
	buf = g_malloc(filesize);
	fread(buf, filesize, 1, f);
	fclose(f);

	result = vga_font_load(font, buf, width, height);
	g_free(buf);

	return result;
}

/**
 * vga_font_load_default:
 * @font: the VGA font object
 *
 * Load the default font into VGA font object.
 */
void vga_font_load_default(VGAFont * font)
{
	vga_font_load(font, default_font, 8, 16);
}

/**
 * vga_font_load_default_8x8:
 * @font: the VGA font object
 *
 * Load the default font into VGA font object.
 */
void vga_font_load_default_8x8(VGAFont * font)
{
	vga_font_load(font, default_font_8x8, 8, 8);
}


/**
 * vga_font_pixels:
 * @font: the VGA font object
 * 
 * Returns: The number of pixels per character
 */
int vga_font_pixels(VGAFont * font)
{
	return font->width * font->height;
}


/* Bit flip/mirror a byte -- maybe there's a better way to do this */
static guchar bitflip(guchar c)
{
	int i;
	guchar y = 0, filter = 128;

	for (i = 7; i > 0; i -= 2)
	{
		y = (filter & c) >> i | y;
		filter = filter >> 1;
	}

	for (i = 1; i < 8; i += 2)
	{
		y = (filter & c) << i | y;
		filter = filter >> 1;
	}

	return y;
}

/**
 * vga_font_get_bitmap:
 * @font: the VGA font object
 *
 * Create a XBM representation of the font, with all characters in one column
 * with the same width as the character width of the font (so, for a standard
 * 8x16 VGA font, the returned bitmap is 8x4096).
 * 
 * Returns: The new GdkBitmap object representing the rendered font
 */
GdkBitmap * vga_font_get_bitmap(VGAFont * font, GdkWindow * win)
{
	unsigned char * xbm;
	int i, size;
	/* The XBM format is pretty similar to the raw VGA font, just
	 * mirrored horizontally.  So we flip/mirror the bits */
	/* We only really support 8-bit width fonts */
	g_assert(font->width == 8);

	size = font->height * 256;
	xbm = g_malloc(size);
	/* bitflip each byte */
	for (i = 0; i < size; i++)
		*(xbm + i) = bitflip(*(font->data + i));
	
	return gdk_bitmap_create_from_data(win, xbm, font->width, size);
}
