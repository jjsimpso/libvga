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
 *  for performance reasons]
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

#include "vgatext.h"

/* FIXME: temp before release */
#ifdef ENABLE_DEBUG
#define VGA_DEBUG
#endif

#define PIXEL_TO_COL(x, vgafont)	(x / vgafont->width)
#define PIXEL_TO_ROW(y, vgafont)	(y / vgafont->height)

#define CURSOR_BLINK_PERIOD_MS	229
#define BLINK_PERIOD_MS		498


typedef struct _VGAScreen VGAScreen;

/* Widget private data */
struct _VGATextPrivate {
	/* int keypad? */
	int rows;
	int cols;
	vga_charcell * video_buf;
	VGAFont * font;
	VGAPalette * pal;
	gboolean icecolor;
	gboolean cursor_visible;
	int cursor_x;		/* 0-based */
	int cursor_y;

	GdkBitmap * glyphs;
	GdkGC * gc;
	guchar fg, bg;	/* Local copy of gc text attribute state */
	
	gboolean cursor_blink_state;
	guint cursor_timeout_id;

	gboolean blink_state;
	guint blink_timeout_id;	/* -1 when no blinking chars on screen */
};

/* These are the palette registers for the standard EGA colors */
/* FIXME: Should this detail go in the palette class? */
static int pal_map[16] = {0,1,2,3,4,5,20,7,56,57,58,59,60,61,62,63};


/* Local Prototypes */
static void vga_alloc_videobuf(VGAText *vga);




GtkWidget * vga_text_new(gint rows, gint cols)
{
        VGAText *vga;

#ifdef VGA_DEBUG
	fprintf(stderr, "vga_text_new()\n");
#endif

	vga = g_object_new(vga_get_type(), NULL);

	vga->pvt->rows = rows;
	vga->pvt->cols = cols;
	
	vga_alloc_videobuf(vga);

	return GTK_WIDGET(vga);
}

static void
vga_alloc_videobuf(VGAText *vga)
{
  if(vga->pvt->video_buf != NULL)
  {
    g_free(vga->pvt->video_buf);
  }
  
  vga->pvt->video_buf = g_malloc0(sizeof(vga_charcell) * vga->pvt->rows * vga->pvt->cols);

}

static void
vga_invalidate_cells(VGAText * vga, glong col_start, gint col_count,
			glong row_start, gint row_count)
{
	GdkRectangle rect;
	GtkWidget * widget;
	
	g_return_if_fail(VGA_IS_VGATEXT(vga));
	widget = GTK_WIDGET(vga);
	if (!GTK_WIDGET_REALIZED(widget))
		return;

	/* Convert the col/row start and end to pixel values by multiplying
	 * by the size of a character cell. */
	rect.x = col_start * vga->pvt->font->width;
	rect.width = col_count * vga->pvt->font->width;
	rect.y = row_start * vga->pvt->font->height;
	rect.height = row_count * vga->pvt->font->height;

	gdk_window_invalidate_rect(widget->window, &rect, TRUE);
}

static void
vga_invalidate_all(VGAText * vga)
{
	vga_invalidate_cells(vga, 0, vga->pvt->cols, 0, vga->pvt->rows);
}

/* Scroll a rectangular region up or down by a fixed number of lines. */
static void
vga_scroll_region(VGAText *vga,
			   long row, long count, long delta)
{
	GtkWidget *widget;
	gboolean repaint = TRUE;

	if ((delta == 0) || (count == 0))
		return;

	/* We only do this if we're scrolling the entire window. */
	if (row == 0 && count == vga->pvt->rows)
	{
		widget = GTK_WIDGET(vga);
		gdk_window_scroll(widget->window, 0, delta * vga->pvt->font->height);
		repaint = FALSE;
	}

	if (repaint)
	{
		/* We have to repaint the entire area. */
		vga_invalidate_cells(vga, 0, vga->pvt->cols, row, count);
	}
}


/* Emit a "contents_changed" signal. */
static void
vga_emit_contents_changed(VGAText * vga)
{
#ifdef VGA_DEBUG
	fprintf(stderr, "Emitting `contents-changed'.\n");
#endif
	g_signal_emit_by_name(vga, "contents-changed");
}

/* Emit a "cursor_moved" signal. */
static void
vga_emit_cursor_moved(VGAText * vga)
{
#ifdef VGA_DEBUG
	fprintf(stderr, "Emitting `cursor-moved'.\n");
#endif
	g_signal_emit_by_name(vga, "cursor-moved");
}

/* Emit a "refresh-window" signal. */
static void
vga_emit_refresh_window(VGAText * vga)
{
#ifdef VGA_DEBUG
	fprintf(stderr, "Emitting `refresh-window'.\n");
#endif
	g_signal_emit_by_name(vga, "refresh-window");
}

