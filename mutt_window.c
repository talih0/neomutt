/**
 * @file
 * Window management
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @page window Window management
 *
 * Window management
 */

#include "config.h"
#include <stdarg.h>
#include <string.h>
#include "mutt/mutt.h"
#include "mutt_window.h"
#include "globals.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "options.h"
#include "pager.h"
#include "reflow.h"

struct MuttWindow *RootWindow = NULL;       ///< Parent of all Windows
struct MuttWindow *MuttDialogWindow = NULL; ///< Parent of all Dialogs

struct MuttWindow *MuttHelpWindow = NULL;     ///< Help Window
struct MuttWindow *MuttIndexWindow = NULL;    ///< Index Window
struct MuttWindow *MuttMessageWindow = NULL;  ///< Message Window
struct MuttWindow *MuttPagerBarWindow = NULL; ///< Pager Status Window
struct MuttWindow *MuttPagerWindow = NULL;    ///< Pager Window
#ifdef USE_SIDEBAR
struct MuttWindow *MuttSidebarWindow = NULL; ///< Sidebar Window
#endif
struct MuttWindow *MuttStatusWindow = NULL; ///< Status Window

/**
 * mutt_window_new - Create a new Window
 * @param orient Window orientation, e.g. #MUTT_WIN_ORIENT_VERTICAL
 * @param size   Window size, e.g. #MUTT_WIN_SIZE_MAXIMISE
 * @param rows   Initial number of rows to allocate, can be #MUTT_WIN_SIZE_UNLIMITED
 * @param cols   Initial number of columns to allocate, can be #MUTT_WIN_SIZE_UNLIMITED
 * @retval ptr New Window
 */
struct MuttWindow *mutt_window_new(enum MuttWindowOrientation orient,
                                   enum MuttWindowSize size, int rows, int cols)
{
  struct MuttWindow *win = mutt_mem_calloc(1, sizeof(struct MuttWindow));

  win->orient = orient;
  win->size = size;
  win->req_rows = rows;
  win->req_cols = cols;
  win->state.visible = true;
  TAILQ_INIT(&win->children);
  return win;
}

/**
 * mutt_window_free - Free a Window and its children
 * @param ptr Window to free
 */
void mutt_window_free(struct MuttWindow **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct MuttWindow *win = *ptr;

  mutt_winlist_free(&win->children);

  FREE(ptr);
}

#ifdef USE_SLANG_CURSES
/**
 * vw_printw - Write a formatted string to a Window (function missing from Slang)
 * @param win Window
 * @param fmt printf format string
 * @param ap  printf arguments
 * @retval 0 Always
 */
static int vw_printw(SLcurses_Window_Type *win, const char *fmt, va_list ap)
{
  char buf[1024];

  (void) SLvsnprintf(buf, sizeof(buf), (char *) fmt, ap);
  SLcurses_waddnstr(win, buf, -1);
  return 0;
}
#endif

/**
 * mutt_window_clearline - Clear a row of a Window
 * @param win Window
 * @param row Row to clear
 */
void mutt_window_clearline(struct MuttWindow *win, int row)
{
  mutt_window_move(win, row, 0);
  mutt_window_clrtoeol(win);
}

/**
 * mutt_window_clrtoeol - Clear to the end of the line
 * @param win Window
 *
 * @note Assumes the cursor has already been positioned within the window.
 */
void mutt_window_clrtoeol(struct MuttWindow *win)
{
  if (!win || !stdscr)
    return;

  if (win->state.col_offset + win->state.cols == COLS)
    clrtoeol();
  else
  {
    int row = 0;
    int col = 0;
    getyx(stdscr, row, col);
    int curcol = col;
    while (curcol < (win->state.col_offset + win->state.cols))
    {
      addch(' ');
      curcol++;
    }
    move(row, col);
  }
}

/**
 * mutt_window_free_all - Free all the default Windows
 */
void mutt_window_free_all(void)
{
  MuttHelpWindow = NULL;
  MuttIndexWindow = NULL;
  MuttMessageWindow = NULL;
  MuttPagerBarWindow = NULL;
  MuttPagerWindow = NULL;
#ifdef USE_SIDEBAR
  MuttSidebarWindow = NULL;
#endif
  MuttStatusWindow = NULL;
  mutt_window_free(&RootWindow);
}

/**
 * mutt_window_getxy - Get the cursor position in the Window
 * @param[in]  win Window
 * @param[out] x   X-Coordinate
 * @param[out] y   Y-Coordinate
 *
 * Assumes the current position is inside the window.  Otherwise it will
 * happily return negative or values outside the window boundaries
 */
