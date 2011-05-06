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
 *
 *  Cursor positions in this widget are both 1-based and relative to the
 *  current window area (default entire area).  This differs from the VGAText
 *  widget, where they are 0-based and absolute.
 */

#include <gdk/gdk.h>
#include "vgaterm.h"

static void vga_term_class_init	(VGATermClass * klass);
static void vga_term_init	(VGATerm * term);

/* Terminal private data */
struct _VGATermPrivate {
};


GType	vga_term_get_type(void)
{
	static GType vgaterm_type = 0;

	if (!vgaterm_type)
	{
		static const GTypeInfo term_info =
		{
			sizeof(VGATermClass),
			(GBaseInitFunc)NULL,
			(GBaseFinalizeFunc)NULL,
			(GClassInitFunc) vga_term_class_init,
			(GClassFinalizeFunc)NULL,
			(gconstpointer)NULL,
			sizeof(VGATerm),
			0,
			(GInstanceInitFunc)vga_term_init,
			(GTypeValueTable*)NULL
		};

		vgaterm_type = g_type_register_static(VGA_TYPE_TEXT,
							"VGATerm",
							&term_info, 0);
	}

	return vgaterm_type;
}

static void vga_term_class_init(VGATermClass * klass)
{
	GtkWidgetClass * widget_class;

#ifdef VGA_DEBUG
	fprintf(stderr, "vga_term_class_init()\n");
#endif

	widget_class = (GtkWidgetClass *) klass;

	/* widget_class->size_request = vga_term_size_request; */
	/* same for size_allocate */
}

static void vga_term_init(VGATerm * term)
{
	struct _VGATermPrivate * pvt;

#ifdef VGA_DEBUG
	fprintf(stderr, "vga_term_init()\n");
#endif

	/* Initialize public fields */
	term->textattr = 0x07;
	term->win_top_left_x = 1;
	term->win_top_left_y = 1;
	term->win_bot_right_x = 80;
	term->win_bot_right_y = 25;

	
	/* Initialize private data */
	pvt = term->pvt = g_malloc0(sizeof(*term->pvt));
}


GtkWidget * vga_term_new(GtkAdjustment *adjustment, int lines)
{
  VGATerm *term;
  
  term = gtk_type_new(vga_term_get_type());

  if (!adjustment)
    adjustment = (GtkAdjustment*) gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

  vga_term_set_adjustment(GTK_WIDGET(term), adjustment);

  vga_set_rows(GTK_WIDGET(term), lines);

  return GTK_WIDGET(term);
}

GtkAdjustment* vga_term_get_adjustment (GtkWidget *widget)
{
  VGATerm * term;

  g_return_if_fail(widget != NULL);
  g_return_if_fail(VGA_IS_TERM(widget));
  term = VGA_TERM(widget);

  return term->adjustment;
}

void vga_term_set_adjustment(GtkWidget *widget, GtkAdjustment *adjustment)
{
  VGATerm * term;

  g_return_if_fail(widget != NULL);
  g_return_if_fail(VGA_IS_TERM(widget));
  term = VGA_TERM(widget);

  if(term->adjustment)
  {
    gtk_signal_disconnect_by_data (GTK_OBJECT (term->adjustment), (gpointer) term);
    gtk_object_unref(GTK_OBJECT(term->adjustment));
  }
  
  term->adjustment = adjustment;
  gtk_object_ref(GTK_OBJECT(term->adjustment));

  g_signal_connect_swapped(term->adjustment,
			   "value-changed",
			   G_CALLBACK(vga_term_handle_scroll),
			   term);

  /*
  dial->old_value = adjustment->value;
  dial->old_lower = adjustment->lower;
  dial->old_upper = adjustment->upper;

  gtk_dial_update (dial);
  */
}

/**
 * vga_term_process_char:
 * @widget: VGA Terminal widget
 * @c: Character to process
 *
 * Update display in memory for printing a character but don't actually
 * refresh the physical display.
 */
