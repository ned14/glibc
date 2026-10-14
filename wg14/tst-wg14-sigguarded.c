/* Port of the wg14_signals sigguarded tests (recovery_null_loop_test.c,
   sigguarded_tss_init_test.c) plus the SIGGUARDED_FAILURE_VALUE sentinel
   check, to glibc's harness.  The N3924 API under test is declared in
   the installed <signal.h> (__USE_GNU) and implemented in libc by the
   wg14 subdir.
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <support/check.h>

/* A store to this address faults on every supported platform.  Volatile
   and opaque so the compiler cannot fold it into a null pointer constant
   and warn, or elide the store.  */
static volatile uintptr_t bad_address;

struct shared_t
{
  volatile int inner_decider_calls;
  int outer_decider_calls;
  int outer_recovery_calls;
  int recovered_signo;
};

/* The inner decider asks to invoke recovery, but the inner sigguarded
   has no recovery function; the raise must fall through to the outer
   frame instead of re-faulting forever (wg14_signals analysis.md 1.7).
   Detect the loop and terminate the test fast instead of hanging.  */
static enum sig_decision
inner_decider_func (struct stdc_siginfo *rsi)
{
  struct shared_t *shared = (struct shared_t *) rsi->value.ptr_value;
  if (++shared->inner_decider_calls > 100)
    _exit (2); /* 1.7 infinite fault loop detected.  */
  return sig_decision_call_recovery;
}

static enum sig_decision
outer_decider_func (struct stdc_siginfo *rsi)
{
  struct shared_t *shared = (struct shared_t *) rsi->value.ptr_value;
  shared->outer_decider_calls++;
  return sig_decision_call_recovery;
}

static union stdc_siginfo_value
outer_recovery_func (const struct stdc_siginfo *rsi)
{
  struct shared_t *shared = (struct shared_t *) rsi->value.ptr_value;
  shared->outer_recovery_calls++;
  shared->recovered_signo = rsi->signo;
  return rsi->value;
}

/* Trigger a genuine synchronous fault.  The kernel re-executes this
   instruction whenever the library handler returns without recovering,
   which is exactly the 1.7 infinite fault loop.  */
#if defined __clang__
__attribute__ ((no_sanitize ("address", "undefined")))
#elif defined __GNUC__
__attribute__ ((no_sanitize_undefined))
#endif
static union stdc_siginfo_value
fault_func (union stdc_siginfo_value value)
{
  (void) value;
  *(volatile int *) bad_address = 1;
  return value;
}

/* The inner sigguarded guards SIGSEGV but supplies recovery == NULL, the
   1.7 trigger.  */
static union stdc_siginfo_value
call_inner (union stdc_siginfo_value value)
{
  sigset_t guarded;
  sigemptyset (&guarded);
  sigaddset (&guarded, SIGSEGV);
  return sigguarded (&guarded, fault_func, NULL, inner_decider_func, value);
}

static int global_decider_called;
static enum sig_decision
declining_decider (struct stdc_siginfo *rsi)
{
  (void) rsi;
  return sig_decision_next_decider;
}

static enum sig_decision
claiming_decider (struct stdc_siginfo *rsi)
{
  (void) rsi;
  global_decider_called++;
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
  (void) sighandle (SIGSEGV, NULL, NULL);
  return value;
}

static int
do_test (void)
{
  /* The documented failure sentinel of sigguarded() (N3924 7.14.4.1,
     SIGGUARDED_FAILURE_VALUE): int_value == -99.  */
  TEST_COMPARE (SIGGUARDED_FAILURE_VALUE.int_value, (intptr_t) -99);

  /* 1.7 arm: an inner guard with a NULL recovery declines via
     call_recovery, and the fault must fall through to the outer frame
     whose recovery runs.  */
  {
    bad_address = 0;
    void *handlers = siginstall (NULL);
    TEST_VERIFY (handlers != NULL);
    if (handlers == NULL)
      return 1;
    struct shared_t shared = { 0 };
    union stdc_siginfo_value value = { .ptr_value = &shared };
    sigset_t guarded;
    sigemptyset (&guarded);
    sigaddset (&guarded, SIGSEGV);
    (void) sigguarded (&guarded, call_inner, outer_recovery_func,
		       outer_decider_func, value);
    TEST_COMPARE (shared.outer_recovery_calls, 1);
    TEST_COMPARE (shared.recovered_signo, SIGSEGV);
    TEST_COMPARE (shared.outer_decider_calls, 1);
    TEST_COMPARE (shared.inner_decider_calls, 1);
    TEST_COMPARE (siguninstall (handlers), 0);
  }

  /* sigguarded_tss_init arm: this thread's only library interaction is
     sigguarded(); a frame decider that declines must leave the raise to
     the global decider for the same signal, which claims it
     (wg14_signals analysis.md 2.19/X3).  */
  {
    global_decider_called = 0;
    sigset_t guarded;
    sigemptyset (&guarded);
    sigaddset (&guarded, SIGSEGV);
    void *handlers = siginstall (&guarded);
    TEST_VERIFY (handlers != NULL);
    if (handlers == NULL)
      return 1;
    union stdc_siginfo_value value = { .int_value = 7 };
    void *decider
	= signal_decider_create (&guarded, false, claiming_decider, value);
    TEST_VERIFY (decider != NULL);
    const union stdc_siginfo_value result = sigguarded (
	&guarded, guarded_func, noop_recovery, declining_decider, value);
    TEST_COMPARE (global_decider_called, 1);
    TEST_COMPARE (result.int_value, 7);
    TEST_COMPARE (signal_decider_destroy (decider), 0);
    TEST_COMPARE (siguninstall (handlers), 0);
  }

  return 0;
}

#include <support/test-driver.c>