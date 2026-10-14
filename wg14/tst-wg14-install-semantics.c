/* Port of the wg14_signals install-semantics tests (siginstall_null_set_
   test.c, edge_api_coverage_test.c, sig_default_preserve_test.c) plus the
   glibc-native three-path internal-signal matrix and the consumer
   coexistence arms, to glibc's harness.  The N3924 API under test is
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

#include <errno.h>
#include <signal.h>
#include <string.h>

#include <support/check.h>

/* glibc's internal signals (SIGCANCEL == __SIGRTMIN == 32, SIGSETXID ==
   __SIGRTMIN + 1 == 33, sysdeps/unix/sysv/linux/internal-signals.h) are
   not exposed by installed <signal.h>, so the tests spell them
   numerically; the wg14_signals machinery excludes them via glibc's own
   sigfillset() (clear_internal_signals) and the reference's own
   deletions, and a hand-built set containing them fails the whole
   siginstall with EINVAL and a full rollback (plans/glibc-fork.md
   "Comprehensive review" item 4).  */
#define SIGCANCEL_NUM (__SIGRTMIN)
#define SIGSETXID_NUM (__SIGRTMIN + 1)

static enum sig_decision
never_called_decider (struct stdc_siginfo *rsi)
{
  (void) rsi;
  return sig_decision_next_decider;
}

