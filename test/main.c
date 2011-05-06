#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdlib.h>
#include <stdio.h>

#include "vgatext.h"
#include "vgaterm.h"

/* Local Functions */
void on_vga_key_pressed(GtkWidget * widget, GdkEventKey * event);
void process_input_key(GdkEventKey * event);

typedef struct {
  int x, y;
  vga_charcell icon;
  vga_charcell bg;
} Avatar;

GtkWidget *vgatext, *vgaterm;
Avatar player;

/* --------------------------
   Callbacks
   -------------------------- */
void on_vga_key_pressed(GtkWidget * widget, GdkEventKey * event)
{
  process_input_key(event);
}	



/* --------------------------
   Gtk GUI Building Functions
   -------------------------- */
void createMainWindow(){
  GtkWidget *window, *vbox, *hbox, *term_win;
  GtkAdjustment *vadjust;

  VGAFont *font8x8;
  font8x8 = vga_font_new();
  vga_font_load_default_8x8(font8x8);
  

  /* Create new top level window */
  window = gtk_window_new( GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "VGA Widget Test");
  gtk_container_border_width(GTK_CONTAINER(window), 2);
  
  /* Quit from main if got delete event */
  gtk_signal_connect(GTK_OBJECT(window), "delete_event", GTK_SIGNAL_FUNC(gtk_main_quit), NULL);
  
  /* Create the widgets we need */
  hbox = gtk_hbox_new(FALSE, 0);
  vbox = gtk_vbox_new(FALSE, 0);
  term_win = gtk_scrolled_window_new (NULL, NULL);
  vadjust = gtk_scrolled_window_get_vadjustment((GtkScrolledWindow *)term_win);
  vgatext = vga_text_new(50, 80);
  //vga_set_font(vgatext, font8x8);
  vgaterm = vga_term_new(vadjust, 100);
  vga_term_emu_init(vgaterm);
  vga_video_buf_clear(vgaterm);
  vga_term_window(GTK_WIDGET(vgaterm), 1, 1, 80, 50);
  
  /* Setup vga terminal */
  gtk_box_pack_start(GTK_BOX(hbox), term_win, TRUE, FALSE, 0);
  gtk_widget_show(term_win);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (term_win), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(term_win), vgaterm);  
  gtk_widget_show(GTK_WIDGET(vgaterm));
  
  /* Setup vga text widget */
  gtk_box_pack_start(GTK_BOX(vbox), vgatext, TRUE, FALSE, 0);
  gtk_widget_show(GTK_WIDGET(vgatext));
  
  
  
  
  /* Add key press callback */
  gtk_signal_connect(GTK_OBJECT(window), "key_press_event", GTK_SIGNAL_FUNC(on_vga_key_pressed), NULL);
  
  gtk_container_add(GTK_CONTAINER(hbox), GTK_WIDGET(vbox));
  gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(hbox));
  gtk_widget_show(GTK_WIDGET(vbox));
  gtk_widget_show(GTK_WIDGET(hbox));
  gtk_widget_show(GTK_WIDGET(window));
}


/* --------------------------
   Functions
   -------------------------- */
void init_player(Avatar *player, int x, int y){
  player->x = x;
  player->y = y;
  player->icon.c = 2;
  player->icon.attr = 7;
  
}

void draw_player(GtkWidget * widget, Avatar *player){
  vga_charcell *bg;

  bg = vga_get_char(widget, player->x, player->y);
  player->bg.c = bg->c;
  player->bg.attr = bg->attr;

  vga_put_char(widget, player->icon.c, player->icon.attr, player->x, player->y);
}

void move_player(GtkWidget * widget, Avatar *player, int x, int y){

  vga_put_char(widget, player->bg.c, player->bg.attr, player->x, player->y);

  player->x = x;
  player->y = y;

  draw_player(widget, player);
}

