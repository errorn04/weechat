/*
 * Copyright (c) 2003-2008 by FlashCode <flashcode@flashtux.org>
 * See README for License detail, AUTHORS for developers list.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* gui-curses-window.c: window display functions for Curses GUI */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "../../core/weechat.h"
#include "../../core/wee-config.h"
#include "../../core/wee-log.h"
#include "../../core/wee-string.h"
#include "../gui-window.h"
#include "../gui-bar.h"
#include "../gui-buffer.h"
#include "../gui-chat.h"
#include "../gui-color.h"
#include "../gui-hotlist.h"
#include "../gui-infobar.h"
#include "../gui-input.h"
#include "../gui-main.h"
#include "../gui-nicklist.h"
#include "../gui-status.h"
#include "gui-curses.h"


/*
 * gui_window_get_width: get screen width (terminal width in chars for Curses)
 */

int
gui_window_get_width ()
{
    return COLS;
}

/*
 * gui_window_get_height: get screen height (terminal height in chars for Curses)
 */

int
gui_window_get_height ()
{
    return LINES;
}

/*
 * gui_window_objects_init: init Curses windows
 */

int
gui_window_objects_init (struct t_gui_window *window)
{
    struct t_gui_curses_objects *new_objects;

    if ((new_objects = (struct t_gui_curses_objects *)malloc (sizeof (struct t_gui_curses_objects))))
    {
        window->gui_objects = new_objects;
        GUI_CURSES(window)->win_title = NULL;
        GUI_CURSES(window)->win_chat = NULL;
        GUI_CURSES(window)->win_nick = NULL;
        GUI_CURSES(window)->win_status = NULL;
        GUI_CURSES(window)->win_infobar = NULL;
        GUI_CURSES(window)->win_input = NULL;
        GUI_CURSES(window)->win_separator = NULL;
        GUI_CURSES(window)->bar_windows = NULL;
        return 1;
    }
    else
        return 0;
}

/*
 * gui_window_objects_free: free Curses windows for a window
 */

void
gui_window_objects_free (struct t_gui_window *window, int free_separator)
{
    if (GUI_CURSES(window)->win_title)
    {
        delwin (GUI_CURSES(window)->win_title);
        GUI_CURSES(window)->win_title = NULL;
    }
    if (GUI_CURSES(window)->win_chat)
    {
        delwin (GUI_CURSES(window)->win_chat);
        GUI_CURSES(window)->win_chat = NULL;
    }
    if (GUI_CURSES(window)->win_nick)
    {
        delwin (GUI_CURSES(window)->win_nick);
        GUI_CURSES(window)->win_nick = NULL;
    }
    if (GUI_CURSES(window)->win_status)
    {
        delwin (GUI_CURSES(window)->win_status);
        GUI_CURSES(window)->win_status = NULL;
    }
    if (GUI_CURSES(window)->win_infobar)
    {
        delwin (GUI_CURSES(window)->win_infobar);
        GUI_CURSES(window)->win_infobar = NULL;
    }
    if (GUI_CURSES(window)->win_input)
    {
        delwin (GUI_CURSES(window)->win_input);
        GUI_CURSES(window)->win_input = NULL;
    }
    if (free_separator && GUI_CURSES(window)->win_separator)
    {
        delwin (GUI_CURSES(window)->win_separator);
        GUI_CURSES(window)->win_separator = NULL;
    }
}

/*
 * gui_window_utf_char_valid: return 1 if utf char is valid for screen
 *                            otherwise return 0
 */

int
gui_window_utf_char_valid (char *utf_char)
{
    /* 146 or 0x7F are not valid */
    if ((((unsigned char)(utf_char[0]) == 146)
         || ((unsigned char)(utf_char[0]) == 0x7F))
        && (!utf_char[1]))
        return 0;
    
    /* any other char is valid */
    return 1;
}

/*
 * gui_window_wprintw: decode then display string with wprintw
 */

void
gui_window_wprintw (WINDOW *window, char *data, ...)
{
    va_list argptr;
    static char buf[4096];
    char *buf2;
    
    va_start (argptr, data);
    vsnprintf (buf, sizeof (buf) - 1, data, argptr);
    va_end (argptr);
    
    buf2 = string_iconv_from_internal (NULL, buf);
    wprintw (window, "%s", (buf2) ? buf2 : buf);
    if (buf2)
        free (buf2);
}

/*
 * gui_window_curses_clear: clear a Curses window
 */

void
gui_window_curses_clear (WINDOW *window, int num_color)
{
    if (!gui_ok)
        return;
    
    wbkgdset(window, ' ' | COLOR_PAIR (gui_color_get_pair (num_color)));
    werase (window);
    wmove (window, 0, 0);
}

/*
 * gui_window_set_weechat_color: set WeeChat color for window
 */

void
gui_window_set_weechat_color (WINDOW *window, int num_color)
{
    if ((num_color >= 0) && (num_color <= GUI_NUM_COLORS - 1))
    {
        wattroff (window, A_BOLD | A_UNDERLINE | A_REVERSE);
        wattron (window, COLOR_PAIR(gui_color_get_pair (num_color)) |
                 gui_color[num_color]->attributes);
    }
}

/*
 * gui_window_calculate_pos_size: calculate position and size for a buffer & subwindows
 *                                return 1 if pos/size changed, 0 if no change
 */

