/* The three sigfillset_* categories and their contents on Linux
   (N3924 7.14.2.1-3): the synchronous set, the non-debug asynchronous
   set, and the debug asynchronous set.  The N3924 API under test is
   declared in the installed <signal.h> (__USE_GNU) and implemented in
   libc by the wg14 subdir.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#define _GNU_SOURCE 1

#include <signal.h>
#include <string.h>

#include <support/check.h>

static int
check_members (const sigset_t *set, const int *must_have,
	       const int *must_not_have)
{
  size_t i;
  for (i = 0; must_have[i] != -1; i++)
    TEST_COMPARE (sigismember (set, must_have[i]), 1);
  for (i = 0; must_not_have[i] != -1; i++)
    TEST_COMPARE (sigismember (set, must_not_have[i]), 0);
  return 0;
}

static const int synchronous[]
    = { SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGPIPE, SIGSEGV, SIGSYS, -1 };
static const int asynchronous_nondebug[]
    = { SIGALRM, SIGCHLD, SIGCONT, SIGHUP,  SIGINT,    SIGKILL,
	SIGSTOP, SIGTERM, SIGTSTP, SIGTTIN, SIGTTOU,   SIGUSR1,
	SIGUSR2, SIGPOLL, SIGPROF, SIGURG,  SIGVTALRM, -1 };
static const int asynchronous_debug[]
    = { SIGQUIT, SIGTRAP, SIGXCPU, SIGXFSZ, -1 };

static int
do_test (void)
{
  sigset_t sync, nondebug, debug;
  memset (&sync, 0, sizeof (sync));
  memset (&nondebug, 0, sizeof (nondebug));
  memset (&debug, 0, sizeof (debug));

  TEST_COMPARE (sigfillset_synchronous (&sync), 0);
  TEST_COMPARE (sigfillset_asynchronous_nondebug (&nondebug), 0);
  TEST_COMPARE (sigfillset_asynchronous_debug (&debug), 0);

  /* The documented minimum content per category.  */
  check_members (&sync, synchronous, (const int[]){ SIGQUIT, SIGTERM, -1 });
  check_members (&nondebug, asynchronous_nondebug,
		 (const int[]){ SIGSEGV, SIGQUIT, -1 });
  check_members (&debug, asynchronous_debug,
		 (const int[]){ SIGSEGV, SIGTERM, -1 });

  /* The sync/async categorisation partitions the standard signals on
     Linux: the synchronous set and the asynchronous sets are disjoint,
     and nothing but the documented minimums leaks between them.  */
  for (int signo = 1; signo < NSIG; signo++)
    {
      const int in_sync = sigismember (&sync, signo);
      const int in_async
	  = (sigismember (&nondebug, signo) || sigismember (&debug, signo));
      TEST_VERIFY (!(in_sync && in_async));
    }

  return 0;
}

#include <support/test-driver.c>