void draw_test_pattern(GtkWidget * widget){
  guchar *buf;
  int i, pos;

  buf = vga_get_video_buf(widget);

  /* populate our buffer with the ASCII table */
  for (i = 0; i < 80*25; i++)
  {
    pos = i*2;
    buf[pos] = i % 256;
    //buf[pos+1] = i % 256;
    buf[pos+1] = ATTR(BLUE,BLACK);
  }

}

void terminal_dump_file(gchar * fname)
{
	FILE * f;
	guchar buf[4096];
	int n, i, line_count;

	f = fopen(fname, "rb");
	if (f == NULL)
		return;

	line_count = 0;
	n = fread(buf, 1, 4096, f);
	while (n > 0)
	{
	  for (i = 0; i < n; i++)
	  {
	    vga_term_emu_writec(vgaterm, buf[i]);
	    if( (buf[i] == 10) && (buf[i-1] == 13))
	    {
	      line_count++;
	    }
	  }
	  
	  line_count = 0;
	  n = fread(buf, 1, 4096, f);
	}
	fclose(f);
}

void process_input_key(GdkEventKey * event)
{
  GdkModifierType modifiers;
  gboolean modifier = FALSE;
  int x, y;
			  

	

  if (event->type == GDK_KEY_PRESS)
    {
      /* Read the modifiers */
      if (gdk_event_get_state((GdkEvent *)event, 
			      &modifiers) == FALSE)
	{
	  modifiers = 0;
	}

      g_debug("Modifier state = %x", event->state);

      switch (event->keyval)
	{	/*
		  case GDK_Alt_L:
		  case GDK_Alt_R:
		  case GDK_Caps_Lock:
		  case GDK_Control_L:
		  case GDK_Control_R:
		  case GDK_Eisu_Shift:
		  case GDK_Hyper_L:
		  case GDK_Hyper_R:
		  case GDK_ISO_First_Group_Lock:
		  case GDK_ISO_Group_Lock:
		  case GDK_ISO_Group_Shift:
		  case GDK_ISO_Last_Group_Lock:
		  case GDK_ISO_Level3_Lock:
		  case GDK_ISO_Level3_Shift:
		  case GDK_ISO_Lock:
		  case GDK_ISO_Next_Group_Lock:
		  case GDK_ISO_Prev_Group_Lock:
		  case GDK_Kana_Lock:
		  case GDK_Kana_Shift:
		  case GDK_Meta_L:
		  case GDK_Meta_R:
		  case GDK_Num_Lock:
		  case GDK_Scroll_Lock:
		  case GDK_Shift_L:
		  case GDK_Shift_Lock:
		  case GDK_Shift_R:
		  case GDK_Super_L:
		  case GDK_Super_R:
		  modifier = TRUE;
		  g_debug("Modifier press, keyval=%d", event->keyval);
		  break;
		*/
	case GDK_Up:
	  x = vga_cursor_x(GTK_WIDGET(vgatext));
	  y = vga_cursor_y(GTK_WIDGET(vgatext));

	  if(y != 0){
	    vga_cursor_move(GTK_WIDGET(vgatext), x, y-1);
	    move_player(GTK_WIDGET(vgatext), &player, x, y-1);
	  }
	  break;
	case GDK_Down:
	  x = vga_cursor_x(GTK_WIDGET(vgatext));
	  y = vga_cursor_y(GTK_WIDGET(vgatext));

	  if(y < (vga_get_rows(GTK_WIDGET(vgatext)) - 1)){
	    vga_cursor_move(GTK_WIDGET(vgatext), x, y+1);
	    move_player(GTK_WIDGET(vgatext), &player, x, y+1);
	  }
	  break;
	case GDK_Left:
	  x = vga_cursor_x(GTK_WIDGET(vgatext));
	  y = vga_cursor_y(GTK_WIDGET(vgatext));
			  
	  if(x != 0){
	    vga_cursor_move(GTK_WIDGET(vgatext), x-1, y);
	    move_player(GTK_WIDGET(vgatext), &player, x-1, y);
	  }
	  break;
	case GDK_Right:
	  x = vga_cursor_x(GTK_WIDGET(vgatext));
	  y = vga_cursor_y(GTK_WIDGET(vgatext));
			  
	  if(x < (vga_get_cols(GTK_WIDGET(vgatext)) - 1)){
	    vga_cursor_move(GTK_WIDGET(vgatext), x+1, y);
	    move_player(GTK_WIDGET(vgatext), &player, x+1, y);
	  }
	  break;
	case GDK_Return:
	  /* Assume binary mode - send CR */

	  break;
	case GDK_BackSpace:
	  vga_cursor_set_visible(GTK_WIDGET(vgatext), !vga_cursor_is_visible(GTK_WIDGET(vgatext)));
	  break;
	case GDK_Delete:

	  break;
	case GDK_Home:

	  break;
	case GDK_End:

	  break;
	case GDK_Insert:

	  break;
	default:
	  modifier = FALSE;
	  g_debug("Key press: (keyval=%d): %s",
		  event->keyval, event->string);
				

	  break;
	}
    }
}


