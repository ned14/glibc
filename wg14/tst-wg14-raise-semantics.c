/* Port of the wg14_signals sighandle tests (sighandle_zero_test.c,
   sighandle_uninstalled_test.c, sighandle_null_info_test.c,
   out_of_range_signo_test.c) to glibc's harness.  The N3924 API under
   test is declared in the installed <signal.h> (__USE_GNU) and
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

/* Deliberately distinct from SIGNAL_TO_USE; SIGTERM is defined on every
   POSIX target.  */
#define OTHER_SIGNAL SIGTERM

static int guarded_raise_returned_false;
static int neg_returned_false;
static int pos_returned_false;
static int far_returned_false;
static int frame_decider_calls;
static int sanity_claimed;

static enum sig_decision
frame_decider (struct stdc_siginfo *rsi)
{
  (void) rsi;
  frame_decider_calls++;
  return sig_decision_resume_execution;
}

static union stdc_siginfo_value
noop_recovery (const struct stdc_siginfo *rsi)
{
  return rsi->value;
}

static union stdc_siginfo_value
guarded_func (union stdc_siginfo_value value)
{
  /* Inside a sigguarded() frame guarding a DIFFERENT signal, a raise of an
     uninstalled signal must return false too, not terminate the process
     (wg14_signals analysis.md 2.16/W5).  */
  guarded_raise_returned_false = (sighandle (SIGILL, NULL, NULL) == false);
  return value;
}

/* Out-of-range and negative signos must be rejected before the POSIX frame
   walk: all three raises must return false - the documented "no decider
   installed for that signal" - WITHOUT calling the frame decider
   (wg14_signals analysis.md NEGS).  */
static union stdc_siginfo_value
guarded_raises_out_of_range (union stdc_siginfo_value value)
{
  sanity_claimed = sighandle (SIGILL, NULL, NULL);
  frame_decider_calls = 0;
  neg_returned_false = !sighandle (-1, NULL, NULL);
  pos_returned_false = !sighandle (NSIG, NULL, NULL);
  far_returned_false = !sighandle (NSIG + 10, NULL, NULL);
  return value;
}

static struct decider_observed_t
{
  int calls;
  int signo;
  stdc_siginfo_error_code_t error_code;
  void *addr;
  void *raw_info;
  void *raw_context;
  int raw_si_signo;
  int raw_si_code;
} observed;

static enum sig_decision
capture_decider (struct stdc_siginfo *rsi)
{
  observed.calls++;
  observed.signo = rsi->signo;
  observed.error_code = rsi->error_code;
  observed.addr = rsi->addr;
  observed.raw_info = rsi->raw_info;
  observed.raw_context = rsi->raw_context;
  if (rsi->raw_info != NULL)
    {
      observed.raw_si_signo = ((siginfo_t *) rsi->raw_info)->si_signo;
      observed.raw_si_code = ((siginfo_t *) rsi->raw_info)->si_code;
    }
  return sig_decision_resume_execution;
}