/* Emit a "move-window" signal. */
static void
vga_emit_move_window(VGAText * vga)
{
#ifdef VGA_DEBUG
	fprintf(stderr, "Emitting `move-window'.\n");
#endif
	g_signal_emit_by_name(vga, "move-window");
}

static void
vga_realize(GtkWidget * widget)
{
	GdkWindowAttr attributes;
	gint attributes_mask;
	//GdkCursor * cursor;
	VGAText * vga;

#ifdef VGA_DEBUG
	fprintf(stderr, "vga_realize()\n");
#endif
	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TEXT(widget));

	vga = VGA_TEXT(widget);

	/* blah blah, do lots of shit here */

	/* Set the realized flag. */
	GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);

	/* Main widget window */
	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = 0;
	attributes.y = 0;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual(widget);
	attributes.colormap = gtk_widget_get_colormap(widget);
	attributes.event_mask = gtk_widget_get_events(widget) |
				GDK_EXPOSURE_MASK |
				GDK_BUTTON_PRESS_MASK |
				GDK_BUTTON_RELEASE_MASK |
				GDK_POINTER_MOTION_MASK |
				GDK_BUTTON1_MOTION_MASK |
				GDK_KEY_PRESS_MASK |
				GDK_KEY_RELEASE_MASK;
	attributes_mask = GDK_WA_X |
			  GDK_WA_Y |
			  GDK_WA_VISUAL |
			  GDK_WA_COLORMAP;
	widget->window = gdk_window_new(gtk_widget_get_parent_window(widget),
					&attributes,
					attributes_mask);
	gdk_window_move_resize(widget->window,
			       widget->allocation.x,
			       widget->allocation.y,
			       widget->allocation.width,
			       widget->allocation.height);
	gdk_window_set_user_data(widget->window, widget);

	gdk_window_show(widget->window);


	/* Initialize private data that depends on the window */
	if (vga->pvt->gc == NULL)
	{
		vga->pvt->glyphs = vga_font_get_bitmap(vga->pvt->font,
				widget->window);
		vga->pvt->gc = gdk_gc_new(widget->window);
		// not needed i guess?
		//gdk_gc_set_colormap(vga->pvt->gc, attributes.colormap);
		gdk_gc_set_rgb_fg_color(vga->pvt->gc,
			&vga->pvt->pal->color[pal_map[vga->pvt->fg]]);
		gdk_gc_set_rgb_bg_color(vga->pvt->gc,
			&vga->pvt->pal->color[pal_map[vga->pvt->bg]]);
		gdk_gc_set_stipple(vga->pvt->gc, vga->pvt->glyphs);
		gdk_gc_set_fill(vga->pvt->gc, GDK_OPAQUE_STIPPLED);
	}

	/* create a gdk window?  is that what we really want? */
	gtk_widget_grab_focus(widget);
}

/* The window is being destroyed. */
static void
vga_unrealize(GtkWidget * widget)
{
	VGAText * vga;

#ifdef VGA_DEBUG
	fprintf(stderr, "vga_unrealize()\n");
#endif

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TEXT(widget));
	vga = VGA_TEXT(widget);

	/* this is now in finalize() */
	//vga_font_destroy(vga->pvt->font);
	//vga_palette_destroy(vga->pvt->pal);
	//g_free(vga->pvt->video_buf);

	if (GTK_WIDGET_MAPPED(widget))
	{
		gtk_widget_unmap(widget);
	}

	/* Remove the GDK Window */
	if (widget->window != NULL)
	{
		gdk_window_destroy(widget->window);
		widget->window = NULL;
	}
	
	/* Mark that we no longer have a GDK window */
	GTK_WIDGET_UNSET_FLAGS(widget, GTK_REALIZED);
}


static void
vga_paint_cursor(GtkWidget * widget, VGAFont * font, gboolean state,
			int x, int y)
{
	gdk_draw_rectangle(widget->window,
		state ? widget->style->white_gc : widget->style->black_gc,
		TRUE,	/* filled */
		x, y,
		font->width, font->height / 8);
}


static gboolean
vga_blink_cursor(gpointer data)
{
	VGAText * vga;

	if (!GTK_WIDGET_REALIZED(GTK_WIDGET(data)))
		return TRUE;
	
	vga = VGA_TEXT(data);

	/* Don't do anything if we're already how we want it */
	if (!vga->pvt->cursor_visible && !vga->pvt->cursor_blink_state)
		return TRUE;
	
	vga->pvt->cursor_blink_state = !vga->pvt->cursor_blink_state;
	vga_paint_cursor(GTK_WIDGET(data), vga->pvt->font,
				vga->pvt->cursor_blink_state,
				vga->pvt->cursor_x * vga->pvt->font->width,
				(vga->pvt->cursor_y + 1) *
					vga->pvt->font->height -
					(vga->pvt->font->height / 8) );

	return TRUE;

	/*
	vga->pvt->cursor_blink_id = g_timeout_add(CURSOR_BLINK_PERIOD_MS,
							vga_blink_cursor,
							data);

	return FALSE;
	*/
	
}
		