static int
do_test (void)
{
  /* Path (a): siginstall(NULL) - "all the standard POSIX signals".  The
     handle must cover the standard signals and exclude glibc's internal
     signals (32/33) and the whole realtime range.  */
  sigset_t *set = siginstall (NULL);
  TEST_VERIFY (set != NULL);
  if (set == NULL)
    return 1;
  TEST_COMPARE (sigismember (set, SIGILL), 1);
  TEST_COMPARE (sigismember (set, SIGTERM), 1);
  TEST_COMPARE (sigismember (set, SIGSEGV), 1);
  TEST_COMPARE (sigismember (set, SIGCANCEL_NUM), 0);
  TEST_COMPARE (sigismember (set, SIGSETXID_NUM), 0);
  for (int signo = SIGRTMIN; signo <= SIGRTMAX; signo++)
    TEST_COMPARE (sigismember (set, signo), 0);
  TEST_COMPARE (siguninstall (set), 0);

  /* Path (b): an explicit set built with glibc's own sigfillset() - the
     documented glibc consumer pattern - installs fine and its handle
     excludes 32/33 too (glibc's sigfillset clears the internal signals
     before the reference code sees the set).  */
  sigset_t guarded;
  sigfillset (&guarded);
  sigset_t *explicit = siginstall (&guarded);
  TEST_VERIFY (explicit != NULL);
  if (explicit == NULL)
    return 1;
  TEST_COMPARE (sigismember (explicit, SIGCANCEL_NUM), 0);
  TEST_COMPARE (sigismember (explicit, SIGSETXID_NUM), 0);
  TEST_COMPARE (sigismember (explicit, SIGILL), 1);
  TEST_COMPARE (sigismember (explicit, SIGTERM), 1);
  TEST_COMPARE (siguninstall (explicit), 0);

  /* Path (c): an explicit set with SIGCANCEL hand-added via sigaddset.
     Inside libc the wg14 library objects see the internal pthread
     headers, which define SIGCANCEL/SIGSETXID (sysdeps/nptl/pthreadP.h
     -> internal-signals.h), so the reference's own libc-internal-signal
     deletion fires (thrd_signal_handle_common.ipp.ipp:583-588 - the
     reference's documented intent: "libc-internal signals are never
     installable, regardless of which guarded set was supplied") and the
     call SUCCEEDS with SIGCANCEL silently excluded - no EINVAL, no
     rollback (the standalone suite sees the EINVAL-rollback arm instead,
     because its build uses only public headers where SIGCANCEL is not
     defined).  SIGTERM sorts before 32 in the install loop, so it is
     installed by this call and must be restored by the siguninstall.  */
  struct sigaction sa_before, sa_after;
  memset (&sa_before, 0, sizeof (sa_before));
  TEST_COMPARE (sigaction (SIGTERM, NULL, &sa_before), 0);
  TEST_VERIFY (sa_before.sa_handler == SIG_DFL);
  sigset_t bad;
  sigemptyset (&bad);
  sigaddset (&bad, SIGTERM);
  sigaddset (&bad, SIGCANCEL_NUM);
  errno = 0;
  void *bad_handlers = siginstall (&bad);
  TEST_VERIFY (bad_handlers != NULL);
  if (bad_handlers != NULL)
    {
      TEST_COMPARE (sigismember (bad_handlers, SIGCANCEL_NUM), 0);
      memset (&sa_after, 0, sizeof (sa_after));
      TEST_COMPARE (sigaction (SIGTERM, NULL, &sa_after), 0);
      TEST_VERIFY (sa_after.sa_handler != SIG_DFL);
      TEST_COMPARE (siguninstall (bad_handlers), 0);
    }
  memset (&sa_after, 0, sizeof (sa_after));
  TEST_COMPARE (sigaction (SIGTERM, NULL, &sa_after), 0);
  TEST_VERIFY (sa_after.sa_handler == SIG_DFL);
  TEST_VERIFY (sa_after.sa_sigaction == sa_before.sa_sigaction);
  void *after_rollback = siginstall (NULL);
  TEST_VERIFY (after_rollback != NULL);
  if (after_rollback != NULL)
    TEST_COMPARE (siguninstall (after_rollback), 0);

  /* Path (d): SIGKILL/SIGSTOP are silently skipped; the non-debug
     asynchronous set contains them per the header docs, and installing
     it must succeed.  */
  sigset_t asyncset;
  TEST_COMPARE (sigfillset_asynchronous_nondebug (&asyncset), 0);
  TEST_COMPARE (sigismember (&asyncset, SIGKILL), 1);
  TEST_COMPARE (sigismember (&asyncset, SIGSTOP), 1);
  void *async_handlers = siginstall (&asyncset);
  TEST_VERIFY (async_handlers != NULL);
  if (async_handlers != NULL)
    TEST_COMPARE (siguninstall (async_handlers), 0);

  /* Edge API coverage (edge_api_coverage_test.c): the asynchronous
     sigfillset helpers, siguninstall_system, and the
     signal_decider_create error paths.  */
  sigset_t nondebug, debug;
  memset (&nondebug, 0, sizeof (nondebug));
  TEST_COMPARE (sigfillset_asynchronous_nondebug (&nondebug), 0);
  TEST_COMPARE (sigismember (&nondebug, SIGSEGV), 0);
  memset (&debug, 0, sizeof (debug));
  TEST_COMPARE (sigfillset_asynchronous_debug (&debug), 0);
  TEST_COMPARE (siguninstall_system (0), 0);
  errno = 0;
  TEST_COMPARE (siguninstall_system (1), -1);
  TEST_COMPARE (errno, EINVAL);
  union stdc_siginfo_value value = { 0 };
  errno = 0;
  TEST_VERIFY (signal_decider_create (NULL, false, never_called_decider, value)
	       == NULL);
  TEST_COMPARE (errno, EINVAL);
  sigset_t empty;
  sigemptyset (&empty);
  errno = 0;
  TEST_VERIFY (
      signal_decider_create (&empty, false, never_called_decider, value)
      == NULL);
  TEST_COMPARE (errno, EINVAL);
  errno = 0;
  TEST_VERIFY (signal_decider_create (&guarded, false, NULL, value) == NULL);
  TEST_COMPARE (errno, EINVAL);

  /* Consumer coexistence: two siginstall'ers on overlapping sets are
     refcounted per signal - the first install captures the pre-install
     disposition, the last uninstall restores it.  */
  sigset_t set1, set2;
  sigemptyset (&set1);
  sigaddset (&set1, SIGTERM);
  sigaddset (&set1, SIGUSR1);
  sigemptyset (&set2);
  sigaddset (&set2, SIGTERM);
  sigaddset (&set2, SIGUSR2);
  void *h1 = siginstall (&set1);
  void *h2 = siginstall (&set2);
  TEST_VERIFY (h1 != NULL && h2 != NULL);
  struct sigaction q;
  memset (&q, 0, sizeof (q));
  TEST_COMPARE (sigaction (SIGTERM, NULL, &q), 0);
  TEST_VERIFY (q.sa_sigaction != NULL);
  TEST_VERIFY (q.sa_handler != SIG_DFL);
  /* The second uninstall leaves the first's handler in place.  */
  TEST_COMPARE (siguninstall (h2), 0);
  memset (&q, 0, sizeof (q));
  TEST_COMPARE (sigaction (SIGTERM, NULL, &q), 0);
  TEST_VERIFY (q.sa_handler != SIG_DFL);
  /* The last uninstall restores the pre-install SIG_DFL disposition.  */
  TEST_COMPARE (siguninstall (h1), 0);
  memset (&q, 0, sizeof (q));
  TEST_COMPARE (sigaction (SIGTERM, NULL, &q), 0);
  TEST_VERIFY (q.sa_handler == SIG_DFL);

  /* Consumer incremental late install: a signal installed on demand after
     the initial registration installs cleanly and uninstalls cleanly.  */
  sigset_t late;
  sigemptyset (&late);
  sigaddset (&late, SIGPIPE);
  void *h3 = siginstall (&late);
  TEST_VERIFY (h3 != NULL);
  if (h3 != NULL)
    TEST_COMPARE (siguninstall (h3), 0);

  /* sig_default_preserve: when sighandle() falls through to a SIG_DFL
     previously-installed disposition for a non-terminating default
     (SIGCONT), the built-in reset-and-re-raise must NOT permanently
     discard the library's installed handler (wg14_signals analysis.md
     DFLT).  */
  sigset_t cont;
  sigemptyset (&cont);
  sigaddset (&cont, SIGCONT);
  void *h4 = siginstall (&cont);
  TEST_VERIFY (h4 != NULL);
  struct sigaction before, after;
  memset (&before, 0, sizeof (before));
  TEST_COMPARE (sigaction (SIGCONT, NULL, &before), 0);
  TEST_VERIFY (before.sa_handler != SIG_DFL);
  TEST_VERIFY (before.sa_handler != SIG_IGN);
  /* No decider claims the raise; the SIG_DFL default action is taken via
     reset-and-re-raise, which must preserve the installed handler.  */
  TEST_VERIFY (sighandle (SIGCONT, NULL, NULL) == true);
  memset (&after, 0, sizeof (after));
  TEST_COMPARE (sigaction (SIGCONT, NULL, &after), 0);
  TEST_VERIFY (after.sa_handler != SIG_DFL);
  TEST_VERIFY (after.sa_handler == before.sa_handler);
  TEST_COMPARE (siguninstall (h4), 0);

  return 0;
}

#include <support/test-driver.c>