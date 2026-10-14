/* Port of the wg14_signals handoff tests (nsih_siginfo_handoff_test.c,
   decider_destroy_return_test.c) to glibc's harness.  The N3924 API
   under test is declared in the installed <signal.h> (__USE_GNU) and
   implemented in libc by the wg14 subdir.
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

static volatile sig_atomic_t handler_calls;
static volatile sig_atomic_t saw_valid_si;
static volatile sig_atomic_t saw_signo;
static volatile sig_atomic_t saw_si_code_user;

static void
preexisting_siginfo_handler (int signo, siginfo_t *si, void *ctx)
{
  (void) ctx;
  handler_calls++;
  if (si != NULL)
    {
      saw_valid_si = 1;
      if (si->si_signo == signo)
	saw_signo = 1;
      if (si->si_code == SI_USER)
	saw_si_code_user = 1;
    }
}

static enum sig_decision
claiming_decider (struct stdc_siginfo *rsi)
{
  (void) rsi;
  return sig_decision_resume_execution;
}

static int
do_test (void)
{
  /* nsih_siginfo_handoff: sighandle(signo, NULL, NULL) hands off to the
     previously installed handler; an SA_SIGINFO handler must receive a
     synthesised minimal siginfo_t (non-NULL, si_signo set, si_code
     SI_USER) rather than NULL, which would crash any handler that reads
     si->si_signo without a NULL check (wg14_signals analysis.md NSIH).  */
  {
    struct sigaction sa;
    memset (&sa, 0, sizeof (sa));
    sa.sa_sigaction = preexisting_siginfo_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset (&sa.sa_mask);
    TEST_COMPARE (sigaction (SIGUSR2, &sa, NULL), 0);

    sigset_t guarded;
    sigemptyset (&guarded);
    sigaddset (&guarded, SIGUSR2);
    void *handlers = siginstall (&guarded);
    TEST_VERIFY (handlers != NULL);
    if (handlers == NULL)
      return 1;
    /* No deciders claim the raise, so sighandle hands off to the
       pre-existing SA_SIGINFO handler with a synthesised siginfo.  */
    TEST_VERIFY (sighandle (SIGUSR2, NULL, NULL) == true);
    TEST_COMPARE (handler_calls, 1);
    TEST_VERIFY (saw_valid_si != 0);
    TEST_VERIFY (saw_signo != 0);
    TEST_VERIFY (saw_si_code_user != 0);

    TEST_COMPARE (siguninstall (handlers), 0);
    /* siguninstall must restore the pre-existing handler.  */
    memset (&sa, 0, sizeof (sa));
    TEST_COMPARE (sigaction (SIGUSR2, NULL, &sa), 0);
    TEST_VERIFY ((sa.sa_flags & SA_SIGINFO) != 0);
    TEST_VERIFY (sa.sa_sigaction == preexisting_siginfo_handler);
  }

  /* decider_destroy_return: destroying a decider handle whose slots are
     all NULL (every guarded signal had no handler installed at create
     time - the warning path) must report success 0, as must a mixed
     handle with one installed signal, while a NULL handle still fails
     with EINVAL (wg14_signals analysis.md SDCF).  Keep the consumer's
     teardown order: signal_decider_destroy BEFORE siguninstall so no
     decider node dangles off a live raw-handler entry.  */
  {
    union stdc_siginfo_value value = { .int_value = 7 };
    sigset_t g;
    sigemptyset (&g);
    sigaddset (&g, SIGABRT);
    void *d = signal_decider_create (&g, false, claiming_decider, value);
    TEST_VERIFY (d != NULL);
    errno = 0;
    TEST_COMPARE (signal_decider_destroy (d), 0);
    TEST_COMPARE (errno, 0);

    sigset_t install;
    sigemptyset (&install);
    sigaddset (&install, SIGABRT);
    void *h = siginstall (&install);
    TEST_VERIFY (h != NULL);
    sigset_t g2;
    sigemptyset (&g2);
    sigaddset (&g2, SIGABRT);
    sigaddset (&g2, SIGTERM);
    void *d2 = signal_decider_create (&g2, false, claiming_decider, value);
    TEST_VERIFY (d2 != NULL);
    TEST_COMPARE (signal_decider_destroy (d2), 0);
    TEST_COMPARE (siguninstall (h), 0);

    errno = 0;
    TEST_COMPARE (signal_decider_destroy (NULL), -1);
    TEST_COMPARE (errno, EINVAL);
  }

  return 0;
}

#include <support/test-driver.c>