void mutt_window_getxy(struct MuttWindow *win, int *x, int *y)
{
  int row = 0;
  int col = 0;

  getyx(stdscr, row, col);
  if (x)
    *x = col - win->state.col_offset;
  if (y)
    *y = row - win->state.row_offset;
}

/**
 * mutt_window_init - Create the default Windows
 *
 * Create the Help, Index, Status, Message and Sidebar Windows.
 */
void mutt_window_init(void)
{
  if (RootWindow)
    return;

  RootWindow = mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_FIXED, 0, 0);
  MuttHelpWindow = mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_FIXED,
                                   1, MUTT_WIN_SIZE_UNLIMITED);
  MuttDialogWindow = mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                                     MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  MuttMessageWindow = mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_FIXED,
                                      1, MUTT_WIN_SIZE_UNLIMITED);

  RootWindow->name = "Root";
  MuttHelpWindow->name = "Help";
  MuttDialogWindow->name = "Dialogs";
  MuttMessageWindow->name = "Message";

  mutt_window_add_child(RootWindow, MuttHelpWindow);
  mutt_window_add_child(RootWindow, MuttDialogWindow);
  mutt_window_add_child(RootWindow, MuttMessageWindow);
}

/**
 * mutt_window_move - Move the cursor in a Window
 * @param win Window
 * @param row Row to move to
 * @param col Column to move to
 * @retval OK  Success
 * @retval ERR Error
 */
int mutt_window_move(struct MuttWindow *win, int row, int col)
{
  return move(win->state.row_offset + row, win->state.col_offset + col);
}

/**
 * mutt_window_mvaddstr - Move the cursor and write a fixed string to a Window
 * @param win Window to write to
 * @param row Row to move to
 * @param col Column to move to
 * @param str String to write
 * @retval OK  Success
 * @retval ERR Error
 */
int mutt_window_mvaddstr(struct MuttWindow *win, int row, int col, const char *str)
{
  return mvaddstr(win->state.row_offset + row, win->state.col_offset + col, str);
}

/**
 * mutt_window_mvprintw - Move the cursor and write a formatted string to a Window
 * @param win Window to write to
 * @param row Row to move to
 * @param col Column to move to
 * @param fmt printf format string
 * @param ... printf arguments
 * @retval num Success, characters written
 * @retval ERR Error, move failed
 */
int mutt_window_mvprintw(struct MuttWindow *win, int row, int col, const char *fmt, ...)
{
  int rc = mutt_window_move(win, row, col);
  if (rc == ERR)
    return rc;

  va_list ap;
  va_start(ap, fmt);
  rc = vw_printw(stdscr, fmt, ap);
  va_end(ap);

  return rc;
}

/**
 * mutt_window_copy_size - Copy the size of one Window to another
 * @param win_src Window to copy
 * @param win_dst Window to resize
 */
void mutt_window_copy_size(const struct MuttWindow *win_src, struct MuttWindow *win_dst)
{
  if (!win_src || !win_dst)
    return;

  win_dst->state.rows = win_src->state.rows;
  win_dst->state.cols = win_src->state.cols;
  win_dst->state.row_offset = win_src->state.row_offset;
  win_dst->state.col_offset = win_src->state.col_offset;
}

/**
 * mutt_window_reflow_prep - Prepare the Windows for reflowing
 *
 * This bit of business logic is temporary.
 * Eventually, it will be split into the handlers for various Windows.
 *
 * The Window layout is affected by whether the Pager is visible and these
 * config variables:
 * - C_Help
 * - C_PagerIndexLines
 * - C_StatusOnTop
 */
void mutt_window_reflow_prep(void)
{
  MuttHelpWindow->state.visible = C_Help;

  struct MuttWindow *parent = MuttHelpWindow->parent;
  struct MuttWindow *first = TAILQ_FIRST(&parent->children);

  if ((C_StatusOnTop && (first == MuttHelpWindow)) ||
      (!C_StatusOnTop && (first != MuttHelpWindow)))
  {
    // Swap the HelpLine and the Container of the Sidebar/Index/Pager
    TAILQ_REMOVE(&parent->children, first, entries);
    TAILQ_INSERT_TAIL(&parent->children, first, entries);
  }

  if (!MuttIndexWindow || !MuttPagerWindow)
    return;

  parent = MuttIndexWindow->parent;
  first = TAILQ_FIRST(&parent->children);

  if ((C_StatusOnTop && (first == MuttIndexWindow)) ||
      (!C_StatusOnTop && (first != MuttIndexWindow)))
  {
    // Swap the Index and the Status Windows
    TAILQ_REMOVE(&parent->children, first, entries);
    TAILQ_INSERT_TAIL(&parent->children, first, entries);
  }

  parent = MuttPagerWindow->parent;
  first = TAILQ_FIRST(&parent->children);

  if ((C_StatusOnTop && (first == MuttPagerWindow)) ||
      (!C_StatusOnTop && (first != MuttPagerWindow)))
  {
    // Swap the Pager and Pager Bar Windows
    TAILQ_REMOVE(&parent->children, first, entries);
    TAILQ_INSERT_TAIL(&parent->children, first, entries);
  }

  parent = MuttPagerWindow->parent;
  if (parent->state.visible)
  {
    MuttIndexWindow->req_rows = C_PagerIndexLines;
    MuttIndexWindow->size = MUTT_WIN_SIZE_FIXED;

    MuttIndexWindow->parent->size = MUTT_WIN_SIZE_MINIMISE;
    MuttIndexWindow->parent->state.visible = (C_PagerIndexLines != 0);
  }
  else
  {
    MuttIndexWindow->req_rows = MUTT_WIN_SIZE_UNLIMITED;
    MuttIndexWindow->size = MUTT_WIN_SIZE_MAXIMISE;

    MuttIndexWindow->parent->size = MUTT_WIN_SIZE_MAXIMISE;
    MuttIndexWindow->parent->state.visible = true;
  }
}