static gboolean
vga_blink_char(gpointer data)
{
	int x, y;
	GtkWidget * widget;
	VGAText * vga;

	widget = GTK_WIDGET(data);
	if (!GTK_WIDGET_REALIZED(widget))
		return TRUE;

	vga = VGA_TEXT(data);
	
	if (vga->pvt->icecolor)
		return TRUE;
	
	vga->pvt->blink_state = !vga->pvt->blink_state;

	for (y = 0; y < vga->pvt->rows; y++)
	{
		for (x = 0; x < vga->pvt->cols; x++)
		{
			/* 
			 * If you encounter a blink bit, refresh it
			 * and the rest of the line, then move on to the
			 * next line
			 */
			if (GETBLINK(vga->pvt->video_buf[y*vga->pvt->cols + x].attr))
			{
				g_print("blink bit encountered on line %d\n", y);
				vga_refresh_region(widget, x, y,
							vga->pvt->cols - x,
							1);
				break;
			}
		}
	}

	return TRUE;
}


static
void vga_start_blink_timer(VGAText * vga)
{
	vga->pvt->blink_timeout_id = g_timeout_add(BLINK_PERIOD_MS,
			vga_blink_char, vga);
}

/*
 * vga_set_textattr:
 * @vga: VGAtext object
 * @textattr: VGA text attribute byte
 *
 * Set the VGAText's graphics context to use the text attribute given using
 * the current VGA palette.  May not actually result in a call to the
 * graphics server, since redundant calls may be optimized out.
 */
static void
vga_set_textattr(VGAText * vga, guchar textattr)
{
	guchar fg, bg;
	
	/* 
	 * Blink logic for text attributes
	 * -----------------------------------
	 * 
	 * has no blink bit: normal (fg = fg, bg = bg)
	 * has blink bit, icecolor: high intensity bg (bg = hi(bg))
	 * has blink bit, no icecolor, blink_state off: fg = bg [hide]
	 * has blink bit, no icecolor, blink_state on: normal (fg = fg)
	 */ 
	if (!GETBLINK(textattr))
	{
		fg = GETFG(textattr);
		bg = GETBG(textattr);
	}
	else if (vga->pvt->icecolor)
	{	/* High intensity background / iCEColor */
		fg = GETFG(textattr);
		bg = BRIGHT(GETBG(textattr));
	}
	else if (vga->pvt->blink_state)
	{	/* Blinking, but in on state so it appears normal */
		fg = GETFG(textattr);
		bg = GETBG(textattr);

		if (vga->pvt->blink_timeout_id == -1)
			vga_start_blink_timer(vga);
	}
	else
	{	/* Hide, blink off state */
		fg = GETBG(textattr);
		bg = GETBG(textattr);

		if (vga->pvt->blink_timeout_id == -1)
			vga_start_blink_timer(vga);
	}

	if (vga->pvt->fg != fg)
	{
		gdk_gc_set_rgb_fg_color(vga->pvt->gc,
			&vga->pvt->pal->color[pal_map[fg]]);
		vga->pvt->fg = fg;
	}
	if (vga->pvt->bg != bg)
	{
		gdk_gc_set_rgb_bg_color(vga->pvt->gc,
			&vga->pvt->pal->color[pal_map[bg]]);
		vga->pvt->bg = bg;
	}
}

/* Deprecated?  Depend strictly on refresh?   This won't work for blinking
 * and friends */
static void
vga_paint_charcell(GtkWidget * da, VGAText * vga,
				vga_charcell cell, int x, int y)
{
	vga_set_textattr(vga, cell.attr);
	gdk_gc_set_ts_origin(vga->pvt->gc, x, y-(16 * cell.c));
	gdk_draw_rectangle(da->window, vga->pvt->gc, TRUE, x, y,
				vga->pvt->font->width, vga->pvt->font->height);
}


/*
 * vga_refresh_area:
 * @da: Drawing area widget
 * @vga: VGAText structure pointer
 * @area: Area to refresh
 */