static
void __deprecated_vga_term_process_char(GtkWidget * widget, guchar c)
{
	static guchar lc = '\0';		/* Last character */
	gboolean cursor_vis;
	VGATerm * term;
	int x = -1, y = -1;
	
	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TERM(widget));
	term = VGA_TERM(widget);

	switch (c)
	{
		case 10:
			x = vga_term_wherex(widget);
			y = vga_term_wherey(widget) + 1;
			if (lc != 13)
				x = 1;
			break;
		case 13:
			x = 1;
			y = vga_term_wherey(widget);
			break;
		case 8:
			x = vga_term_wherex(widget) - 1;
			y = vga_term_wherey(widget);
			break;
		default:
			//buf = vga_get_video_buf(widget);
			/*
			ofs = buf + (vga_cursor_y(widget) *
					vga_get_cols(widget) +
					vga_cursor_x(widget)) * 2;
			*(ofs) = c;
			*(ofs + 1) = term->textattr;
			*/
			x = vga_cursor_x(widget);
			y = vga_cursor_y(widget);
			vga_put_char(widget, c, term->textattr, x, y);

			/* Go to next line? */
			if ((x+1) == term->win_bot_right_x)
			{
				x = 1;
				/* Need to scroll? */
				if ((y+1) == term->win_bot_right_y)
				{
					cursor_vis =
						vga_cursor_is_visible(widget);
					if (cursor_vis)
						vga_cursor_set_visible(widget,
								FALSE);
					vga_cursor_move(widget,
						term->win_top_left_x - 1,
						term->win_top_left_y - 1);
					vga_term_dellines(widget, vga_term_wherey(widget), 1);
					x = term->win_top_left_x;
					y = term->win_bot_right_y;
					vga_cursor_move(widget, x-1, y-1);
					if (cursor_vis)
						vga_cursor_set_visible(widget,
								TRUE);
					/* Don't need to update cursor */
					x = -1;
					y = -1;
				}
				else
					y++;
			}
			else
			{
				x++;
				y = vga_term_wherey(widget);
			}
	}
	lc = c;
	if (x != -1 && y != -1)
	{
		vga_cursor_move(widget, x - 1, y - 1);
		g_print("[x->%d, y->%d]", x-1, y-1);
	}
}

void vga_term_writec(GtkWidget * widget, guchar c)
{
	static guchar lc = '\0';		/* Last character */
	gboolean cursor_vis;
	VGATerm * term;
	int x = -1, y = -1, cx, cy;
	
	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TERM(widget));
	term = VGA_TERM(widget);

	//g_print("%c", c);
	switch (c)
	{
		case 10:
			x = vga_term_wherex(widget);
			y = vga_term_wherey(widget) + 1;
			/* I Don't know about this.
			 * Can't some ansi files and boards depend on this
			 * behavior?  what if they send JUST a newline
			 * because that's what they really want?
			 *
			 * I had this problem with some .tfx files
			 * that depended on this behavior
			 *
			 * Decision: make this an option.
			 * Translate LF->CR+LF.  Off by default..
			 *
			 * Should be settable for directory entries
			 */
			if (lc != 13)
				x = 1;
			break;
		case 13:
			x = 1;
			y = vga_term_wherey(widget);
			break;
		case 7:
			gdk_beep();
			break;
		case 8:
			x = MAX(vga_term_wherex(widget) - 1, 1);
			y = vga_term_wherey(widget);
			break;
		default:
			/*buf = vga_get_video_buf(widget);
			
			ofs = buf + (vga_cursor_y(widget) *
					vga_get_cols(widget) +
					vga_cursor_x(widget)) * 2;
			*(ofs) = c;
			*(ofs + 1) = term->textattr;
			*/
			x = vga_term_wherex(widget);
			y = vga_term_wherey(widget);
			cx = vga_cursor_x(widget);
			cy = vga_cursor_y(widget);
			/*
			if (c == '[')
				g_print("@<%d,%d>", cx, cy);
			*/
			vga_put_char(widget, c, term->textattr, cx, cy);

			/* Go to next line? */
			if (cx+1 == term->win_bot_right_x)
			{
				x = 1;
				y++;
			}
			else
				x++;
	}
	lc = c;
	/* Check if we need to scroll down */
	if (y > (term->win_bot_right_y - term->win_top_left_y + 1))
	{
		cursor_vis = vga_cursor_is_visible(widget);
		if (cursor_vis)
			vga_cursor_set_visible(widget, FALSE);
		vga_cursor_move(widget,	term->win_top_left_x - 1,
					term->win_top_left_y - 1);
		// JJS Start
		vga_term_scroll_down(GTK_WIDGET(term), 1);
		printf("win_top_left_y = %d, win_bot_right_y = %d\n", term->win_top_left_y, term->win_bot_right_y);
		// JJS End

		// ORIG: vga_term_dellines(widget, vga_term_wherey(widget), 1);

		x = term->win_top_left_x;
		y = term->win_bot_right_y;
		vga_cursor_move(widget, x-1, y-1);
		if (cursor_vis)
			vga_cursor_set_visible(widget, TRUE);
		/* Don't need to update cursor */
		x = -1;
		y = -1;
	}
	else
	if (x != -1 && y != -1)
	{
		vga_term_gotoxy(widget, x, y);
	}
}