int
gui_window_calculate_pos_size (struct t_gui_window *window, int force_calculate)
{
    int max_length, max_height, lines, width_used;
    int add_top, add_bottom, add_left, add_right;
    
    if (!gui_ok)
        return 0;
    
    add_bottom = gui_bar_window_get_size (NULL, window, GUI_BAR_POSITION_BOTTOM);
    add_top = gui_bar_window_get_size (NULL, window, GUI_BAR_POSITION_TOP);
    add_left = gui_bar_window_get_size (NULL, window, GUI_BAR_POSITION_LEFT);
    add_right = gui_bar_window_get_size (NULL, window, GUI_BAR_POSITION_RIGHT);
    
    /* init chat & nicklist settings */
    if (window->buffer->nicklist)
    {
        max_length = window->buffer->nicklist_max_length;
        
        lines = 0;
        
        if ((CONFIG_INTEGER(config_look_nicklist_position) == CONFIG_LOOK_NICKLIST_LEFT) ||
            (CONFIG_INTEGER(config_look_nicklist_position) == CONFIG_LOOK_NICKLIST_RIGHT))
        {
            if ((CONFIG_INTEGER(config_look_nicklist_min_size) > 0)
                && (max_length < CONFIG_INTEGER(config_look_nicklist_min_size)))
                max_length = CONFIG_INTEGER(config_look_nicklist_min_size);
            else if ((CONFIG_INTEGER(config_look_nicklist_max_size) > 0)
                     && (max_length > CONFIG_INTEGER(config_look_nicklist_max_size)))
                max_length = CONFIG_INTEGER(config_look_nicklist_max_size);
            if (!force_calculate
                && (window->win_nick_width ==
                    max_length + ((CONFIG_BOOLEAN(config_look_nicklist_separator)) ? 1 : 0)))
                return 0;
        }
        else
        {
            width_used = (window->win_width - add_left - add_right)
                - ((window->win_width - add_left - add_right) % (max_length + 2));
            if (((max_length + 2) * window->buffer->nicklist_visible_count) % width_used == 0)
                lines = ((max_length + 2) * window->buffer->nicklist_visible_count) / width_used;
            else
                lines = (((max_length + 2) * window->buffer->nicklist_visible_count) / width_used) + 1;
            if ((CONFIG_INTEGER(config_look_nicklist_max_size) > 0)
                && (lines > CONFIG_INTEGER(config_look_nicklist_max_size)))
                lines = CONFIG_INTEGER(config_look_nicklist_max_size);
            if ((CONFIG_INTEGER(config_look_nicklist_min_size) > 0)
                && (lines < CONFIG_INTEGER(config_look_nicklist_min_size)))
                lines = CONFIG_INTEGER(config_look_nicklist_min_size);
            max_height = (CONFIG_BOOLEAN(config_look_infobar)) ?
                window->win_height - add_top - add_bottom - 3 - 4 :
                window->win_height - add_top - add_bottom - 2 - 4;
            if (lines > max_height)
                lines = max_height;
            if (!force_calculate
                && (window->win_nick_height ==
                    lines + ((CONFIG_BOOLEAN(config_look_nicklist_separator)) ? 1 : 0)))
                return 0;
        }
        
        switch (CONFIG_INTEGER(config_look_nicklist_position))
        {
            case CONFIG_LOOK_NICKLIST_LEFT:
                window->win_chat_x = window->win_x + add_left + max_length +
                    ((CONFIG_BOOLEAN(config_look_nicklist_separator)) ? 1 : 0);
                window->win_chat_y = window->win_y + add_top + 1;
                window->win_chat_width = window->win_width - max_length -
                    ((CONFIG_BOOLEAN(config_look_nicklist_separator)) ? 1 : 0);
                window->win_nick_x = window->win_x + add_left + 0;
                window->win_nick_y = window->win_y + add_top + 1;
                window->win_nick_width = max_length +
                    ((CONFIG_BOOLEAN(config_look_nicklist_separator)) ? 1 : 0);
                if (CONFIG_BOOLEAN(config_look_infobar))
                {
                    window->win_chat_height = window->win_height - add_top - add_bottom - 4;
                    window->win_nick_height = window->win_height - add_top - add_bottom - 4;
                }
                else
                {
                    window->win_chat_height = window->win_height - add_top - add_bottom - 3;
                    window->win_nick_height = window->win_height - add_top - add_bottom - 3;
                }
                window->win_nick_num_max = window->win_nick_height;
                break;
            case CONFIG_LOOK_NICKLIST_RIGHT:
                window->win_chat_x = window->win_x + add_left;
                window->win_chat_y = window->win_y + add_top + 1;
                window->win_chat_width = window->win_width - add_left - add_right - max_length -
                    ((CONFIG_BOOLEAN(config_look_nicklist_separator)) ? 1 : 0);
                window->win_nick_x = window->win_x + window->win_width - add_right - max_length -
                    ((CONFIG_BOOLEAN(config_look_nicklist_separator)) ? 1 : 0);
                window->win_nick_y = window->win_y + add_top + 1;
                window->win_nick_width = max_length +
                    ((CONFIG_BOOLEAN(config_look_nicklist_separator)) ? 1 : 0);
                if (CONFIG_BOOLEAN(config_look_infobar))
                {
                    window->win_chat_height = window->win_height - add_top - add_bottom - 4;
                    window->win_nick_height = window->win_height - add_top - add_bottom - 4;
                }
                else
                {
                    window->win_chat_height = window->win_height - add_top - add_bottom - 3;
                    window->win_nick_height = window->win_height - add_top - add_bottom - 3;
                }
                window->win_nick_num_max = window->win_nick_height;
                break;
            case CONFIG_LOOK_NICKLIST_TOP:
                window->win_chat_x = window->win_x + add_left;
                window->win_chat_y = window->win_y + add_top + 1 + lines +
                    ((CONFIG_BOOLEAN(config_look_nicklist_separator)) ? 1 : 0);
                window->win_chat_width = window->win_width - add_left - add_right;
                if (CONFIG_BOOLEAN(config_look_infobar))
                    window->win_chat_height = window->win_height - add_top - add_bottom - 3 - lines -
                        ((CONFIG_BOOLEAN(config_look_nicklist_separator)) ? 1 : 0) - 1;
                else
                    window->win_chat_height = window->win_height - add_top - add_bottom - 3 - lines -
                        ((CONFIG_BOOLEAN(config_look_nicklist_separator)) ? 1 : 0);
                window->win_nick_x = window->win_x + add_left;
                window->win_nick_y = window->win_y + add_top + 1;
                window->win_nick_width = window->win_width - add_left - add_right;
                window->win_nick_height = lines +
                    ((CONFIG_BOOLEAN(config_look_nicklist_separator)) ? 1 : 0);
                window->win_nick_num_max = lines * (window->win_nick_width / (max_length + 1));
                break;
            case CONFIG_LOOK_NICKLIST_BOTTOM:
                window->win_chat_x = window->win_x + add_left;
                window->win_chat_y = window->win_y + add_top + 1;
                window->win_chat_width = window->win_width - add_left - add_right;
                if (CONFIG_BOOLEAN(config_look_infobar))
                    window->win_chat_height = window->win_height - add_top - add_bottom - 3 - lines -
                        ((CONFIG_BOOLEAN(config_look_nicklist_separator)) ? 1 : 0) - 1;
                else
                    window->win_chat_height = window->win_height - add_top - add_bottom - 3 - lines -
                        ((CONFIG_BOOLEAN(config_look_nicklist_separator)) ? 1 : 0);
                window->win_nick_x = window->win_x;
                if (CONFIG_BOOLEAN(config_look_infobar))
                    window->win_nick_y = window->win_y + window->win_height - add_bottom -
                        2 - lines -
                        ((CONFIG_BOOLEAN(config_look_nicklist_separator)) ? 1 : 0) - 1;
                else
                    window->win_nick_y = window->win_y + window->win_height - add_bottom -
                        2 - lines -
                        ((CONFIG_BOOLEAN(config_look_nicklist_separator)) ? 1 : 0);
                window->win_nick_width = window->win_width;
                window->win_nick_height = lines +
                    ((CONFIG_BOOLEAN(config_look_nicklist_separator)) ? 1 : 0);
                window->win_nick_num_max = lines * (window->win_nick_width / (max_length + 1));
                break;
        }
        
        window->win_chat_cursor_x = window->win_x + add_left;
        window->win_chat_cursor_y = window->win_y + add_top;
    }
    else
    {
        window->win_chat_x = window->win_x + add_left;
        window->win_chat_y = window->win_y + add_top + 1;
        window->win_chat_width = window->win_width - add_left - add_right;
        if (CONFIG_BOOLEAN(config_look_infobar))
            window->win_chat_height = window->win_height - add_top - add_bottom - 4;
        else
            window->win_chat_height = window->win_height - add_top - add_bottom - 3;
        window->win_chat_cursor_x = window->win_x + add_left;
        window->win_chat_cursor_y = window->win_y + add_top;
        window->win_nick_x = -1;
        window->win_nick_y = -1;
        window->win_nick_width = -1;
        window->win_nick_height = -1;
        window->win_nick_num_max = -1;
    }

    /* title window */
    window->win_title_x = window->win_x + add_left;
    window->win_title_y = window->win_y + add_top;
    window->win_title_width = window->win_width - add_left - add_right;
    window->win_title_height = 1;
    
    /* status window */
    window->win_status_x = window->win_x + add_left;
    if (CONFIG_BOOLEAN(config_look_infobar))
        window->win_status_y = window->win_y + window->win_height - add_bottom - 3;
    else
        window->win_status_y = window->win_y + window->win_height - add_bottom - 2;
    window->win_status_width = window->win_width - add_left - add_right;
    window->win_status_height = 1;
    
    /* infobar window */
    if (CONFIG_BOOLEAN(config_look_infobar))
    {
        window->win_infobar_x = window->win_x + add_left;
        window->win_infobar_y = window->win_y + window->win_height - add_bottom - 2;
        window->win_infobar_width = window->win_width - add_left - add_right;
        window->win_infobar_height = 1;
    }
    else
    {
        window->win_infobar_x = -1;
        window->win_infobar_y = -1;
        window->win_infobar_width = -1;
        window->win_infobar_height = -1;
    }

    /* input window */
    window->win_input_x = window->win_x + add_left;
    window->win_input_y = window->win_y + window->win_height - add_bottom - 1;
    window->win_input_width = window->win_width - add_left - add_right;
    window->win_input_height = 1;
    
    return 1;
}