static void
vga_refresh_area(GtkWidget * da, VGAText * vga, GdkRectangle * area)
{
	int x, y, x2, y2;
	int char_x, char_y, x_drawn, y_drawn, row, col, columns;
	x2 = area->x + area->width;	/* Last column in area + 1 */
	y2 = area->y + area->height;	/* Last row in area + 1 */
	x2 = MIN(x2, vga->pvt->font->width * vga->pvt->cols);
	y2 = MIN(y2, vga->pvt->font->height * vga->pvt->rows);
	y = area->y;
	vga_charcell * cell;

	columns = vga->pvt->cols;

	while (y < y2)
	{
		row = PIXEL_TO_ROW(y, vga->pvt->font);
		char_y = row * vga->pvt->font->height;
		y_drawn = (char_y + vga->pvt->font->height) - y;

		x = area->x;
		while (x < x2)
		{
		/*
		 * x       : pixel position to start drawing at
		 * y       : pixel position to start drawing at
		 * row     : row of character (usually 0-24)
		 * col     : column of character (usually 0-79)
		 * char_x  : pixel coordinate of beginning of whole character
		 * char_y  : pixel coordinate of beginning of whole character
		 * x_drawn : number of columns to draw of character
		 * y_drawn : number of rows to draw of character
		 */
			col = PIXEL_TO_COL(x, vga->pvt->font);
			char_x = col * vga->pvt->font->width;
			x_drawn = (char_x + vga->pvt->font->width) - x;
		
			cell = &(vga->pvt->video_buf[row * columns + col]);
		
			vga_set_textattr(vga, cell->attr);
			gdk_gc_set_ts_origin(vga->pvt->gc, char_x,
					char_y-(vga->pvt->font->height * cell->c));
			gdk_draw_rectangle(da->window, vga->pvt->gc, TRUE, x, y,
					x_drawn, y_drawn);

			x += x_drawn;
		}
		y += y_drawn;
	}
}

/* Draw the widget */
static void
vga_paint(GtkWidget * widget, GdkRectangle * area)
{
	VGAText * vga;

	/* Sanity checks */
	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TEXT(widget));
	g_return_if_fail(area != NULL);
	vga = VGA_TEXT(widget);
	if (!GTK_WIDGET_DRAWABLE(widget))
	{
		fprintf(stderr, "vga_paint(): widget not drawable!\n");
		return;
	}

	vga_refresh_area(widget, vga, area);
	
	/* 
	 * FIXME: The algorithm seems to work great.  I watch the debug output
	 * as it runs trying various things and it always calculates the
	 * right starting/stopping row/col.  However, still sometimes things
	 * are not drawn, but I'm lead to believe right now that the problem
	 * lies elsewhere.
	 * UPDATE: Correct.  We now use vga_refresh_area() which is a
	 * pixel-level based refresh rather than character level.  It
	 * works fine.  Commenting out approach below.
	 *
	 */
#if 0
	row_start = area->y / vga->pvt->font->height; /* verified */
	
	num_rows = area->height / vga->pvt->font->height + 1;
	row_stop = MIN(row_start + num_rows, vga->pvt->rows);
	
	num_cols = area->width / vga->pvt->font->width + 1;
	col_start = area->x / vga->pvt->font->width;
	col_stop = MIN(col_start + num_cols, vga->pvt->cols);
#ifdef VGA_DEBUG
	fprintf(stderr, "area->y = %d, area->height = %d\n", area->y, area->height);
	fprintf(stderr, "area->x = %d, area->width = %d\n", area->x, area->width);
	fprintf(stderr, "row_start = %d, row_stop = %d\n", row_start, row_stop);
	fprintf(stderr, "col_start = %d, col_stop = %d\n", col_start, col_stop);
	fprintf(stderr, "\tnum_cols = %d\n", num_cols);

#endif

	for (y = row_start; y < row_stop; y++)
		for (x = col_start; x < col_stop; x++)
		{
			vga_paint_charcell(widget, vga,
					vga->pvt->video_buf[y*80+x],
					x*vga->pvt->font->width,
					y*vga->pvt->font->height);
		}
#endif
	
}

static gint
vga_expose(GtkWidget * widget, GdkEventExpose * event)
{
#ifdef VGA_DEBUG
	fprintf(stderr, "vga_expose()\n");
#endif
	g_return_val_if_fail(VGA_IS_TEXT(widget), 0);
	if (event->window == widget->window)
	{
		vga_paint(widget, &event->area);
	}
	else
		g_assert_not_reached();
	return TRUE;
}


/* Perform final cleanups for the widget before it's free'd */
static void
vga_finalize(GObject * object)
{
	VGAText * vga;
	//GtkWidget * toplevel;
	GtkWidgetClass * widget_class;
#ifdef VGA_DEBUG
	fprintf(stderr, "vga_finalize()\n");
#endif
	
	g_return_if_fail(VGA_IS_TEXT(object));
	vga = VGA_TEXT(object);
	widget_class = g_type_class_peek(GTK_TYPE_WIDGET);


	/* Destroy its font object */
	vga_font_destroy(vga->pvt->font);

	/* Destroy palette */
	vga_palette_destroy(vga->pvt->pal);

	g_free(vga->pvt->video_buf);

	/* Remove the blink timeout functions */
	if (vga->pvt->cursor_timeout_id != -1)
		g_source_remove(vga->pvt->cursor_timeout_id);
	if (vga->pvt->blink_timeout_id != -1)
		g_source_remove(vga->pvt->blink_timeout_id);


	/* Call the inherited finalize() method. */
	if (G_OBJECT_CLASS(widget_class)->finalize)
	{
		(G_OBJECT_CLASS(widget_class))->finalize(object);
	}
}