gint vga_term_write(GtkWidget * widget, guchar * s)
{
	int i = 0;
	while (s[i] != '\0')
		vga_term_writec(widget, s[i++]);
	return i;
}

gint vga_term_writeln(GtkWidget * widget, guchar * s)
{
	return vga_term_write(widget, s) + vga_term_write(widget, "\r\n");
}


int vga_term_print(GtkWidget * widget, const gchar * format, ...)
{
	va_list args;
	gchar * string;

	g_return_val_if_fail(format != NULL, 0);

	va_start(args, format);
	string = g_strdup_vprintf(format, args);
	va_end(args);

	vga_term_write(widget, string);

	g_free(string);
	return strlen(string);
}


/* 
 * these input functions may be removed.  i'm thinking it will just
 * be handled best in the application level with key_press events
 */
gboolean vga_term_kbhit(GtkWidget * widget)
{
	return '\0';
}

guchar vga_term_readkey(GtkWidget * widget)
{
	return '\0';
}

void vga_term_window(GtkWidget * widget, int x1, int y1, int x2, int y2)
{
	VGATerm * term;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TERM(widget));
	term = VGA_TERM(widget);

	g_assert(x1 > 0 && x1 <= x2);
	g_assert(y1 > 0 && y1 <= y2);
	g_assert(x2 <= vga_get_cols(widget));
	g_assert(y2 <= vga_get_rows(widget));
			
	term->win_top_left_x = x1;
	term->win_top_left_y = y1;
	term->win_bot_right_x = x2;
	term->win_bot_right_y = y2;

	vga_term_gotoxy(widget, 1, 1);
}

void vga_term_gotoxy(GtkWidget * widget, int x, int y)
{
	VGATerm * term;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TERM(widget));
	term = VGA_TERM(widget);

	/* Boundary coercions */
	x = MAX(1, x);
	y = MAX(1, y);
	x = MIN(term->win_bot_right_x - term->win_top_left_x + 1, x);
	y = MIN(term->win_bot_right_y - term->win_top_left_y + 1, y);

	/* Adjust for window offsets */	
	x += term->win_top_left_x - 1;
	y += term->win_top_left_y - 1;

	vga_cursor_move(widget, x - 1, y - 1);
}

int vga_term_wherex(GtkWidget * widget)
{
	VGATerm * term;
	
	g_return_val_if_fail(widget != NULL, -1);
	g_return_val_if_fail(VGA_IS_TERM(widget), -1);
	term = VGA_TERM(widget);

	return vga_cursor_x(widget) - term->win_top_left_x + 2;
}


int vga_term_wherey(GtkWidget * widget)
{
	VGATerm * term;
	
	g_return_val_if_fail(widget != NULL, -1);
	g_return_val_if_fail(VGA_IS_TERM(widget), -1);
	term = VGA_TERM(widget);

	return vga_cursor_y(widget) - term->win_top_left_y + 2;
}

int vga_term_cols(GtkWidget * widget)
{
	VGATerm * term;
	
	g_return_val_if_fail(widget != NULL, -1);
	g_return_val_if_fail(VGA_IS_TERM(widget), -1);
	term = VGA_TERM(widget);

	return term->win_bot_right_x - term->win_top_left_x + 1;
}

int vga_term_rows(GtkWidget * widget)
{
	VGATerm * term;
	
	g_return_val_if_fail(widget != NULL, -1);
	g_return_val_if_fail(VGA_IS_TERM(widget), -1);
	term = VGA_TERM(widget);

	return term->win_bot_right_y - term->win_top_left_y + 1;
}