/*
 * gui_window_draw_separator: draw window separation
 */

void
gui_window_draw_separator (struct t_gui_window *window)
{
    if (GUI_CURSES(window)->win_separator)
        delwin (GUI_CURSES(window)->win_separator);
    
    if (window->win_x > gui_bar_root_get_size (NULL, GUI_BAR_POSITION_LEFT))
    {
        GUI_CURSES(window)->win_separator = newwin (window->win_height,
                                                    1,
                                                    window->win_y,
                                                    window->win_x - 1);
        gui_window_set_weechat_color (GUI_CURSES(window)->win_separator,
                                      GUI_COLOR_SEPARATOR);
        mvwvline (GUI_CURSES(window)->win_separator, 0, 0, ACS_VLINE,
                  window->win_height);
        wnoutrefresh (GUI_CURSES(window)->win_separator);
        refresh ();
    }
}

/*
 * gui_window_redraw_buffer: redraw a buffer
 */

void
gui_window_redraw_buffer (struct t_gui_buffer *buffer)
{
    if (!gui_ok)
        return;
    
    gui_chat_draw_title (buffer, 1);
    gui_chat_draw (buffer, 1);
    if (buffer->nicklist)
        gui_nicklist_draw (buffer, 1);
    gui_status_draw (1);
    if (CONFIG_BOOLEAN(config_look_infobar))
        gui_infobar_draw (buffer, 1);
    gui_input_draw (buffer, 1);
}

/*
 * gui_window_redraw_all_buffers: redraw all buffers
 */

void
gui_window_redraw_all_buffers ()
{
    struct t_gui_buffer *ptr_buffer;

    if (!gui_ok)
        return;
    
    for (ptr_buffer = gui_buffers; ptr_buffer;
         ptr_buffer = ptr_buffer->next_buffer)
    {
        gui_window_redraw_buffer (ptr_buffer);
    }
}

/*
 * gui_window_switch_to_buffer: switch to another buffer
 */

void
gui_window_switch_to_buffer (struct t_gui_window *window,
                             struct t_gui_buffer *buffer)
{
    if (!gui_ok)
        return;