static gint
vga_focus_in(GtkWidget * widget, GdkEventFocus * event)
{
#ifdef VGA_DEBUG
	fprintf(stderr, "vga_focus_in()\n");
#endif
	g_return_val_if_fail(VGA_IS_TEXT(widget), 0);
	GTK_WIDGET_SET_FLAGS(widget, GTK_HAS_FOCUS);
	//gtk_im_context_focus_in((VGA_TEXT(widget))->pvt->im_context);
	/* invalidate cursor once? */
	return TRUE;
}

static gint
vga_focus_out(GtkWidget * widget, GdkEventFocus * event)
{
#ifdef VGA_DEBUG
	fprintf(stderr, "vga_focus_out()\n");
#endif
	g_return_val_if_fail(VGA_IS_TEXT(widget), 0);
	GTK_WIDGET_UNSET_FLAGS(widget, GTK_HAS_FOCUS);
	//gtk_im_context_focus_out((VGA_TEXT(widget))->pvt->im_context);
	/* invalidate cursor once? */
	return TRUE;
}

/* Tell GTK+ how much space we need. */
static void
vga_size_request(GtkWidget * widget, GtkRequisition *req)
{
	VGAText * vga;
#ifdef VGA_DEBUG
	fprintf(stderr, "vga_size_request()\n");
#endif
	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TEXT(widget));
	vga = VGA_TEXT(widget);

	req->width = vga->pvt->font->width * vga->pvt->cols;
	req->height = vga->pvt->font->height * vga->pvt->rows;

#ifdef VGA_DEBUG
	fprintf(stderr, "Size request is %dx%d.\n",
			req->width, req->height);
#endif
}

/* Accept a given size from GTK+. */
static void
vga_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	VGAText * vga;
	glong width, height;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TEXT(widget));
	vga = VGA_TEXT(widget);

	width = allocation->width / vga->pvt->font->width;
	height = allocation->height / vga->pvt->font->height;

#ifdef VGA_DEBUG
	fprintf(stderr, "Sizing window to %dx%d (%ldx%ld).\n",
		allocation->width, allocation->height,
		width, height);
#endif

	/* Set our allocation to match the structure. */
	widget->allocation = *allocation;

	/* Set the size of the pseudo-terminal. */
	//vte_terminal_set_size(terminal, width, height);

	/* Adjust scrollback buffers to ensure that they're big enough. */
	//vte_terminal_set_scrollback_lines(terminal,
	//				  MAX(terminal->pvt->scrollback_lines,
//					      height));

	/* Resize the GDK window. */
	if (widget->window != NULL) {
		gdk_window_move_resize(widget->window,
				       allocation->x,
				       allocation->y,
				       allocation->width,
				       allocation->height);
		//vte_terminal_queue_background_update(terminal);
	}

	/* Adjust the adjustments. */
	//vte_terminal_adjust_adjustments(terminal);
}


static void
vga_class_init(VGATextClass *klass, gconstpointer data)
{
	GObjectClass *gobject_class;
	GtkWidgetClass *widget_class;

#ifdef VGA_DEBUG
	fprintf(stderr, "vga_class_init()\n");
#endif

	// bindtextdomain(PACKAGE, LOCALEDIR);
	
	gobject_class = G_OBJECT_CLASS(klass);
	widget_class = GTK_WIDGET_CLASS(klass);

	/* Override some of the default handlers */
	gobject_class->finalize = vga_finalize;
	widget_class->realize = vga_realize;
	widget_class->expose_event = vga_expose;
	widget_class->focus_in_event = vga_focus_in;
	widget_class->focus_out_event = vga_focus_out;
	widget_class->unrealize = vga_unrealize;
	widget_class->size_request = vga_size_request;
	widget_class->size_allocate = vga_size_allocate;
	//widget_class->get_accessible = vga_get_accessible;

	/* Register some signals of our own */
	klass->contents_changed_signal =
		g_signal_new("contents-changed",
			G_OBJECT_CLASS_TYPE(klass),
			G_SIGNAL_RUN_LAST,
			0,
			NULL,
			NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE, 0);
	
}


/*
 *  Initialize the VGA text widget after the base widget stuff is initialized.
 */