static union stdc_siginfo_value
raise_with_info (union stdc_siginfo_value value)
{
  /* First raise supplies a real siginfo, second a NULL info: the second
     decider call in the same frame must observe a freshly constructed
     default, not the first raise's raw_info pointer (wg14_signals
     analysis.md 2.14/W2, N3924 7.14.2.9 p7).  */
  siginfo_t fake_info;
  memset (&fake_info, 0, sizeof (fake_info));
  fake_info.si_addr = (void *) (intptr_t) 0x1234;
  (void) sighandle (SIGILL, &fake_info, NULL);
  (void) sighandle (SIGILL, NULL, NULL);
  return value;
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
  /* The documented one-line library setup call: it must initialise the
     calling thread's per-thread state and return false doing nothing else
     (thrd_signal_handle.h, sighandle(signo == 0) setup form).  */
  TEST_VERIFY (sighandle (0, NULL, NULL) == false);

  /* sighandle() of a signal with NO siginstall() ever performed returns
     false immediately and must not terminate the process.  */
  TEST_VERIFY (sighandle (SIGILL, NULL, NULL) == false);

  /* A supported signo with no handler installed and a siginstall() of an
     unrelated signal done: still false.  */
  sigset_t base;
  sigemptyset (&base);
  sigaddset (&base, OTHER_SIGNAL);
  void *handlers = siginstall (&base);
  TEST_VERIFY (handlers != NULL);

  /* Same raise inside a sigguarded() frame guarding a different signal:
     false, and the frame decider is not consulted (guarded_func raises
     SIGILL, guarded by OTHER_SIGNAL).  */
  sigset_t guarded;
  sigemptyset (&guarded);
  sigaddset (&guarded, OTHER_SIGNAL);
  union stdc_siginfo_value value = { .int_value = 7 };
  (void) sigguarded (&guarded, guarded_func, noop_recovery, frame_decider,
		     value);
  TEST_VERIFY (guarded_raise_returned_false);
  TEST_VERIFY (frame_decider_calls == 0);

  /* Out-of-range signo rejection inside a frame guarding the same
     signal: the frame decider must not be called for -1/NSIG/NSIG+10,
     while a valid raise is claimed.  */
  frame_decider_calls = 0;
  sigemptyset (&guarded);
  sigaddset (&guarded, SIGILL);
  TEST_VERIFY (siginstall (&guarded) != NULL);
  value.int_value = 7;
  (void) sigguarded (&guarded, guarded_raises_out_of_range, noop_recovery,
		     frame_decider, value);
  TEST_VERIFY (sanity_claimed);
  TEST_VERIFY (neg_returned_false);
  TEST_VERIFY (pos_returned_false);
  TEST_VERIFY (far_returned_false);
  TEST_VERIFY (frame_decider_calls == 0);

  /* NULL-info raises hand the decider a constructed implementation-defined
     default (N3924 7.14.2.9 p7/p8): raw_info/raw_context are never NULL, and
     a constructed default siginfo carries si_signo = signo and
     si_code = SI_USER.  This must hold both through the global-decider path
     and through the frame path (wg14_signals analysis.md 2.14/W2).  */
  memset (&observed, 0, sizeof (observed));
  sigset_t g;
  sigemptyset (&g);
  sigaddset (&g, SIGILL);
  void *decider = signal_decider_create (&g, false, capture_decider, value);
  TEST_VERIFY (decider != NULL);
  TEST_VERIFY (sighandle (SIGILL, NULL, NULL) == true);
  TEST_COMPARE (observed.calls, 1);
  TEST_COMPARE (observed.signo, SIGILL);
  TEST_COMPARE (observed.error_code, 0);
  TEST_VERIFY (observed.addr == NULL);
  TEST_VERIFY (observed.raw_info != NULL);
  TEST_VERIFY (observed.raw_context != NULL);
  TEST_COMPARE (observed.raw_si_signo, SIGILL);
  TEST_COMPARE (observed.raw_si_code, SI_USER);
  /* The NULL raw_context was replaced by a context captured from the calling
     frame, not a zeroed object (7.14.2.9 p8); the object lives in the
     per-thread state, so it is still valid here after sighandle() returned. */
  {
    static const unsigned char zero_context[sizeof (stdc_siginfo_context_t)];
    TEST_VERIFY (memcmp (observed.raw_context, zero_context,
			 sizeof (stdc_siginfo_context_t))
		 != 0);
  }
  TEST_VERIFY (signal_decider_destroy (decider) == 0);

  memset (&observed, 0, sizeof (observed));
  (void) sigguarded (&guarded, raise_with_info, noop_recovery, capture_decider,
		     value);
  TEST_COMPARE (observed.calls, 2);
  TEST_COMPARE (observed.signo, SIGILL);
  TEST_COMPARE (observed.error_code, 0);
  TEST_VERIFY (observed.addr == NULL);
  TEST_VERIFY (observed.raw_info != NULL);
  TEST_VERIFY (observed.raw_context != NULL);
  TEST_COMPARE (observed.raw_si_signo, SIGILL);
  TEST_COMPARE (observed.raw_si_code, SI_USER);

  /* Sanity: once SIGILL is installed (from the out-of-range arm) and a
     claiming decider exists, sighandle must still return true.  */
  void *decider2 = signal_decider_create (&g, false, claiming_decider, value);
  TEST_VERIFY (decider2 != NULL);
  TEST_VERIFY (sighandle (SIGILL, NULL, NULL) == true);
  TEST_VERIFY (signal_decider_destroy (decider2) == 0);

  TEST_VERIFY (siguninstall (handlers) == 0);
  return 0;
}

#include <support/test-driver.c>