void vga_term_clrscr(GtkWidget * widget)
{
	VGATerm * term;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TERM(widget));
	term = VGA_TERM(widget);

	vga_clear_area(widget, SETBG(0x00, GETBG(term->textattr)),
			term->win_top_left_x - 1,
			term->win_top_left_y - 1,
			term->win_bot_right_x - term->win_top_left_x + 1,
			term->win_bot_right_y - term->win_top_left_y + 1);

	vga_cursor_move(widget, term->win_top_left_x - 1,
				term->win_top_left_y - 1);

#if 0
	VGATerm * term;
	guchar * video_buf;
	struct { guchar c; guchar attr; } cell;
	gint16 * cellword;
	int ofs, rows, cols, win_rows, win_cols, y;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TERM(widget));
	term = VGA_TERM(widget);

	rows = vga_get_rows(widget);
	cols = vga_get_cols(widget);
	
	vga_cursor_move(widget, 0, 0);  // wrong??
	
	video_buf = vga_get_video_buf(widget);
	cell.c = 0x00;
	cell.attr = SETBG(0x00, GETBG(term->textattr));
	cellword = (gint16 *) &cell;
	win_rows = term->win_bot_right_y - term->win_top_left_y + 1;
	win_cols = term->win_bot_right_x - term->win_top_left_x + 1;
	/* Special case optimization */
	if (win_cols == cols)
	{
		ofs = (term->win_top_left_y - 1) * cols * 2;
		memsetword(video_buf + ofs, *cellword, cols * rows); 
	}
	else
	{
		for (y = term->win_top_left_y - 1; y < win_rows; y++)
		{
			ofs = (y * cols + term->win_top_left_x - 1) * 2;
			memsetword(video_buf + ofs, *cellword, win_cols);
		}
	}
	vga_refresh_region(widget, term->win_top_left_x, term->win_top_left_y,
					win_cols, win_rows);
#endif
}

/* clears down using 0x00 attr, NOT current textattr */
/* untested */
void vga_term_clrdown(GtkWidget * widget)
{
	VGATerm * term;
	int y;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TERM(widget));
	term = VGA_TERM(widget);

	y = vga_cursor_y(widget);
	vga_clear_area(widget, 0x00,
			term->win_top_left_x - 1,
			y,
			term->win_bot_right_x - term->win_top_left_x + 1,
			term->win_bot_right_y - y);
}

/* clears up using 0x00 attr, NOT current textattr */
/* untested */
void vga_term_clrup(GtkWidget * widget)
{
	VGATerm * term;
	int y;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TERM(widget));
	term = VGA_TERM(widget);

	y = vga_cursor_y(widget);
	vga_clear_area(widget, 0x00,
			term->win_top_left_x - 1,
			term->win_top_left_y - 1,
			term->win_bot_right_x - term->win_top_left_x + 1,
			y + 1);
}

void vga_term_clreol(GtkWidget * widget)
{
	VGATerm * term;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TERM(widget));
	term = VGA_TERM(widget);

	vga_clear_area(widget, SETBG(0x00, GETBG(term->textattr)),
			vga_cursor_x(widget), vga_cursor_y(widget),
			(term->win_bot_right_x - term->win_top_left_x + 1) -
				vga_term_wherex(widget) + 1, 1);
			
#if 0
	VGATerm * term;
	guchar * video_buf;
	struct { guchar c; guchar attr; } cell;
	gint16 * cellword;
	int ofs, cols, win_cols, x, y, clrcols;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TERM(widget));
	term = VGA_TERM(widget);

	cols = vga_get_cols(widget);  // wrong!?
	
	vga_cursor_move(widget, 0, 0);
	
	video_buf = vga_get_video_buf(widget);
	cell.c = 0x00;
	cell.attr = SETBG(0x00, GETBG(term->textattr));
	cellword = (gint16 *) &cell;
	win_cols = term->win_bot_right_x - term->win_top_left_x + 1;
	x = vga_cursor_x(widget);
	y = vga_cursor_y(widget);
	ofs = (y * cols + x) * 2;
	clrcols = win_cols - vga_term_wherex(widget) + 1;
	memsetword(video_buf + ofs, *cellword, clrcols);
	vga_refresh_region(widget, x, vga_cursor_y(widget), clrcols, 1);
#endif
}

