/*
 *  Copyright (C) 2002 Nate Case 
 *
 *  (2007) Additional modifications by Jonathan Simpson
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
 *****************************************************************************
 * 
 *  This file implements the Gtk+ widget for the VGA Text Mode display
 *  emulator.  It provides a display (default 80x25 characters) with text
 *  rendered by VGA fonts, VGA palette capabilities, as well as other
 *  display details commonly used in DOS programs (such as toggling ICE
 *  color mode to get high-intensity backgrounds without blinking).
 *
 *  The interface for writing to this emulated display is by using the
 *  character cell manipulation methods, or by writing to the video buffer
 *  display region (similar to 0xB800 segment from DOS).  Writing to the
 *  video buffer directly requires that you call the appropriate refresh
 *  method before any drawing actually takes place.
 *
 *  This is *NOT* a terminal widget.  See VGATerm for a terminal widget
 *  implemented using this (VGAText).
 *
 *  All XY coordinates are 0-based.
 *
 *  This widget does not do any key input handling.
 *
 *  Many ideas and some code taken from the VTE widget, so a lot of credit
 *  for this goes to Nalin Dahyabhai from Red Hat.
 *
 */


#ifndef __VGA_TEXT_H__
#define __VGA_TEXT_H__

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <glib.h>
#include <gtk/gtk.h>
#include "vgafont.h"
#include "vgapalette.h"


/* Standard byte-representation helpers for VGA text-mode attributes */

#define PURPLE MAGENTA
#define GRAY GREY
typedef enum
{
        BLACK, BLUE, GREEN, CYAN, RED, MAGENTA, BROWN, GREY
} base_color;

typedef enum
{
        DARKGREY=8, LIGHTBLUE, LIGHTGREEN, LIGHTCYAN, ORANGE, PINK, YELLOW, WHITE
} bright_color;

/* VGA text attribute byte manipulation */
#define GETFG(attr) (attr & 0x0F)
#define GETBG(attr) ((attr & 0x70) >> 4)
#define GETBLINK(attr) (attr >> 7)
#define BRIGHT(col) (col | 0x08)
#define SETFG(attr, fg) ((attr & 0xF0) | fg)
#define SETBG(attr, bg) ((attr & 0x8F) | (bg << 4))
#define BLINK(col) (col | 0x80)
#define ATTR(fg, bg) ((bg << 4) | fg)


G_BEGIN_DECLS

typedef struct
{
	guchar c;		/* The ASCII character */
	guchar attr;		/* The text attribute */
} vga_charcell;

/* The widget itself */
typedef struct _VGAText
{
	/*< public >*/
	
	/* Implementation stuff */
	GtkWidget widget;

	/*< private >*/
	struct _VGATextPrivate * pvt;
} VGAText;

/* The widget's class structure */
typedef struct _VGATextClass
{
	/*< public >*/
	/* Inherited parent class */
	GtkWidgetClass parent_class;

	/*< private >*/
	/* Signals we might emit */
	guint contents_changed_signal;
	guint refresh_window_signal;
	guint move_window_signal;

	gpointer reserved1;
	gpointer reserved2;
	gpointer reserved3;
	gpointer reserved4;
} VGATextClass;

GtkType vga_get_type(void);

#define VGA_TYPE_TEXT	               (vga_get_type())
#define VGA_TEXT(obj)               (GTK_CHECK_CAST((obj),\
                                                        VGA_TYPE_TEXT,\
                                                        VGAText))
#define VGA_TEXT_CLASS(klass)       GTK_CHECK_CLASS_CAST((klass),\
                                                             VGA_TYPE_TEXT,\                                                             VGATextClass)
#define VGA_IS_TEXT	VGA_IS_VGATEXT
#define VGA_IS_VGATEXT(obj)            GTK_CHECK_TYPE((obj),\
                                                       VGA_TYPE_TEXT)
#define VGA_TEXT_IS_VGA_CLASS(klass)    GTK_CHECK_CLASS_TYPE((klass),\
                                                             VGA_TYPE_TEXT)
#define VGA__GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), VGA_TYPE_TEXT, VGATextClass))


GtkWidget *	vga_text_new(gint rows, gint cols);
void		vga_cursor_set_visible(GtkWidget * widget, gboolean visible);
gboolean	vga_cursor_is_visible(GtkWidget * widget);

void		vga_cursor_move(GtkWidget * widget, int x, int y);
void		vga_set_palette(GtkWidget * widget, VGAPalette * palette);
VGAPalette *	vga_get_palette(GtkWidget * widget);
VGAFont *	vga_get_font(GtkWidget * font);
void		vga_refresh_font(GtkWidget * font);
void		vga_set_font(GtkWidget * widget, VGAFont * font);
void		vga_set_icecolor(GtkWidget * widget, gboolean status);
gboolean	vga_get_icecolor(GtkWidget * widget);
vga_charcell *  vga_get_char(GtkWidget * widget, int col, int row);
void		vga_put_char(GtkWidget * widget, guchar c, guchar attr,
					int col, int row);
void		vga_put_string(GtkWidget * widget, guchar * s, guchar attr,
					int col, int row);
guchar *	vga_get_video_buf(GtkWidget * widget);
int		vga_cursor_x(GtkWidget * widget);
int		vga_cursor_y(GtkWidget * widget);
void		vga_refresh_region(GtkWidget * widget,
					int top_left_x, int top_left_y,
					int cols, int rows);
void		vga_refresh(GtkWidget * widget);
void            vga_set_rows(GtkWidget * widget, int rows);
void            vga_set_cols(GtkWidget * widget, int cols);
int		vga_get_rows(GtkWidget * widget);
int		vga_get_cols(GtkWidget * widget);
void		vga_clear_area(GtkWidget * widget, guchar attr, int top_left_x,
				int top_left_y, int cols, int rows);
int             vga_video_buf_size(GtkWidget * widget);
void		vga_video_buf_clear(GtkWidget * widget);

G_END_DECLS

#endif	/* __VGA_TEXT_H__ */
