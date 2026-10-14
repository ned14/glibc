/* abort() semantics under the N3924 machinery (plans/glibc-fork.md
   "Comprehensive review" item 1): no siginstall -> SIGABRT death;
   abort() inside sigguarded(SIGABRT) -> recovery; a returning claim
   decider -> the abort wrlock path -> SIG_DFL reset + re-raise -> death;
   and the abort-lock interplay arm (a thread blocked in abort()'s
   wrlock window while another thread's raise runs the raw handler's
   default-action reset).  The N3924 API under test is declared in the
   installed <signal.h> (__USE_GNU) and implemented in libc by the wg14
   subdir.
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
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <threads.h>
#include <unistd.h>

#include <support/check.h>

static int recovered;
static int recovered_signo;

static enum sig_decision
abort_frame_decider (struct stdc_siginfo *rsi)
{
  (void) rsi;
  return sig_decision_call_recovery;
}

static union stdc_siginfo_value
abort_recovery (const struct stdc_siginfo *rsi)
{
  recovered++;
  recovered_signo = rsi->signo;
  return rsi->value;
}

static union stdc_siginfo_value
call_abort (union stdc_siginfo_value value)
{
  abort ();
  return value;
}

static enum sig_decision
claiming_decider (struct stdc_siginfo *rsi)
{
  (void) rsi;
  return sig_decision_resume_execution;
}

static int
check_dead_by_sigabrt (pid_t pid)
{
  int status = 0;
  TEST_COMPARE (waitpid (pid, &status, 0), pid);
  TEST_VERIFY (WIFSIGNALED (status));
  TEST_COMPARE (WTERMSIG (status), SIGABRT);
  return 0;
}

static void
alarm_guard (void)
{
  /* A hang in any arm (the wrlock window analysis is the thing under
     test) must terminate the test instead of stalling the harness.  */
  alarm (120);
}

static int
blocked_abort_thr (void *unused)
{
  (void) unused;
  sigset_t mask;
  sigemptyset (&mask);
  sigaddset (&mask, SIGABRT);
  pthread_sigmask (SIG_BLOCK, &mask, NULL);
  abort ();
  return 0;
}

static int
raise_sigabrt_thr (void *unused)
{
  (void) unused;
  /* SIGABRT is unblocked here, so kill() delivers into this thread's
     raw handler while the aborter holds the abort wrlock.  */
  kill (getpid (), SIGABRT);
  return 0;
}

static int
do_test (void)
{
  alarm_guard ();

  /* Arm 1: abort() with no siginstall -> the plain SIGABRT death.  */
  {
    pid_t pid = fork ();
    TEST_VERIFY (pid >= 0);
    if (pid == 0)
      {
	alarm (60);
	abort ();
      }
    check_dead_by_sigabrt (pid);
  }

  /* Arm 3: abort() with SIGABRT siginstall'ed and a returning claim
     decider -> the raise is claimed and returns, abort() takes the
     wrlock, resets SIG_DFL and re-raises -> SIGABRT death.  */
  {
    pid_t pid = fork ();
    TEST_VERIFY (pid >= 0);
    if (pid == 0)
      {
	alarm (60);
	sigset_t s;
	sigemptyset (&s);
	sigaddset (&s, SIGABRT);
	void *h = siginstall (&s);
	if (h == NULL)
	  _exit (42);
	void *d = signal_decider_create (&s, false, claiming_decider,
					 (union stdc_siginfo_value){ 0 });
	if (d == NULL)
	  _exit (43);
	abort ();
	_exit (44); /* Unreachable: abort() must not return.  */
      }
    check_dead_by_sigabrt (pid);
  }

  /* Arm 4: the abort-lock interplay.  Thread A blocks SIGABRT and calls
     abort() (its first raise stays pending, A takes the abort wrlock,
     resets SIG_DFL and re-raises); thread B concurrently raises SIGABRT
     into the installed raw handler whose default-action reset contends
     with A's wrlock.  The process must terminate promptly - the wrlock
     window A holds is only between the first raise's return and the
     SIG_DFL re-raise (plans/glibc-fork.md "Comprehensive review" item
     1).  */
  {
    pid_t pid = fork ();
    TEST_VERIFY (pid >= 0);
    if (pid == 0)
      {
	alarm (60);
	sigset_t s;
	sigemptyset (&s);
	sigaddset (&s, SIGABRT);
	void *h = siginstall (&s);
	if (h == NULL)
	  _exit (42);

	thrd_t aborter, raiser;
	TEST_COMPARE (thrd_create (&aborter, blocked_abort_thr, NULL),
		      thrd_success);
	TEST_COMPARE (thrd_create (&raiser, raise_sigabrt_thr, NULL),
		      thrd_success);
	thrd_join (aborter, NULL);
	thrd_join (raiser, NULL);
	_exit (45); /* Unreachable: the process must have died.  */
      }
    /* The expected death is by SIGABRT (either thread's re-raise).  */
    check_dead_by_sigabrt (pid);
  }

  /* Arm 2 (in-process): abort() inside a sigguarded(SIGABRT) frame is
     recovered - the frame decider claims call_recovery and the longjmp
     unwinds back into sigguarded(), so abort() never returns and the
     abort wrlock is never taken.  */
  {
    recovered = 0;
    sigset_t s;
    sigemptyset (&s);
    sigaddset (&s, SIGABRT);
    void *h = siginstall (&s);
    TEST_VERIFY (h != NULL);
    if (h == NULL)
      return 1;
    union stdc_siginfo_value value = { .int_value = 7 };
    const union stdc_siginfo_value result = sigguarded (
	&s, call_abort, abort_recovery, abort_frame_decider, value);
    TEST_COMPARE (recovered, 1);
    TEST_COMPARE (recovered_signo, SIGABRT);
    TEST_COMPARE (result.int_value, 7);
    /* The wrlock machinery is fully released after the recovery: a
       SIGABRT query still works.  */
    struct sigaction q;
    memset (&q, 0, sizeof (q));
    TEST_COMPARE (sigaction (SIGABRT, NULL, &q), 0);
    TEST_COMPARE (siguninstall (h), 0);
  }

  alarm (0);
  return 0;
}

#include <support/test-driver.c>