int main(int argc, char **argv){

  gtk_init(&argc, &argv);

  createMainWindow();

  vga_cursor_move(GTK_WIDGET(vgatext), 0, 25);
  vga_cursor_set_visible(GTK_WIDGET(vgatext), FALSE);
  
  init_player(&player, 0, 25);
  draw_player(GTK_WIDGET(vgatext), &player);
  draw_test_pattern(GTK_WIDGET(vgatext));
  //terminal_dump_file("IC-CCI.ICE");

  vga_term_emu_print(GTK_WIDGET(vgaterm), "ANSI Emulation Test\n");
  vga_term_emu_print(GTK_WIDGET(vgaterm), "\033[0;34mBlue\n");
  vga_term_emu_print(GTK_WIDGET(vgaterm), "\033[0;32mGreen\n");
  vga_term_emu_print(GTK_WIDGET(vgaterm), "\033[0;36mCyan\n");
  vga_term_emu_print(GTK_WIDGET(vgaterm), "\033[0;31mRed\n");
  vga_term_emu_print(GTK_WIDGET(vgaterm), "\033[0;35mPurple\n");
  vga_term_emu_print(GTK_WIDGET(vgaterm), "\033[0;33mBrown\n");
  vga_term_emu_print(GTK_WIDGET(vgaterm), "\033[0;37mGrey\n");
  vga_term_emu_print(GTK_WIDGET(vgaterm), "\033[1;30mDark Grey\n");
  vga_term_emu_print(GTK_WIDGET(vgaterm), "\033[1;34mLight Blue\n");
  vga_term_emu_print(GTK_WIDGET(vgaterm), "\033[1;32mLight Green\n");
  vga_term_emu_print(GTK_WIDGET(vgaterm), "\033[1;36mLight Cyan\n");
  vga_term_emu_print(GTK_WIDGET(vgaterm), "\033[1;31mLight Red\n");
  vga_term_emu_print(GTK_WIDGET(vgaterm), "\033[1;35mLight Purple\n");
  vga_term_emu_print(GTK_WIDGET(vgaterm), "\033[1;33mYellow\n");
  vga_term_emu_print(GTK_WIDGET(vgaterm), "\033[1;37mWhite\n");
  vga_term_emu_print(GTK_WIDGET(vgaterm), "\033[0;30;44mBlack\n");
  vga_term_emu_print(GTK_WIDGET(vgaterm), "\033[4;32;40mUnderline\n");
  vga_term_emu_print(GTK_WIDGET(vgaterm), "\033[5;32;mBlink\n");
  vga_term_emu_print(GTK_WIDGET(vgaterm), "\033[7;32mReverse\n");
  vga_term_emu_print(GTK_WIDGET(vgaterm), "\033[8;32mInvisible\n");
  vga_term_emu_print(GTK_WIDGET(vgaterm), "\033[0;37mCursor moved up two lines\033[2A");

  gtk_main();

  return 0;
}