static void
vga_init(VGAText * vga, gpointer *klass)
{
	struct _VGATextPrivate * pvt;
	GtkWidget * widget;

#ifdef VGA_DEBUG
	fprintf(stderr, "vga_init()\n");
#endif

	g_return_if_fail(VGA_IS_TEXT(vga));
	widget = GTK_WIDGET(vga);
	GTK_WIDGET_SET_FLAGS(widget, GTK_CAN_FOCUS);

	/* Initialize private data */
	pvt = vga->pvt = g_malloc0(sizeof(*vga->pvt));
	pvt->font = vga_font_new();
	vga_font_load_default(pvt->font);
	//vga_font_load_from_file(pvt->font, "dump.fnt");

	pvt->pal = vga_palette_dup(vga_palette_stock(PAL_DEFAULT));

	//vga_palette_load_default(pvt->pal);
	/*if (vga_palette_load_from_file(pvt->pal, "dos.pal"))
		g_message("Loaded palette file successfully");
	else
	{
		g_message("Failed to load palette file, falling back to default"); */
	/*}*/
			
	pvt->rows = 25;
	pvt->cols = 80;

	pvt->fg = 0x07;
	pvt->bg = 0x00;

	pvt->cursor_visible = TRUE;

	/* These are initialized if needed in vga_realize() for now */
	pvt->glyphs = NULL;
	pvt->gc = NULL;

	vga_alloc_videobuf(vga);

	/*
	pvt->video_buf[0].c = '!';
	pvt->video_buf[0].attr = 0x09;
	pvt->video_buf[100].c = '@';
	pvt->video_buf[100].attr = 0x2A; */

	/* Add our cursor timeout event to make it blink */
	/* 229 ms is about how often the cursor blink is toggled in DOS */
	pvt->cursor_timeout_id = g_timeout_add(CURSOR_BLINK_PERIOD_MS,
					       vga_blink_cursor, vga);

	/* Leave this at -1 until we actually have characters blinking */
	pvt->blink_timeout_id = -1;

	pvt->blink_state = TRUE;
	pvt->cursor_blink_state = TRUE;
	pvt->icecolor = TRUE;

}

GtkType
vga_get_type(void)
{
	static GtkType vga_type = 0;
	static const GTypeInfo vga_info =
	{
		sizeof(VGATextClass),
		(GBaseInitFunc)NULL,
		(GBaseFinalizeFunc)NULL,

		(GClassInitFunc)vga_class_init,
		(GClassFinalizeFunc)NULL,
		(gconstpointer)NULL,

		sizeof(VGAText),
		0,
		(GInstanceInitFunc)vga_init,

		(GTypeValueTable*)NULL,
	};

	if (vga_type == 0)
	{
		vga_type = g_type_register_static(GTK_TYPE_WIDGET,
						"VGAText",
						&vga_info,
						0);
	}

	return vga_type;
}



/*************************************
 * Public methods
 *************************************/

VGAPalette *
vga_get_palette(GtkWidget * widget)
{
	VGAText * vga;

	g_return_val_if_fail(widget != NULL, NULL);
	g_return_val_if_fail(VGA_IS_TEXT(widget), NULL);
	vga = VGA_TEXT(widget);

	return vga->pvt->pal;
}


VGAFont *
vga_get_font(GtkWidget * widget)
{
	VGAText * vga;

	g_return_val_if_fail(widget != NULL, NULL);
	g_return_val_if_fail(VGA_IS_TEXT(widget), NULL);
	vga = VGA_TEXT(widget);

	return vga->pvt->font;
}

/* Override the default VGA palette */
void
vga_set_palette(GtkWidget * widget, VGAPalette * palette)
{
	VGAText * vga;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TEXT(widget));
	vga = VGA_TEXT(widget);

	/* FIXME: Poor error checking */
	vga_palette_destroy(vga->pvt->pal);
	vga->pvt->pal = palette;

	vga_refresh(widget);
}


/* Re-render the internal font data so that changes will be reflected on
 * the next display refresh. */
void
vga_refresh_font(GtkWidget * widget)
{
	VGAText * vga;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TEXT(widget));
	vga = VGA_TEXT(widget);

	g_object_unref(vga->pvt->glyphs);
	vga->pvt->glyphs = vga_font_get_bitmap(vga->pvt->font,
			widget->window);
	gdk_gc_set_stipple(vga->pvt->gc, vga->pvt->glyphs);
}

/* Override the default VGA font.  Refreshes the display. */
void
vga_set_font(GtkWidget * widget, VGAFont * font)
{
	VGAText * vga;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TEXT(widget));
	vga = VGA_TEXT(widget);

	if (vga->pvt->font != font)
	{
		vga_font_destroy(vga->pvt->font);
		vga->pvt->font = font;
	}

	vga_refresh_font(widget);
	vga_refresh(widget);
}

/*
 * Enable or disable iCECOLOR (use 'high intensity backgrounds' rather than
 * 'blink' for the blink bit of the text attribute.
 */
void
vga_set_icecolor(GtkWidget * widget, gboolean status)
{
	VGAText * vga;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TEXT(widget));
	vga = VGA_TEXT(widget);

	vga->pvt->icecolor = status;
}

gboolean vga_get_icecolor(GtkWidget * widget)
{
	VGAText * vga;

	g_assert(widget != NULL);
	g_assert(VGA_IS_TEXT(widget));
	vga = VGA_TEXT(widget);

	return vga->pvt->icecolor;
}

