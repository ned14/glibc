/* Port of wg14_signals guard_abandon_test.c to glibc's harness: the
   sigdecider_abandon() / sigdecider_abandon_resume() pair for both
   thread-local frame deciders and global deciders.  The N3924 API under
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

#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>

#include <support/check.h>

#define SIGNAL_TO_USE SIGILL

static int global_decider_calls;

static enum sig_decision
global_abandon_resume_decider (struct stdc_siginfo *rsi)
{
  global_decider_calls++;
  sigdecider_abandon (rsi);
  sigdecider_abandon_resume (rsi);
  return sig_decision_resume_execution;
}

static int global_abandon_calls;
static jmp_buf global_abandon_env;

static enum sig_decision
global_abandon_never_returns_decider (struct stdc_siginfo *rsi)
{
  global_abandon_calls++;
  sigdecider_abandon (rsi);
  longjmp (global_abandon_env, 1);
}

static int frame_decider_calls;

static enum sig_decision
flicker_decider (struct stdc_siginfo *rsi)
{
  frame_decider_calls++;
  sigdecider_abandon (rsi);
  sigdecider_abandon_resume (rsi);
  return sig_decision_resume_execution;
}

static int raise_result1;
static int raise_result2;

static union stdc_siginfo_value
guarded_calls_raise (union stdc_siginfo_value value)
{
  raise_result1 = sighandle (SIGNAL_TO_USE, NULL, NULL);
  raise_result2 = sighandle (SIGNAL_TO_USE, NULL, NULL);
  return value;
}

static union stdc_siginfo_value
noop_recovery (const struct stdc_siginfo *rsi)
{
  return rsi->value;
}

static union stdc_siginfo_value
single_raise (union stdc_siginfo_value value)
{
  (void) sighandle (SIGNAL_TO_USE, NULL, NULL);
  return value;
}

static int frame_abandon_calls;
static jmp_buf frame_abandon_env;

static enum sig_decision
frame_abandon_never_returns_decider (struct stdc_siginfo *rsi)
{
  frame_abandon_calls++;
  sigdecider_abandon (rsi);
  longjmp (frame_abandon_env, 1);
}

static int fresh_decider_calls;
static int fresh_recovery_calls;

static enum sig_decision
fresh_claiming_decider (struct stdc_siginfo *rsi)
{
  (void) rsi;
  fresh_decider_calls++;
  return sig_decision_call_recovery;
}

static union stdc_siginfo_value
fresh_counting_recovery (const struct stdc_siginfo *rsi)
{
  fresh_recovery_calls++;
  return rsi->value;
}

static int flicker_recover_calls;
static int raise_result5;

static enum sig_decision
flicker_then_recover_decider (struct stdc_siginfo *rsi)
{
  flicker_recover_calls++;
  sigdecider_abandon (rsi);
  sigdecider_abandon (rsi);
  sigdecider_abandon_resume (rsi);
  sigdecider_abandon_resume (rsi);
  if (flicker_recover_calls >= 2)
    return sig_decision_call_recovery;
  return sig_decision_resume_execution;
}

static int nested_inner_abandon_calls;
static int outer_decider_calls;
static int outer_recovery_calls;
static int inner_abandon_failed;
static jmp_buf nested_abandon_env;

static enum sig_decision
nested_inner_abandon_decider (struct stdc_siginfo *rsi)
{
  nested_inner_abandon_calls++;
  sigdecider_abandon (rsi);
  longjmp (nested_abandon_env, 1);
}

static enum sig_decision
outer_claiming_decider (struct stdc_siginfo *rsi)
{
  (void) rsi;
  outer_decider_calls++;
  return sig_decision_call_recovery;
}

static union stdc_siginfo_value
outer_counting_recovery (const struct stdc_siginfo *rsi)
{
  outer_recovery_calls++;
  return rsi->value;
}

static union stdc_siginfo_value
nested_inner_guard (union stdc_siginfo_value value)
{
  sigset_t inner;
  sigemptyset (&inner);
  sigaddset (&inner, SIGNAL_TO_USE);
  union stdc_siginfo_value inner_value = { .int_value = 7 };
  (void) sigguarded (&inner, single_raise, noop_recovery,
		     nested_inner_abandon_decider, inner_value);
  return value;
}

static union stdc_siginfo_value
outer_guarded_body (union stdc_siginfo_value value)
{
  if (setjmp (nested_abandon_env) == 0)
    {
      nested_inner_guard (value);
      inner_abandon_failed = 1;
    }
  (void) sighandle (SIGNAL_TO_USE, NULL, NULL);
  return value;
}

static union stdc_siginfo_value
double_raise (union stdc_siginfo_value value)
{
  /* Only the first raise returns (resume_execution); the second raise's
     decider invokes recovery and never returns here.  */
  raise_result5 = sighandle (SIGNAL_TO_USE, NULL, NULL);
  (void) sighandle (SIGNAL_TO_USE, NULL, NULL);
  return value;
}

