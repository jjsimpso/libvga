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
 *
 *  This file implements the VGA terminal Gtk+ widget, based on the VGA
 *  Text Mode widget.  It provides convienence methods to give a terminal-like
 *  API (for things like printing strings, while knowing to scroll the screen).
 *  This is similar to what some of the BIOS functions would implement.
 */


#ifndef __VGA_TERM_H__
#define __VGA_TERM_H__

#include <gdk/gdk.h>
#include "vgatext.h"
#include "emulation.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define VGA_TYPE_TERM		(vga_term_get_type())
#define VGA_TERM(obj)		(GTK_CHECK_CAST((obj), VGA_TYPE_TERM, VGATerm))
#define VGA_TERM_CLASS(klass)	(GTK_CHECK_CLASS_CAST((klass), VGA_TYPE_TERM, VGATermClass))
#define VGA_IS_TERM(obj)	(GTK_CHECK_TYPE((obj), VGA_TYPE_TERM))
#define IS_VGA_TERM		VGA_IS_TERM
#define VGA_IS_TERM_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), VGA_TYPE_TERM))
#define VGA_TERM_GET_CLASS(obj)	(GTK_CHECK_GET_CLASS((obj), VGA_TYPE_TERM, VGATermClass))

typedef struct _VGATerm	VGATerm;
typedef struct _VGATermClass VGATermClass;

struct _VGATerm
{
	VGAText vga;
	GtkAdjustment *adjustment;	/* Scrolling adjustment */

	/* <public> -- though should be read-only really */

	guchar textattr;
	int win_top_left_x, win_top_left_y;
	int win_bot_right_x, win_bot_right_y;
	
	/* <private> */
	struct _VGATermPrivate * pvt;
};

struct _VGATermClass
{
	VGATextClass parent_class;
};

GType		vga_term_get_type	(void)	G_GNUC_CONST;
  GtkWidget * 	vga_term_new		(GtkAdjustment *adjustment, int lines);
void            vga_term_set_adjustment (GtkWidget *term, GtkAdjustment *adjustment);
void		vga_term_writec		(GtkWidget * widget, guchar c);
gint		vga_term_write		(GtkWidget * widget, guchar * s);
gint		vga_term_writeln	(GtkWidget * widget, guchar * s);
int		vga_term_print		(GtkWidget * widget,
						const gchar * format, ...);

gboolean	vga_term_kbhit		(GtkWidget * widget);
guchar		vga_term_readkey	(GtkWidget * widget);
void		vga_term_window		(GtkWidget * widget, int x1, int y1,
						int x2, int y2);

void		vga_term_gotoxy		(GtkWidget * widget, int x, int y);
int		vga_term_wherex		(GtkWidget * widget);
int		vga_term_wherey		(GtkWidget * widget);
int		vga_term_cols		(GtkWidget * widget);
int		vga_term_rows		(GtkWidget * widget);
void		vga_term_clrscr		(GtkWidget * widget);
void		vga_term_clrdown	(GtkWidget * widget);
void		vga_term_clrup		(GtkWidget * widget);
void		vga_term_clreol		(GtkWidget * widget);
void            vga_term_scroll_up      (GtkWidget * widget, int lines);
void            vga_term_scroll_down    (GtkWidget * widget, int lines);
void            vga_term_handle_scroll  (GtkWidget * widget);
void            vga_term_dellines_absolute(GtkWidget * widget, int top_row, int lines);
void		vga_term_dellines	(GtkWidget * widget, int start_row,
							int lines);
void		vga_term_inslines	(GtkWidget * widget, int start_row,
							int lines);
void		vga_term_set_attr	(GtkWidget * widget, guchar textattr);
guchar		vga_term_get_attr	(GtkWidget * widget);
void		vga_term_set_fg		(GtkWidget * widget, guchar fg);
void		vga_term_set_bg		(GtkWidget * widget, guchar bg);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* __VGA_TERM_H__ */