vga_charcell *
vga_get_char(GtkWidget * widget, int col, int row)
{
	VGAText * vga;
	int ofs;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TEXT(widget));
	vga = VGA_TEXT(widget);
	
	ofs = vga->pvt->cols * row + col;
	
	return &vga->pvt->video_buf[ofs];
}

/* Put a character on the screen */
void
vga_put_char(GtkWidget * widget, guchar c, guchar attr, int col, int row)
{
	VGAText * vga;
	int ofs;
	GdkRectangle area;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TEXT(widget));
	vga = VGA_TEXT(widget);

	/* Update video buffer */
	ofs = vga->pvt->cols * row + col;
	vga->pvt->video_buf[ofs].c = c;
	vga->pvt->video_buf[ofs].attr = attr;

	/* Refresh charcell */
	area.x = col * vga->pvt->font->width;
	area.y = row * vga->pvt->font->height;
	area.width = vga->pvt->font->width;
	area.height = vga->pvt->font->height;
	vga_refresh_area(widget, vga, &area);
}

/* Put a string on the screen.  String will be truncated if exceeds screen
 * width, so it's not a real print function.  Intended for quick and dirty
 * uses, and for optimized string writes. */
void
vga_put_string(GtkWidget * widget, guchar * s, guchar attr, int col, int row)
{
	VGAText * vga;
	int ofs, i, len;
	GdkRectangle area;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TEXT(widget));
	g_return_if_fail(s != NULL);
	vga = VGA_TEXT(widget);

	len = strlen(s);
	if (len > (vga->pvt->cols - col))
		/* Truncate string */
		len = vga->pvt->cols - col;

	/* Update video buffer */
	ofs = vga->pvt->cols * row + col;
	for (i = 0; i < len; i++)
	{
		vga->pvt->video_buf[ofs].c = s[i];
		vga->pvt->video_buf[ofs++].attr = attr;
	}

	/* Refresh charcells */
	area.x = col * vga->pvt->font->width;
	area.y = row * vga->pvt->font->height;
	area.width = vga->pvt->font->width * len;
	area.height = vga->pvt->font->height;
	vga_refresh_area(widget, vga, &area);
}



/* Get a pointer to the screen internal video buffer */
guchar *
vga_get_video_buf(GtkWidget * widget)
{
	VGAText * vga;

	g_return_val_if_fail(widget != NULL, NULL);
	g_return_val_if_fail(VGA_IS_TEXT(widget), NULL);
	vga = VGA_TEXT(widget);

	return (guchar *) vga->pvt->video_buf;
}


int
vga_video_buf_size(GtkWidget * widget)
{
	VGAText * vga;

	g_return_val_if_fail(widget != NULL, -1);
	g_return_val_if_fail(VGA_IS_TEXT(widget), -1);
	vga = VGA_TEXT(widget);

	return vga->pvt->rows * vga->pvt->cols * sizeof(vga_charcell);
}

void
vga_video_buf_clear(GtkWidget * widget)
{
	memset(vga_get_video_buf(widget), 0, vga_video_buf_size(widget));
}


/* Show or hide the cursor */
void
vga_cursor_set_visible(GtkWidget * widget, gboolean visible)
{
	VGAText * vga;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TEXT(widget));
	vga = VGA_TEXT(widget);

	if(visible && !vga->pvt->cursor_visible){
	  // Restart the cursor blink timer
	  
	  /* Add our cursor timeout event to make it blink */
	  /* 229 ms is about how often the cursor blink is toggled in DOS */
	  vga->pvt->cursor_timeout_id = g_timeout_add(CURSOR_BLINK_PERIOD_MS,
						 vga_blink_cursor, vga);
	  
	}
	else if(!visible && vga->pvt->cursor_visible){
	  // Remove the cursor blink timer
	  if (vga->pvt->cursor_timeout_id != -1)
	    g_source_remove(vga->pvt->cursor_timeout_id);

	  // Refresh cell to remove the cursor
	  vga_refresh_region(widget, vga->pvt->cursor_x, vga->pvt->cursor_y, 1, 1);
	}

	vga->pvt->cursor_visible = visible;
}

gboolean
vga_cursor_is_visible(GtkWidget * widget)
{
	VGAText * vga;

	g_assert(widget != NULL);
	g_assert(VGA_IS_TEXT(widget));
	vga = VGA_TEXT(widget);

	return vga->pvt->cursor_visible;
}


/* Change the cursor position */
void
vga_cursor_move(GtkWidget * widget, int x, int y)
{
	VGAText * vga;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TEXT(widget));
	vga = VGA_TEXT(widget);

	if (vga->pvt->cursor_visible)
		vga_refresh_region(widget, vga->pvt->cursor_x,
				vga->pvt->cursor_y, 1, 1);

	vga->pvt->cursor_x = x;
	vga->pvt->cursor_y = y;

	if (vga->pvt->cursor_visible)
		vga_refresh_region(widget, x, y, 1, 1);
		
}

