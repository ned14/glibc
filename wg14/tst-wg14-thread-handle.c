/* Port of wg14_signals thrd_signal_handle_test.c to glibc's harness:
   thread-local guarding, global deciders, and concurrent destroy of a
   global decider while a raise is in flight.  The N3924 API under test
   is declared in the installed <signal.h> (__USE_GNU) and implemented in
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
#include <stdatomic.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>

#include <support/check.h>

#define SIGNAL_TO_USE SIGILL

struct shared_t
{
  int count_decider, count_recovery;
  atomic_int latch;
};

/* Recovery: the thread-local sigguarded() frame recovers via longjmp.  */
static union stdc_siginfo_value
sigill_recovery_func (const struct stdc_siginfo *rsi)
{
  struct shared_t *shared = (struct shared_t *) rsi->value.ptr_value;
  shared->count_recovery++;
  return rsi->value;
}

static enum sig_decision
sigill_decider_func (struct stdc_siginfo *rsi)
{
  struct shared_t *shared = (struct shared_t *) rsi->value.ptr_value;
  shared->count_decider++;
  if (atomic_load_explicit (&shared->latch, memory_order_acquire) == 1)
    {
      /* Wait here until the other thread destroys this decider.  */
      atomic_store_explicit (&shared->latch, 2, memory_order_release);
      while (atomic_load_explicit (&shared->latch, memory_order_acquire) == 2)
	;
    }
  return sig_decision_call_recovery; /* handled.  */
}

static union stdc_siginfo_value
sigill_func (union stdc_siginfo_value value)
{
  (void) stdc_raise (SIGNAL_TO_USE, NULL, NULL);
  return value;
}

static int
sigill_thread (void *arg)
{
  (void) arg;
  return stdc_raise (SIGNAL_TO_USE, NULL, NULL);
}

/* Nested kernel delivery (port of nested_decider_rsi_test.c): the first
   (software) activation of the frame decider issues a REAL kill(), which
   the kernel delivers into the installed raw handler (SA_NODEFER leaves
   the signal unblocked inside the handler); the nested delivery runs the
   same thread-local frame's decider again, each activation with its own
   rsi - the outer (software, NULL raw_info) must not be clobbered by the
   nested (kernel siginfo) activation.  */
static atomic_int outer_decider_calls;
static atomic_int nested_decider_calls;
static volatile int outer_saw_nested_info;
static volatile int nested_saw_bad_info;

static enum sig_decision
nested_rsi_decider (struct stdc_siginfo *rsi)
{
  if (atomic_fetch_add (&outer_decider_calls, 1) == 0)
    {
      /* Outer activation: software raise with NULL info.  */
      if (rsi->raw_info != NULL)
	outer_saw_nested_info = 2;
      /* Re-enter the same frame's decider via a real delivery; the
	 kernel siginfo is what the nested activation must carry.  */
      kill (getpid (), SIGNAL_TO_USE);
      if (rsi->raw_info != NULL)
	outer_saw_nested_info = 1;
      return sig_decision_resume_execution;
    }
  /* Nested activation: real signal delivery, kernel siginfo present.  */
  atomic_fetch_add (&nested_decider_calls, 1);
  if (rsi->raw_info == NULL || rsi->signo != SIGNAL_TO_USE)
    nested_saw_bad_info = 1;
  return sig_decision_resume_execution;
}