    if (window->buffer->num_displayed > 0)
        window->buffer->num_displayed--;
    
    if (window->buffer != buffer)
    {
        window->buffer->last_read_line = window->buffer->last_line;
        if (buffer->last_read_line == buffer->last_line)
            buffer->last_read_line = NULL;
        gui_previous_buffer = window->buffer;
    }
    
    window->buffer = buffer;
    window->win_title_start = 0;
    window->win_nick_start = 0;
    
    gui_window_calculate_pos_size (window, 1);
    
    /* destroy Curses windows */
    gui_window_objects_free (window, 0);
    
    /* create Curses windows */
    GUI_CURSES(window)->win_title = newwin (window->win_title_height,
                                            window->win_title_width,
                                            window->win_title_y,
                                            window->win_title_x);
    GUI_CURSES(window)->win_input = newwin (window->win_input_height,
                                            window->win_input_width,
                                            window->win_input_y,
                                            window->win_input_x);
    if (buffer->nicklist)
    {
        if (GUI_CURSES(window)->win_chat)
            delwin (GUI_CURSES(window)->win_chat);
        GUI_CURSES(window)->win_chat = newwin (window->win_chat_height,
                                               window->win_chat_width,
                                               window->win_chat_y,
                                               window->win_chat_x);
        if (CONFIG_BOOLEAN(config_look_nicklist))
            GUI_CURSES(window)->win_nick = newwin (window->win_nick_height,
                                                   window->win_nick_width,
                                                   window->win_nick_y,
                                                   window->win_nick_x);
        else
            GUI_CURSES(window)->win_nick = NULL;
    }
    else
    {
        if (GUI_CURSES(window)->win_chat)
            delwin (GUI_CURSES(window)->win_chat);
        GUI_CURSES(window)->win_chat = newwin (window->win_chat_height,
                                               window->win_chat_width,
                                               window->win_chat_y,
                                               window->win_chat_x);
    }
    
    /* create status/infobar windows */
    if (CONFIG_BOOLEAN(config_look_infobar))
        GUI_CURSES(window)->win_infobar = newwin (window->win_infobar_height,
                                                  window->win_infobar_width,
                                                  window->win_infobar_y,
                                                  window->win_infobar_x);
    
    GUI_CURSES(window)->win_status = newwin (window->win_status_height,
                                             window->win_status_width,
                                             window->win_status_y,
                                             window->win_status_x);
    
    window->start_line = NULL;
    window->start_line_pos = 0;

    buffer->num_displayed++;
    
    gui_hotlist_remove_buffer (buffer);
}

/*
 * gui_window_page_up: display previous page on buffer
 */

void
gui_window_page_up (struct t_gui_window *window)
{
    if (!gui_ok)
        return;
    
    if (!window->first_line_displayed)
    {
        gui_chat_calculate_line_diff (window, &window->start_line,
                                      &window->start_line_pos,
                                      (window->start_line) ?
                                      (-1) * (window->win_chat_height - 1) :
                                      (-1) * ((window->win_chat_height - 1) * 2));
        gui_chat_draw (window->buffer, 0);
        if (!window->scroll)
        {
            window->start_line = NULL;
            window->start_line_pos = 0;
        }
        gui_status_refresh_needed = 1;
    }
}

/*
 * gui_window_page_down: display next page on buffer
 */

void
gui_window_page_down (struct t_gui_window *window)
{
    struct t_gui_line *ptr_line;
    int line_pos;
    
    if (!gui_ok)
        return;
    
    if (window->start_line)
    {
        gui_chat_calculate_line_diff (window, &window->start_line,
                                      &window->start_line_pos,
                                      window->win_chat_height - 1);
        
        /* check if we can display all */
        ptr_line = window->start_line;
        line_pos = window->start_line_pos;
        gui_chat_calculate_line_diff (window, &ptr_line,
                                      &line_pos,
                                      window->win_chat_height - 1);
        if (!ptr_line)
        {
            window->start_line = NULL;
            window->start_line_pos = 0;
        }
        
        gui_chat_draw (window->buffer, 0);
        if (!window->scroll)
        {
            window->start_line = NULL;
            window->start_line_pos = 0;
            gui_hotlist_remove_buffer (window->buffer);
        }
        gui_status_refresh_needed = 1;
    }
}

/*
 * gui_window_scroll_up: display previous few lines in buffer
 */

void
gui_window_scroll_up (struct t_gui_window *window)
{
    if (!gui_ok)
        return;
    
    if (!window->first_line_displayed)
    {
        gui_chat_calculate_line_diff (window, &window->start_line,
                                      &window->start_line_pos,
                                      (window->start_line) ?
                                      (-1) * CONFIG_INTEGER(config_look_scroll_amount) :
                                      (-1) * ( (window->win_chat_height - 1) +
                                               CONFIG_INTEGER(config_look_scroll_amount)));
        gui_chat_draw (window->buffer, 0);
        if (!window->scroll)
        {
            window->start_line = NULL;
            window->start_line_pos = 0;
        }
        gui_status_refresh_needed = 1;
    }
}

/*
 * gui_window_scroll_down: display next few lines in buffer
 */

void
gui_window_scroll_down (struct t_gui_window *window)
{
    struct t_gui_line *ptr_line;
    int line_pos;
    
    if (!gui_ok)
        return;
    
    if (window->start_line)
    {
        gui_chat_calculate_line_diff (window, &window->start_line,
                                      &window->start_line_pos,
                                      CONFIG_INTEGER(config_look_scroll_amount));
        
        /* check if we can display all */
        ptr_line = window->start_line;
        line_pos = window->start_line_pos;
        gui_chat_calculate_line_diff (window, &ptr_line,
                                      &line_pos,
                                      window->win_chat_height - 1);
        
        if (!ptr_line)
        {
            window->start_line = NULL;
            window->start_line_pos = 0;
        }
        
        gui_chat_draw (window->buffer, 0);
        if (!window->scroll)
        {
            window->start_line = NULL;
            window->start_line_pos = 0;
            gui_hotlist_remove_buffer (window->buffer);
        }
        gui_status_refresh_needed = 1;
    }
}