/**
 * mutt_window_reflow - Resize a Window and its children
 * @param win Window to resize
 */
void mutt_window_reflow(struct MuttWindow *win)
{
  if (OptNoCurses)
    return;

  mutt_debug(LL_DEBUG2, "entering\n");
  mutt_window_reflow_prep();
  window_reflow(win ? win : RootWindow);

  mutt_menu_set_current_redraw_full();
  /* the pager menu needs this flag set to recalc line_info */
  mutt_menu_set_current_redraw(REDRAW_FLOW);
  win_dump();
}

/**
 * mutt_window_reflow_message_rows - Resize the Message Window
 * @param mw_rows Number of rows required
 *
 * Resize the other Windows to allow a multi-line message to be displayed.
 */
void mutt_window_reflow_message_rows(int mw_rows)
{
  MuttMessageWindow->req_rows = mw_rows;
  mutt_window_reflow(MuttMessageWindow->parent);

  /* We don't also set REDRAW_FLOW because this function only
   * changes rows and is a temporary adjustment. */
  mutt_menu_set_current_redraw_full();
}

/**
 * mutt_window_wrap_cols - Calculate the wrap column for a given screen width
 * @param width Screen width
 * @param wrap  Wrap config
 * @retval num Column that text should be wrapped at
 *
 * The wrap variable can be negative, meaning there should be a right margin.
 */
int mutt_window_wrap_cols(int width, short wrap)
{
  if (wrap < 0)
    return (width > -wrap) ? (width + wrap) : width;
  else if (wrap)
    return (wrap < width) ? wrap : width;
  else
    return width;
}

/**
 * mutt_window_add_child - Add a child to Window
 * @param parent Window to add to
 * @param child  Window to add
 */
void mutt_window_add_child(struct MuttWindow *parent, struct MuttWindow *child)
{
  if (!parent || !child)
    return;

  TAILQ_INSERT_TAIL(&parent->children, child, entries);
  child->parent = parent;
}

/**
 * mutt_winlist_free - Free a tree of Windows
 * @param head WindowList to free
 */
void mutt_winlist_free(struct MuttWindowList *head)
{
  if (!head)
    return;

  struct MuttWindow *np = NULL;
  struct MuttWindow *tmp = NULL;
  TAILQ_FOREACH_SAFE(np, head, entries, tmp)
  {
    TAILQ_REMOVE(head, np, entries);
    mutt_winlist_free(&np->children);
    FREE(&np);
  }
}

/**
 * mutt_window_set_root - XXX
 * @param rows
 * @param cols
 */
void mutt_window_set_root(int rows, int cols)
{
  if (!RootWindow)
    return;

  bool changed = false;

  if (RootWindow->state.rows != rows)
  {
    RootWindow->state.rows = rows;
    changed = true;
  }

  if (RootWindow->state.cols != cols)
  {
    RootWindow->state.cols = cols;
    changed = true;
  }

  if (changed)
  {
    mutt_window_reflow(RootWindow);
  }
}

void dump(struct MuttWindow *win, int indent)
{
  if (!win->state.visible)
    return;

  mutt_debug(LL_DEBUG1, "%*s[%d,%d] %s (%d,%d)\n", indent, "", win->state.col_offset,
             win->state.row_offset, win->name, win->state.cols, win->state.rows);

  struct MuttWindow *np = NULL;
  TAILQ_FOREACH(np, &win->children, entries)
  {
    dump(np, indent + 4);
  }
}

void win_dump(void)
{
  dump(RootWindow, 0);
}
