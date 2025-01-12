/**
 * @file
 * Signal handling
 *
 * @authors
 * Copyright (C) 1996-2000,2012 Michael R. Elkins <me@mutt.org>
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
 * @page mutt_signal Signal handling
 *
 * Signal handling
 */

#include "config.h"
#include <stddef.h>
#include <errno.h>
#include <signal.h>
#include "mutt/mutt.h"
#include "globals.h"
#include "mutt_attach.h"
#include "mutt_curses.h"
#ifdef HAVE_LIBUNWIND
#include "mutt.h"
#endif

static int IsEndwin = 0;

/**
 * curses_signal_handler - Catch signals and relay the info to the main program
 * @param sig Signal number, e.g. SIGINT
 */
static void curses_signal_handler(int sig)
{
  int save_errno = errno;

  switch (sig)
  {
    case SIGTSTP: /* user requested a suspend */
      if (!C_Suspend)
        break;
      IsEndwin = isendwin();
      curs_set(1);
      if (!IsEndwin)
        endwin();
      kill(0, SIGSTOP);
      /* fallthrough */

    case SIGCONT:
      if (!IsEndwin)
        refresh();
      mutt_curs_set(-1);
      /* We don't receive SIGWINCH when suspended; however, no harm is done by
       * just assuming we received one, and triggering the 'resize' anyway. */
      SigWinch = 1;
      break;

    case SIGWINCH:
      SigWinch = 1;
      break;

    case SIGINT:
      SigInt = 1;
      break;
  }
  errno = save_errno;
}

/**
 * curses_exit_handler - Notify the user and shutdown gracefully
 * @param sig Signal number, e.g. SIGTERM
 */
static void curses_exit_handler(int sig)
{
  curs_set(1);
  endwin(); /* just to be safe */
  mutt_unlink_temp_attachments();
  mutt_sig_exit_handler(sig); /* DOES NOT RETURN */
}

/**
 * curses_segv_handler - Catch a segfault and print a backtrace
 * @param sig Signal number, e.g. SIGSEGV
 */
static void curses_segv_handler(int sig)
{
  curs_set(1);
  endwin(); /* just to be safe */
#ifdef HAVE_LIBUNWIND
  show_backtrace();
#endif

  struct sigaction act;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  act.sa_handler = SIG_DFL;
  sigaction(sig, &act, NULL);
  // Re-raise the signal to give outside handlers a chance to deal with it
  raise(sig);
}

#ifdef USE_SLANG_CURSES
/**
 * mutt_intr_hook - Workaround handler for slang
 * @retval -1 Always
 */
static int mutt_intr_hook(void)
{
  return -1;
}
#endif /* USE_SLANG_CURSES */

/**
 * mutt_signal_init - Initialise the signal handling
 */
void mutt_signal_init(void)
{
  mutt_sig_init(curses_signal_handler, curses_exit_handler, curses_segv_handler);

#ifdef USE_SLANG_CURSES
  /* This bit of code is required because of the implementation of
   * SLcurses_wgetch().  If a signal is received (like SIGWINCH) when we
   * are in blocking mode, SLsys_getkey() will not return an error unless
   * a handler function is defined and it returns -1.  This is needed so
   * that if the user resizes the screen while at a prompt, it will just
   * abort and go back to the main-menu.  */
  SLang_getkey_intr_hook = mutt_intr_hook;
#endif
}