/*
 * gui_window_scroll_top: scroll to top of buffer
 */

void
gui_window_scroll_top (struct t_gui_window *window)
{
    if (!gui_ok)
        return;
    
    if (!window->first_line_displayed)
    {
        window->start_line = window->buffer->lines;
        window->start_line_pos = 0;
        gui_chat_draw (window->buffer, 0);
        if (!window->scroll)
        {
            window->start_line = NULL;
            window->start_line_pos = 0;
        }
        gui_status_refresh_needed = 1;
    }
}

/*
 * gui_window_scroll_bottom: scroll to bottom of buffer
 */

void
gui_window_scroll_bottom (struct t_gui_window *window)
{
    if (!gui_ok)
        return;
    
    if (window->start_line)
    {
        window->start_line = NULL;
        window->start_line_pos = 0;
        gui_chat_draw (window->buffer, 0);
        if (!window->scroll)
        {
            window->start_line = NULL;
            window->start_line_pos = 0;
            gui_hotlist_remove_buffer (window->buffer);
        }
        gui_status_refresh_needed = 1;
    }
}

/*
 * gui_window_scroll_topic_left: scroll left topic
 */

void
gui_window_scroll_topic_left (struct t_gui_window *window)
{
    if (!gui_ok)
        return;
    
    if (window->win_title_start > 0)
        window->win_title_start -= (window->win_width * 3) / 4;
    if (window->win_title_start < 0)
        window->win_title_start = 0;
    window->buffer->title_refresh_needed = 1;
}

/*
 * gui_window_scroll_topic_right: scroll right topic
 */

void
gui_window_scroll_topic_right (struct t_gui_window *window)
{
    if (!gui_ok)
        return;
    
    window->win_title_start += (window->win_width * 3) / 4;
    window->buffer->title_refresh_needed = 1;
}

/*
 * gui_window_nick_beginning: go to beginning of nicklist
 */

void
gui_window_nick_beginning (struct t_gui_window *window)
{
    if (!gui_ok)
        return;
    
    if (window->buffer->nicklist)
    {
        if (window->win_nick_start > 0)
        {
            window->win_nick_start = 0;
            window->buffer->nicklist_refresh_needed = 1;
        }
    }
}

/*
 * gui_window_nick_end: go to the end of nicklist
 */

void
gui_window_nick_end (struct t_gui_window *window)
{
    int new_start;
    
    if (!gui_ok)
        return;
    
    if (window->buffer->nicklist)
    {
        new_start =
            window->buffer->nicklist_visible_count - window->win_nick_num_max;
        if (new_start < 0)
            new_start = 0;
        else if (new_start >= 1)
            new_start++;
        
        if (new_start != window->win_nick_start)
        {
            window->win_nick_start = new_start;
            window->buffer->nicklist_refresh_needed = 1;
        }
    }
}

/*
 * gui_window_nick_page_up: scroll one page up in nicklist
 */

void
gui_window_nick_page_up (struct t_gui_window *window)
{
    if (!gui_ok)
        return;
    
    if (window->buffer->nicklist)
    {
        if (window->win_nick_start > 0)
        {
            window->win_nick_start -= (window->win_nick_num_max - 1);
            if (window->win_nick_start <= 1)
                window->win_nick_start = 0;
            window->buffer->nicklist_refresh_needed = 1;
        }
    }
}

/*
 * gui_window_nick_page_down: scroll one page down in nicklist
 */

void
gui_window_nick_page_down (struct t_gui_window *window)
{
    if (!gui_ok)
        return;
    
    if (window->buffer->nicklist)
    {
        if ((window->buffer->nicklist_visible_count > window->win_nick_num_max)
            && (window->win_nick_start + window->win_nick_num_max - 1
                < window->buffer->nicklist_visible_count))
        {
            if (window->win_nick_start == 0)
                window->win_nick_start += (window->win_nick_num_max - 1);
            else
                window->win_nick_start += (window->win_nick_num_max - 2);
            window->buffer->nicklist_refresh_needed = 1;
        }
    }
}

/*
 * gui_window_auto_resize: auto-resize all windows, according to % of global size
 *                         This function is called after a terminal resize.
 *                         Returns 0 if ok, -1 if all window should be merged
 *                         (not enough space according to windows %)
 */

int
gui_window_auto_resize (struct t_gui_window_tree *tree,
                        int x, int y, int width, int height,
                        int simulate)
{
    int size1, size2;
    
    if (!gui_ok)
        return 0;
    
    if (tree)
    {
        if (tree->window)
        {
            if ((width < WINDOW_MIN_WIDTH) || (height < WINDOW_MIN_HEIGHT))
                return -1;
            if (!simulate)
            {
                tree->window->win_x = x;
                tree->window->win_y = y;
                tree->window->win_width = width;
                tree->window->win_height = height;
            }
        }
        else
        {
            if (tree->split_horiz)
            {
                size1 = (height * tree->split_pct) / 100;
                size2 = height - size1;
                if (gui_window_auto_resize (tree->child1, x, y + size1,
                                            width, size2, simulate) < 0)
                    return -1;
                if (gui_window_auto_resize (tree->child2, x, y,
                                            width, size1, simulate) < 0)
                    return -1;
            }
            else
            {
                size1 = (width * tree->split_pct) / 100;
                size2 = width - size1 - 1;
                if (gui_window_auto_resize (tree->child1, x, y,
                                            size1, height, simulate) < 0)
                    return -1;
                if (gui_window_auto_resize (tree->child2, x + size1 + 1, y,
                                            size2, height, simulate) < 0)
                    return -1;
            }
        }
    }
    return 0;
}

/*
 * gui_window_refresh_windows: auto resize and refresh all windows
 */