int
vga_cursor_x(GtkWidget * widget)
{
	VGAText * vga;

	g_return_val_if_fail(widget != NULL, -1);
	g_return_val_if_fail(VGA_IS_TEXT(widget), -1);
	vga = VGA_TEXT(widget);

	return vga->pvt->cursor_x;
}

int
vga_cursor_y(GtkWidget * widget)
{
	VGAText * vga;

	g_return_val_if_fail(widget != NULL, -1);
	g_return_val_if_fail(VGA_IS_TEXT(widget), -1);
	vga = VGA_TEXT(widget);

	return vga->pvt->cursor_y;
}


/* Refresh a square region of the screen to match the contents of the
 * video buffer.  */
void
vga_refresh_region(GtkWidget * widget,
			int top_left_x, int top_left_y,
			int cols, int rows)
{
	VGAText * vga;
	GdkRectangle area;

#ifdef VGA_DEBUG
	fprintf(stderr, "vga_refresh_region(%p, %d, %d, %d, %d)\n", widget, top_left_x, top_left_y, cols, rows);
#endif

	g_return_if_fail(widget != NULL);
	if (!GTK_WIDGET_REALIZED(widget))
		return;

	g_return_if_fail(VGA_IS_TEXT(widget));
	vga = VGA_TEXT(widget);

	area.x = top_left_x * vga->pvt->font->width;
	area.y = top_left_y * vga->pvt->font->height;
	area.width = cols * vga->pvt->font->width;
	area.height = rows * vga->pvt->font->height;
	vga_refresh_area(widget, vga, &area);
}
		
/* Refresh to screen display to match the contents of the buffer */
void
vga_refresh(GtkWidget * widget)
{
	VGAText * vga;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TEXT(widget));
	vga = VGA_TEXT(widget);

	vga_refresh_region(widget, 0, 0, vga->pvt->cols, vga->pvt->rows);
}

void vga_set_rows(GtkWidget * widget, int rows)
{
        VGAText * vga;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TEXT(widget));
	vga = VGA_TEXT(widget);

	if(rows == vga->pvt->rows){
	  return;
	}

	vga->pvt->rows = rows;
	vga_alloc_videobuf(vga);
}

void vga_set_cols(GtkWidget * widget, int cols)
{
        VGAText * vga;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TEXT(widget));
	vga = VGA_TEXT(widget);

	if(cols == vga->pvt->cols){
	  return;
	}

	vga->pvt->cols = cols;
	vga_alloc_videobuf(vga);
}

int vga_get_rows(GtkWidget * widget)
{
	VGAText * vga;

	g_return_val_if_fail(widget != NULL, -1);
	g_return_val_if_fail(VGA_IS_TEXT(widget), -1);
	vga = VGA_TEXT(widget);

	return vga->pvt->rows;
}

int vga_get_cols(GtkWidget * widget)
{
	VGAText * vga;

	g_return_val_if_fail(widget != NULL, -1);
	g_return_val_if_fail(VGA_IS_TEXT(widget), -1);
	vga = VGA_TEXT(widget);

	return vga->pvt->cols;
}

/**
 * memsetword:
 * @s: Pointer to the start of the area
 * @c: The word to fill the area with
 * @count: The size of the area
 *
 * Fill a memory area with a 16-byte word value
 */
static void * memsetword(void * s, gint16 c, size_t count)
{
	/* Simple portable way of doing it */
	gint16 * xs = (gint16 *) s;

	while (count--)
		*xs++ = c;

	return s;
}

void vga_clear_area(GtkWidget * widget, guchar attr, int top_left_x,
		int top_left_y, int cols, int rows)
{
	VGAText * vga;
	gint16 cellword;
	int ofs, y, endrow;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TEXT(widget));
	vga = VGA_TEXT(widget);
	/* FIXME: Endianness.  No << 8 for big endian */
	cellword = 0x0000 | ((gint16) attr << 8);
	/* Special case optimization */
	if (cols == vga->pvt->cols)
	{
		ofs = top_left_y * cols;
		memsetword(vga->pvt->video_buf + ofs, cellword, cols * rows);
	}
	else
	{
		endrow = top_left_y + rows;
		for (y = top_left_y; y < endrow; y++)
		{
			ofs = (y * vga->pvt->cols + top_left_x);
			memsetword(vga->pvt->video_buf + ofs, cellword, cols);
		}
	}
	vga_refresh_region(widget, top_left_x, top_left_y, cols, rows);
}

/* Clear screen / eol will be done in the terminal widget since it is
 * based on the screen 'textattr'. */

/* Delete a line, scrolling .. hmm question.. should we have the 'window'
 * concept in here?  i think the BIOS might have actually had something
 * like that but i doubt it now that i think about it. */