static int
do_test (void)
{
  sigset_t guarded;
  sigemptyset (&guarded);
  sigaddset (&guarded, SIGNAL_TO_USE);

  /* Global decider abandons and resumes in-call: the raise completes and
     the decider runs again on the next raise.  */
  {
    void *handlers = siginstall (&guarded);
    TEST_VERIFY (handlers != NULL);
    if (handlers == NULL)
      return 1;
    union stdc_siginfo_value value = { .int_value = 7 };
    void *decider1 = signal_decider_create (
	&guarded, false, global_abandon_resume_decider, value);
    TEST_VERIFY (decider1 != NULL);
    TEST_VERIFY (sighandle (SIGNAL_TO_USE, NULL, NULL) == true);
    TEST_COMPARE (global_decider_calls, 1);
    TEST_VERIFY (sighandle (SIGNAL_TO_USE, NULL, NULL) == true);
    TEST_COMPARE (global_decider_calls, 2);
    TEST_COMPARE (signal_decider_destroy (decider1), 0);
    TEST_COMPARE (siguninstall (handlers), 0);
  }

  /* Global decider abandons then never returns: the machinery is left at
     steady state and consults the decider again.  */
  {
    void *handlers = siginstall (&guarded);
    TEST_VERIFY (handlers != NULL);
    if (handlers == NULL)
      return 1;
    union stdc_siginfo_value value = { .int_value = 7 };
    void *decider = signal_decider_create (
	&guarded, false, global_abandon_never_returns_decider, value);
    TEST_VERIFY (decider != NULL);
    if (setjmp (global_abandon_env) == 0)
      {
	TEST_VERIFY (sighandle (SIGNAL_TO_USE, NULL, NULL) == true);
	TEST_VERIFY (0 && "global decider failed to abandon");
      }
    TEST_COMPARE (global_abandon_calls, 1);
    if (setjmp (global_abandon_env) == 0)
      {
	TEST_VERIFY (sighandle (SIGNAL_TO_USE, NULL, NULL) == true);
	TEST_VERIFY (0 && "global decider failed to abandon");
      }
    TEST_COMPARE (global_abandon_calls, 2);
    TEST_COMPARE (signal_decider_destroy (decider), 0);
    TEST_COMPARE (siguninstall (handlers), 0);
  }

  /* Frame decider abandons and resumes in-call: the guard stays
     consultable and pops normally on return.  */
  {
    const int before = frame_decider_calls;
    union stdc_siginfo_value value = { .int_value = 7 };
    const union stdc_siginfo_value guarded_ret = sigguarded (
	&guarded, guarded_calls_raise, noop_recovery, flicker_decider, value);
    TEST_VERIFY (raise_result1);
    TEST_VERIFY (raise_result2);
    TEST_COMPARE (guarded_ret.int_value, 7);
    TEST_COMPARE (frame_decider_calls, before + 2);
    TEST_VERIFY (sighandle (SIGNAL_TO_USE, NULL, NULL) == false);
    TEST_COMPARE (frame_decider_calls, before + 2);
  }

  /* Frame decider abandons then never returns: the popped frame is not
     consulted and a fresh guard still recovers.  */
  {
    frame_abandon_calls = 0;
    fresh_decider_calls = 0;
    fresh_recovery_calls = 0;
    union stdc_siginfo_value value = { .int_value = 7 };
    if (setjmp (frame_abandon_env) == 0)
      {
	(void) sigguarded (&guarded, single_raise, noop_recovery,
			   frame_abandon_never_returns_decider, value);
	TEST_VERIFY (0 && "frame decider failed to abandon");
      }
    TEST_COMPARE (frame_abandon_calls, 1);
    TEST_VERIFY (sighandle (SIGNAL_TO_USE, NULL, NULL) == false);
    TEST_COMPARE (frame_abandon_calls, 1);
    const union stdc_siginfo_value ret_value
	= sigguarded (&guarded, single_raise, fresh_counting_recovery,
		      fresh_claiming_decider, value);
    TEST_COMPARE (fresh_decider_calls, 1);
    TEST_COMPARE (fresh_recovery_calls, 1);
    TEST_COMPARE (ret_value.int_value, 7);
  }

  /* Abandon and resume is idempotent and a resumed frame still recovers
     by longjmp.  */
  {
    flicker_recover_calls = 0;
    raise_result5 = 0;
    union stdc_siginfo_value value = { .int_value = 7 };
    const union stdc_siginfo_value ret_value
	= sigguarded (&guarded, double_raise, noop_recovery,
		      flicker_then_recover_decider, value);
    TEST_VERIFY (raise_result5);
    TEST_COMPARE (flicker_recover_calls, 2);
    TEST_COMPARE (ret_value.int_value, 7);
    TEST_VERIFY (sighandle (SIGNAL_TO_USE, NULL, NULL) == false);
    TEST_COMPARE (flicker_recover_calls, 2);
  }

  /* Nested abandon with a live outer guard: the inner frame is dropped,
     the outer guard still recovers.  */
  {
    nested_inner_abandon_calls = 0;
    outer_decider_calls = 0;
    outer_recovery_calls = 0;
    inner_abandon_failed = 0;
    union stdc_siginfo_value value = { .int_value = 7 };
    const union stdc_siginfo_value ret_value
	= sigguarded (&guarded, outer_guarded_body, outer_counting_recovery,
		      outer_claiming_decider, value);
    TEST_VERIFY (!inner_abandon_failed);
    TEST_COMPARE (nested_inner_abandon_calls, 1);
    TEST_COMPARE (outer_decider_calls, 1);
    TEST_COMPARE (outer_recovery_calls, 1);
    TEST_COMPARE (ret_value.int_value, 7);
    TEST_VERIFY (sighandle (SIGNAL_TO_USE, NULL, NULL) == false);
    TEST_COMPARE (outer_decider_calls, 1);
  }

  return 0;
}

#include <support/test-driver.c>