void
gui_window_refresh_windows ()
{
    struct t_gui_window *ptr_win, *old_current_window;
    struct t_gui_buffer *ptr_buffer;
    struct t_gui_bar *ptr_bar;
    int add_bottom, add_top, add_left, add_right;
    
    if (!gui_ok)
        return;
    
    old_current_window = gui_current_window;

    for (ptr_bar = gui_bars; ptr_bar; ptr_bar = ptr_bar->next_bar)
    {
        if (ptr_bar->type == GUI_BAR_TYPE_ROOT)
        {
            gui_bar_window_calculate_pos_size (ptr_bar->bar_window, NULL);
            gui_bar_draw (ptr_bar);
        }
    }
    
    add_bottom = gui_bar_root_get_size (NULL, GUI_BAR_POSITION_BOTTOM);
    add_top = gui_bar_root_get_size (NULL, GUI_BAR_POSITION_TOP);
    add_left = gui_bar_root_get_size (NULL, GUI_BAR_POSITION_LEFT);
    add_right = gui_bar_root_get_size (NULL, GUI_BAR_POSITION_RIGHT);
    
    if (gui_window_auto_resize (gui_windows_tree, add_left, add_top,
                                gui_window_get_width () - add_left - add_right,
                                gui_window_get_height () - add_top - add_bottom,
                                0) < 0)
        gui_window_merge_all (gui_current_window);
    
    for (ptr_win = gui_windows; ptr_win; ptr_win = ptr_win->next_window)
    {
        gui_window_switch_to_buffer (ptr_win, ptr_win->buffer);
        gui_window_draw_separator (ptr_win);
    }
    
    for (ptr_buffer = gui_buffers; ptr_buffer;
         ptr_buffer = ptr_buffer->next_buffer)
    {
        gui_window_redraw_buffer (ptr_buffer);
    }
    
    gui_current_window = old_current_window;
}

/*
 * gui_window_split_horiz: split a window horizontally
 */

void
gui_window_split_horiz (struct t_gui_window *window, int pourcentage)
{
    struct t_gui_window *new_window;
    int height1, height2;
    
    if (!gui_ok)
        return;
    
    height1 = (window->win_height * pourcentage) / 100;
    height2 = window->win_height - height1;
    
    if ((height1 >= WINDOW_MIN_HEIGHT) && (height2 >= WINDOW_MIN_HEIGHT)
        && (pourcentage > 0) && (pourcentage <= 100))
    {
        if ((new_window = gui_window_new (window,
                                          window->win_x, window->win_y,
                                          window->win_width, height1,
                                          100, pourcentage)))
        {
            /* reduce old window height (bottom window) */
            window->win_y = new_window->win_y + new_window->win_height;
            window->win_height = height2;
            window->win_height_pct = 100 - pourcentage;
            
            /* assign same buffer for new window (top window) */
            new_window->buffer = window->buffer;
            new_window->buffer->num_displayed++;
            
            gui_window_switch_to_buffer (window, window->buffer);
            
            gui_current_window = new_window;
            gui_window_switch_to_buffer (gui_current_window, gui_current_window->buffer);
            gui_window_redraw_buffer (gui_current_window->buffer);
        }
    }
}

/*
 * gui_window_split_vertic: split a window vertically
 */

void
gui_window_split_vertic (struct t_gui_window *window, int pourcentage)
{
    struct t_gui_window *new_window;
    int width1, width2;
    
    if (!gui_ok)
        return;
    
    width1 = (window->win_width * pourcentage) / 100;
    width2 = window->win_width - width1 - 1;
    
    if ((width1 >= WINDOW_MIN_WIDTH) && (width2 >= WINDOW_MIN_WIDTH)
        && (pourcentage > 0) && (pourcentage <= 100))
    {
        if ((new_window = gui_window_new (window,
                                          window->win_x + width1 + 1, window->win_y,
                                          width2, window->win_height,
                                          pourcentage, 100)))
        {
            /* reduce old window height (left window) */
            window->win_width = width1;
            window->win_width_pct = 100 - pourcentage;
            
            /* assign same buffer for new window (right window) */
            new_window->buffer = window->buffer;
            new_window->buffer->num_displayed++;
            
            gui_window_switch_to_buffer (window, window->buffer);
            
            gui_current_window = new_window;
            gui_window_switch_to_buffer (gui_current_window, gui_current_window->buffer);
            gui_window_redraw_buffer (gui_current_window->buffer);
            
            /* create & draw separator */
            gui_window_draw_separator (gui_current_window);
        }
    }
}

/*
 * gui_window_resize: resize window
 */

void
gui_window_resize (struct t_gui_window *window, int pourcentage)
{
    struct t_gui_window_tree *parent;
    int old_split_pct, add_bottom, add_top, add_left, add_right;
    
    if (!gui_ok)
        return;
    
    parent = window->ptr_tree->parent_node;
    if (parent)
    {
        old_split_pct = parent->split_pct;
        if (((parent->split_horiz) && (window->ptr_tree == parent->child2))
            || ((!(parent->split_horiz)) && (window->ptr_tree == parent->child1)))
            parent->split_pct = pourcentage;
        else
            parent->split_pct = 100 - pourcentage;
        
        add_bottom = gui_bar_root_get_size (NULL, GUI_BAR_POSITION_BOTTOM);
        add_top = gui_bar_root_get_size (NULL, GUI_BAR_POSITION_TOP);
        add_left = gui_bar_root_get_size (NULL, GUI_BAR_POSITION_LEFT);
        add_right = gui_bar_root_get_size (NULL, GUI_BAR_POSITION_RIGHT);
        
        if (gui_window_auto_resize (gui_windows_tree, add_left, add_top,
                                    gui_window_get_width () - add_left - add_right,
                                    gui_window_get_height () - add_top - add_bottom,
                                    1) < 0)
            parent->split_pct = old_split_pct;
        else
            gui_window_refresh_windows ();
    }
}

/*
 * gui_window_merge: merge window with its sister
 */