void vga_term_scroll_up(GtkWidget * widget, int lines)
{
  VGATerm * term;
  VGAFont *termfont;
  
  g_return_if_fail(widget != NULL);
  g_return_if_fail(VGA_IS_TERM(widget));
  term = VGA_TERM(widget);
  
  term->win_top_left_y  -= lines;
  term->win_bot_right_y -= lines;

  termfont = vga_get_font(&term->vga);
  gtk_adjustment_set_value(term->adjustment, term->adjustment->value - termfont->height);
}

void vga_term_scroll_down(GtkWidget * widget, int lines)
{
  VGATerm * term;
  VGAFont *termfont;
  int buf_rows;

  g_return_if_fail(widget != NULL);
  g_return_if_fail(VGA_IS_TERM(widget));
  term = VGA_TERM(widget);
  
  term->win_top_left_y  += lines;
  term->win_bot_right_y += lines;

  buf_rows = vga_get_rows(widget);
  if(term->win_bot_right_y > buf_rows)
  {
    int diff = term->win_bot_right_y - buf_rows;

    printf("exceeded buffer, diff = %d\n", diff);

    term->win_top_left_y  -= diff;
    term->win_bot_right_y -= diff;

    vga_term_dellines_absolute(widget, 1, diff);
  }
  else
  {
    termfont = vga_get_font(&term->vga);
    gtk_adjustment_set_value(term->adjustment, term->adjustment->value + termfont->height);
  }
}

void vga_term_handle_scroll(GtkWidget * widget)
{
  VGATerm * term;
  VGAFont *termfont;
  
  g_return_if_fail(widget != NULL);
  g_return_if_fail(VGA_IS_TERM(widget));
  term = VGA_TERM(widget);

  printf("***WARNING*** vga_term_handle_scroll not implemented!\n");
}

void vga_term_dellines_absolute(GtkWidget * widget, int top_row, int lines)
{
	VGATerm * term;
	guchar * video_buf;
	int ofs, len, cols, win_cols, rows, y, start_y, end_y;
	
	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TERM(widget));
	term = VGA_TERM(widget);

	cols = vga_get_cols(widget);
	rows = vga_get_rows(widget);

	video_buf = vga_get_video_buf(widget);
	win_cols = term->win_bot_right_x - term->win_top_left_x + 1;
	start_y = top_row - 1;
	end_y = rows - lines;
	
	/* 
	 * In the case where the window is as wide as the display, we
	 * can optimize the shifting by using a single memmove() call
	 */
	if (win_cols == cols)
	{
	  ofs = start_y * cols * 2;
	  len = (rows * cols * 2) - ofs;
	  memmove(video_buf + ofs, video_buf + ofs + cols*2*lines, len);
	}
	else
	{

	}
	
	/* Now clear the free'd up lines at the bottom */
	vga_clear_area(widget, SETBG(0x00, GETBG(term->textattr)),
			term->win_top_left_x - 1, end_y, win_cols, lines);
		
	/* FIXME: Refresh region only */
	vga_refresh(widget);
}

/**
 * vga_term_dellines:
 * @widget: VGATerm to operate on
 * @top_row: Top row (relative to current window) of scroll region
 * @lines: Number of lines to scroll up
 *
 * From the top row, shift all lines from it and below up
 * one row.  The current cursor's line becomes overwritten with the contents
 * of the row under it.  The last row in the display becomes a blank line.
 */
void vga_term_dellines(GtkWidget * widget, int top_row, int lines)
{
	VGATerm * term;
	guchar * video_buf;
	//struct { guchar c; guchar attr; } cell;
	//gint16 * cellword;
	int ofs, cols, win_cols, y, start_y, end_y;
	
	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TERM(widget));
	term = VGA_TERM(widget);

	cols = vga_get_cols(widget);
	
	video_buf = vga_get_video_buf(widget);
	win_cols = term->win_bot_right_x - term->win_top_left_x + 1;
	// start_y = relative_to_absolute(top_row)
	start_y = term->win_top_left_y + top_row - 2;
	end_y = term->win_bot_right_y - lines;
	
	/* 
	 * In the case where the window is as wide as the display, we
	 * can optimize the shifting by using a single memmove() call
	 */
	if (win_cols == cols)
	{
		ofs = start_y * cols * 2;
		memmove(video_buf + ofs, video_buf + ofs + cols*2*lines,
				cols*2*(term->win_bot_right_y - lines));
	}
	else
	{
		for (y = start_y; y < end_y; y++)
		{
			/* Source is line below y at column of window start */
			ofs = (y*cols + term->win_top_left_x - 1) * 2;
			/* Shift line up */
			memmove(video_buf + ofs, video_buf + ofs + cols*2,
					win_cols*2);
		}
	}
	
	/* Now clear the free'd up lines at the bottom */
	vga_clear_area(widget, SETBG(0x00, GETBG(term->textattr)),
			term->win_top_left_x - 1, end_y, win_cols, lines);
		