static int
do_test (void)
{
  void *handlers = siginstall (NULL);
  TEST_VERIFY (handlers != NULL);
  if (handlers == NULL)
    return 1;

  /* Thread-local handling: a raise inside a sigguarded() frame is claimed
       by the frame decider and recovered via the frame recovery.  */
  {
    struct shared_t shared
	= { .count_decider = 0, .count_recovery = 0, .latch = 0 };
    union stdc_siginfo_value value = { .ptr_value = &shared };
    sigset_t guarded;
    sigemptyset (&guarded);
    sigaddset (&guarded, SIGNAL_TO_USE);
    (void) sigguarded (&guarded, sigill_func, sigill_recovery_func,
		       sigill_decider_func, value);
    TEST_COMPARE (shared.count_decider, 1);
    TEST_COMPARE (shared.count_recovery, 1);
  }

  /* Nested kernel delivery arm: each activation of the frame decider
     gets its own rsi; the outer (software) activation must still see
     raw_info == NULL after the nested (real, kernel-siginfo) delivery
     ran inside it.  */
  {
    atomic_store_explicit (&outer_decider_calls, 0, memory_order_relaxed);
    atomic_store_explicit (&nested_decider_calls, 0, memory_order_relaxed);
    outer_saw_nested_info = 0;
    nested_saw_bad_info = 0;
    sigset_t guarded;
    sigemptyset (&guarded);
    sigaddset (&guarded, SIGNAL_TO_USE);
    union stdc_siginfo_value value = { .int_value = 7 };
    const union stdc_siginfo_value result
	= sigguarded (&guarded, sigill_func, sigill_recovery_func,
		      nested_rsi_decider, value);
    TEST_COMPARE (result.int_value, 7);
    TEST_COMPARE (atomic_load (&outer_decider_calls), 1);
    TEST_COMPARE (atomic_load (&nested_decider_calls), 1);
    TEST_COMPARE (outer_saw_nested_info, 0);
    TEST_COMPARE (nested_saw_bad_info, 0);
  }

  /* Global handling: a raise with no guard frame is claimed by the global
     decider; there is no recovery (a global decider has no frame to
     longjmp to).  */
  {
    struct shared_t shared
	= { .count_decider = 0, .count_recovery = 0, .latch = 0 };
    union stdc_siginfo_value value = { .ptr_value = &shared };
    sigset_t guarded;
    sigemptyset (&guarded);
    sigaddset (&guarded, SIGNAL_TO_USE);
    void *sigill_decider
	= signal_decider_create (&guarded, false, sigill_decider_func, value);
    TEST_VERIFY (sigill_decider != NULL);
    TEST_VERIFY (stdc_raise (SIGNAL_TO_USE, NULL, NULL) == true);
    TEST_COMPARE (shared.count_decider, 1);
    TEST_COMPARE (shared.count_recovery, 0);
    TEST_COMPARE (signal_decider_destroy (sigill_decider), 0);
  }

  /* Concurrent destroy of a global decider whilst a raise is in flight:
     the decider completes its execution and the registry entry is
     released after the raise completes.  Synchronised by the latch, no
     sleeps.  */
  {
    struct shared_t shared
	= { .count_decider = 0, .count_recovery = 0, .latch = 1 };
    union stdc_siginfo_value value = { .ptr_value = &shared };
    sigset_t guarded;
    sigemptyset (&guarded);
    sigaddset (&guarded, SIGNAL_TO_USE);
    void *sigill_decider
	= signal_decider_create (&guarded, false, sigill_decider_func, value);
    TEST_VERIFY (sigill_decider != NULL);

    thrd_t th;
    TEST_COMPARE (thrd_create (&th, sigill_thread, &shared), thrd_success);
    /* The decider parks inside the raise until the main thread destroys
       it (the latch arm); then the raise completes.  */
    while (atomic_load_explicit (&shared.latch, memory_order_acquire) != 2)
      ;
    TEST_COMPARE (signal_decider_destroy (sigill_decider), 0);
    atomic_store_explicit (&shared.latch, 0, memory_order_release);
    int res = 0;
    TEST_COMPARE (thrd_join (th, &res), thrd_success);
    TEST_COMPARE (res, 1);
    TEST_COMPARE (shared.count_decider, 1);
    TEST_COMPARE (shared.count_recovery, 0);
  }

  TEST_COMPARE (siguninstall (handlers), 0);
  return 0;
}

#include <support/test-driver.c>