int
gui_window_merge (struct t_gui_window *window)
{
    struct t_gui_window_tree *parent, *sister;
    
    if (!gui_ok)
        return 0;
    
    parent = window->ptr_tree->parent_node;
    if (parent)
    {
        sister = (parent->child1->window == window) ?
            parent->child2 : parent->child1;
        
        if (!(sister->window))
            return 0;
        
        if (window->win_y == sister->window->win_y)
        {
            /* horizontal merge */
            window->win_width += sister->window->win_width + 1;
            window->win_width_pct += sister->window->win_width_pct;
        }
        else
        {
            /* vertical merge */
            window->win_height += sister->window->win_height;
            window->win_height_pct += sister->window->win_height_pct;
        }
        if (sister->window->win_x < window->win_x)
            window->win_x = sister->window->win_x;
        if (sister->window->win_y < window->win_y)
            window->win_y = sister->window->win_y;
        
        gui_window_free (sister->window);
        gui_window_tree_node_to_leaf (parent, window);
        
        gui_window_switch_to_buffer (window, window->buffer);
        gui_window_redraw_buffer (window->buffer);
        return 1;
    }
    return 0;
}

/*
 * gui_window_merge_all: merge all windows into only one
 */

void
gui_window_merge_all (struct t_gui_window *window)
{
    int num_deleted, add_bottom, add_top, add_left, add_right;
    
    if (!gui_ok)
        return;
    
    num_deleted = 0;
    while (gui_windows->next_window)
    {
        gui_window_free ((gui_windows == window) ? gui_windows->next_window : gui_windows);
        num_deleted++;
    }
    
    if (num_deleted > 0)
    {
        gui_window_tree_free (&gui_windows_tree);
        gui_window_tree_init (window);
        window->ptr_tree = gui_windows_tree;

        add_bottom = gui_bar_root_get_size (NULL, GUI_BAR_POSITION_BOTTOM);
        add_top = gui_bar_root_get_size (NULL, GUI_BAR_POSITION_TOP);
        add_left = gui_bar_root_get_size (NULL, GUI_BAR_POSITION_LEFT);
        add_right = gui_bar_root_get_size (NULL, GUI_BAR_POSITION_RIGHT);
        window->win_x = add_left;
        window->win_y = add_top;
        window->win_width = gui_window_get_width () - add_left - add_right;
        window->win_height = gui_window_get_height () - add_top - add_bottom;
        
        window->win_width_pct = 100;
        window->win_height_pct = 100;
        
        gui_current_window = window;
        gui_window_switch_to_buffer (window, window->buffer);
        gui_window_redraw_buffer (window->buffer);
    }
}

/*
 * gui_window_side_by_side: return a code about position of 2 windows:
 *                          0 = they're not side by side
 *                          1 = side by side (win2 is over the win1)
 *                          2 = side by side (win2 on the right)
 *                          3 = side by side (win2 below win1)
 *                          4 = side by side (win2 on the left)
 */

int
gui_window_side_by_side (struct t_gui_window *win1, struct t_gui_window *win2)
{
    if (!gui_ok)
        return 0;
    
    /* win2 over win1 ? */
    if (win2->win_y + win2->win_height == win1->win_y)
    {
        if (win2->win_x >= win1->win_x + win1->win_width)
            return 0;
        if (win2->win_x + win2->win_width <= win1->win_x)
            return 0;
        return 1;
    }

    /* win2 on the right ? */
    if (win2->win_x == win1->win_x + win1->win_width + 1)
    {
        if (win2->win_y >= win1->win_y + win1->win_height)
            return 0;
        if (win2->win_y + win2->win_height <= win1->win_y)
            return 0;
        return 2;
    }

    /* win2 below win1 ? */
    if (win2->win_y == win1->win_y + win1->win_height)
    {
        if (win2->win_x >= win1->win_x + win1->win_width)
            return 0;
        if (win2->win_x + win2->win_width <= win1->win_x)
            return 0;
        return 3;
    }
    
    /* win2 on the left ? */
    if (win2->win_x + win2->win_width + 1 == win1->win_x)
    {
        if (win2->win_y >= win1->win_y + win1->win_height)
            return 0;
        if (win2->win_y + win2->win_height <= win1->win_y)
            return 0;
        return 4;
    }

    return 0;
}

/*
 * gui_window_switch_up: search and switch to a window over current window
 */

void
gui_window_switch_up (struct t_gui_window *window)
{
    struct t_gui_window *ptr_win;
    
    if (!gui_ok)
        return;
    
    for (ptr_win = gui_windows; ptr_win;
         ptr_win = ptr_win->next_window)
    {
        if ((ptr_win != window) &&
            (gui_window_side_by_side (window, ptr_win) == 1))
        {
            gui_current_window = ptr_win;
            gui_window_switch_to_buffer (gui_current_window, gui_current_window->buffer);
            gui_window_redraw_buffer (gui_current_window->buffer);
            return;
        }
    }
}

/*
 * gui_window_switch_down: search and switch to a window below current window
 */

void
gui_window_switch_down (struct t_gui_window *window)
{
    struct t_gui_window *ptr_win;
    
    if (!gui_ok)
        return;
    
    for (ptr_win = gui_windows; ptr_win;
         ptr_win = ptr_win->next_window)
    {
        if ((ptr_win != window) &&
            (gui_window_side_by_side (window, ptr_win) == 3))
        {
            gui_current_window = ptr_win;
            gui_window_switch_to_buffer (gui_current_window, gui_current_window->buffer);
            gui_window_redraw_buffer (gui_current_window->buffer);
            return;
        }
    }
}

/*
 * gui_window_switch_left: search and switch to a window on the left of current window
 */

void
gui_window_switch_left (struct t_gui_window *window)
{
    struct t_gui_window *ptr_win;
    
    if (!gui_ok)
        return;
    
    for (ptr_win = gui_windows; ptr_win;
         ptr_win = ptr_win->next_window)
    {
        if ((ptr_win != window) &&
            (gui_window_side_by_side (window, ptr_win) == 4))
        {
            gui_current_window = ptr_win;
            gui_window_switch_to_buffer (gui_current_window, gui_current_window->buffer);
            gui_window_redraw_buffer (gui_current_window->buffer);
            return;
        }
    }
}