/*	
	cell.c = 0x00;
	cell.attr = SETBG(0x00, GETBG(term->textattr));
	cellword = (gint16 *) &cell;
	ofs = (end_y*cols + term->win_top_left_x-1) * 2;
	for (y = 0; y < lines; y++)
	{
		memsetword(video_buf + ofs, *cellword, win_cols);
		ofs += (2*cols);
	}
	*/

	/* FIXME: Refresh region only */
	vga_refresh(widget);
}

/**
 * vga_term_inslines:
 * @widget: VGATerm widget to use
 *
 * From the top row, shift all lines from it and below down
 * one row.  The last row in the display is truncated, and the current
 * line becomes a blank line.
 **/
void vga_term_inslines(GtkWidget * widget, int top_row, int lines)
{
	VGATerm * term;
	guchar * video_buf;
	//struct { guchar c; guchar attr; } cell;
	//gint16 * cellword;
	int ofs, cols, win_cols, y, start_y;
	
	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TERM(widget));
	term = VGA_TERM(widget);

	cols = vga_get_cols(widget);
	
	video_buf = vga_get_video_buf(widget);
	win_cols = term->win_bot_right_x - term->win_top_left_x + 1;
	// start_y = relative_to_absolute(top_row)
	start_y = term->win_top_left_y + top_row - 2;

	/* 
	 * In the case where the window is as wide as the display, we
	 * can optimize the shifting by using a single memmove() call
	 */
	if (win_cols == cols)
	{
		ofs = start_y * cols * 2;
		memmove(video_buf + ofs + cols*2*lines, video_buf + ofs,
				cols*2*(term->win_bot_right_y - start_y - 1));
	}
	else
	{
		/* Start from the bottom */
		for (y = term->win_bot_right_y - 1; y > start_y; y--)
		{
			/* Source is line above y at column of window start */
			ofs = ((y-1)*cols + term->win_top_left_x - 1) * 2;
			/* Shift line down */
			memmove(video_buf + ofs + win_cols*2, video_buf + ofs,
					win_cols*2);
		}
	}
	
	/* Now clear the gap lines */
	vga_clear_area(widget, SETBG(0x00, GETBG(term->textattr)),
		term->win_top_left_x - 1, start_y, win_cols, lines);
			
#if 0	
	cell.c = 0x00;
	cell.attr = SETBG(0x00, GETBG(term->textattr));
	cellword = (gint16 *) &cell;
	ofs = (start_y * cols + term->win_top_left_x - 1) * 2;
	for (y = 0; y < lines; y++)
	{
		memsetword(video_buf + ofs, *cellword, win_cols);
		ofs += (2*cols);
	}
#endif

	/* FIXME: Refresh region only */
	vga_refresh(widget);
}

void vga_term_set_attr(GtkWidget * widget, guchar textattr)
{
	VGATerm * term;
	
	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TERM(widget));
	term = VGA_TERM(widget);

	term->textattr = textattr;
}

guchar vga_term_get_attr(GtkWidget * widget)
{
	VGATerm * term;
	
	g_assert(widget != NULL);
	g_assert(VGA_IS_TERM(widget));
	term = VGA_TERM(widget);

	return term->textattr;
}

void vga_term_set_fg(GtkWidget * widget, guchar fg)
{
	VGATerm * term;
	
	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TERM(widget));
	term = VGA_TERM(widget);

	if (fg > 15)
		fg = (fg & 0x0F) | 0x80;
	term->textattr = (term->textattr & 0x70) | fg;
}

void vga_term_set_bg(GtkWidget * widget, guchar bg)
{
	VGATerm * term;
	
	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TERM(widget));
	term = VGA_TERM(widget);

	term->textattr = (term->textattr & 0x8F) | ((bg & 0x07) << 4);
}