/*
 * gui_window_switch_right: search and switch to a window on the right of current window
 */

void
gui_window_switch_right (struct t_gui_window *window)
{
    struct t_gui_window *ptr_win;
    
    if (!gui_ok)
        return;
    
    for (ptr_win = gui_windows; ptr_win;
         ptr_win = ptr_win->next_window)
    {
        if ((ptr_win != window) &&
            (gui_window_side_by_side (window, ptr_win) == 2))
        {
            gui_current_window = ptr_win;
            gui_window_switch_to_buffer (gui_current_window, gui_current_window->buffer);
            gui_window_redraw_buffer (gui_current_window->buffer);
            return;
        }
    }
}

/*
 * gui_window_refresh_screen: called when term size is modified
 *                            force == 1 when Ctrl+L is pressed
 */

void
gui_window_refresh_screen (int force)
{
    int new_height, new_width;
    
    if (!gui_ok)
        return;
    
    if (force || (gui_window_refresh_needed == 1))
    {
        endwin ();
        refresh ();
        
        getmaxyx (stdscr, new_height, new_width);
        
        gui_ok = ((new_width > WINDOW_MIN_WIDTH) && (new_height > WINDOW_MIN_HEIGHT));
        
        if (gui_ok)
        {
            refresh ();
            gui_window_refresh_windows ();
        }
    }
    
    if (gui_window_refresh_needed > 0)
        gui_window_refresh_needed = 0;
}

/*
 * gui_window_refresh_screen_sigwinch: called when signal SIGWINCH is received
 */

void
gui_window_refresh_screen_sigwinch ()
{
    gui_window_refresh_needed = 1;
    //gui_window_refresh_screen (0);
}

/*
 * gui_window_title_set: set terminal title
 */

void
gui_window_title_set ()
{
    char *envterm = getenv ("TERM");
    
    if (envterm)
    {
	if (strcmp( envterm, "sun-cmd") == 0)
	    printf ("\033]l%s %s\033\\", PACKAGE_NAME, PACKAGE_VERSION);
	else if (strcmp(envterm, "hpterm") == 0)
	    printf ("\033&f0k%dD%s %s",
                    (int)(strlen(PACKAGE_NAME) + strlen(PACKAGE_VERSION) + 1),
		    PACKAGE_NAME, PACKAGE_VERSION);
	/* the following term supports the xterm excapes */
	else if (strncmp (envterm, "xterm", 5) == 0
		 || strncmp (envterm, "rxvt", 4) == 0
		 || strcmp (envterm, "Eterm") == 0
		 || strcmp (envterm, "aixterm") == 0
		 || strcmp (envterm, "iris-ansi") == 0
		 || strcmp (envterm, "dtterm") == 0)
	    printf ("\33]0;%s %s\7", PACKAGE_NAME, PACKAGE_VERSION);
	else if (strcmp (envterm, "screen") == 0)
	{
	    printf ("\033k%s %s\033\\", PACKAGE_NAME, PACKAGE_VERSION);
	    /* tryning to set the title of a backgrounded xterm like terminal */
	    printf ("\33]0;%s %s\7", PACKAGE_NAME, PACKAGE_VERSION);
	}
    }
}

/*
 * gui_window_title_reset: reset terminal title
 */

void
gui_window_title_reset ()
{
    char *envterm = getenv ("TERM");
    char *envshell = getenv ("SHELL");

    if (envterm)
    {
	if (strcmp( envterm, "sun-cmd") == 0)
	    printf ("\033]l%s\033\\", "Terminal");
	else if (strcmp( envterm, "hpterm") == 0)
	    printf ("\033&f0k%dD%s", (int)strlen("Terminal"), "Terminal");
	/* the following term supports the xterm excapes */
	else if (strncmp (envterm, "xterm", 5) == 0
		 || strncmp (envterm, "rxvt", 4) == 0
		 || strcmp (envterm, "Eterm") == 0
		 || strcmp( envterm, "aixterm") == 0
		 || strcmp( envterm, "iris-ansi") == 0
		 || strcmp( envterm, "dtterm") == 0)
	    printf ("\33]0;%s\7", "Terminal");
	else if (strcmp (envterm, "screen") == 0)
	{
	    char *shell, *shellname;
	    if (envshell)
	    {
		shell  = strdup (envterm);
		shellname = basename(shell);
		if (shell)
		{
		    printf ("\033k%s\033\\", shellname);
		    free (shell);
		}
		else
		    printf ("\033k%s\033\\", envterm);
	    }
	    else
		printf ("\033k%s\033\\", envterm);
	    /* tryning to reset the title of a backgrounded xterm like terminal */
	    printf ("\33]0;%s\7", "Terminal");
	}
    }
}

/*
 * gui_window_objects_print_log: print window Curses objects infos in log
 *                               (usually for crash dump)
 */

void
gui_window_objects_print_log (struct t_gui_window *window)
{
    struct t_gui_bar_window *ptr_bar_win;

    log_printf ("");
    log_printf ("  win_title . . . . . : 0x%x", GUI_CURSES(window)->win_title);
    log_printf ("  win_chat. . . . . . : 0x%x", GUI_CURSES(window)->win_chat);
    log_printf ("  win_nick. . . . . . : 0x%x", GUI_CURSES(window)->win_nick);
    log_printf ("  win_status. . . . . : 0x%x", GUI_CURSES(window)->win_status);
    log_printf ("  win_infobar . . . . : 0x%x", GUI_CURSES(window)->win_infobar);
    log_printf ("  win_input . . . . . : 0x%x", GUI_CURSES(window)->win_input);
    log_printf ("  win_separator . . . : 0x%x", GUI_CURSES(window)->win_separator);
    
    for (ptr_bar_win = GUI_CURSES(window)->bar_windows; ptr_bar_win;
         ptr_bar_win = ptr_bar_win->next_bar_window)
    {
        gui_bar_window_print_log (ptr_bar_win);
    }
}
