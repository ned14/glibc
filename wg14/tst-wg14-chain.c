/* The glibc-native shape of the LLVM consumer contract that the llvm-libc
   fork's final commit established (plans/glibc-fork.md "Consumer
   validation"): np-setters before the first siginstall(), siginfo
   fidelity through the user-handler handoff, claim/decline/restore
   round-trips, and abort() re-entry recovering out of a
   sigguarded(SIGABRT) frame.  The N3924 API under test is declared in
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

#include <signal.h>
#include <string.h>

#include <support/check.h>

static volatile sig_atomic_t user_handler_calls;
static volatile int user_saw_signo;
static volatile int user_saw_si_code;
static volatile int user_saw_value;
static volatile int user_saw_pointer;
static siginfo_t user_info;

static void
user_handler (int signo, siginfo_t *si, void *ctx)
{
  (void) ctx;
  user_handler_calls++;
  user_saw_signo = (signo == SIGUSR1 && si != NULL && si->si_signo == SIGUSR1);
  user_saw_si_code = (si != NULL && si->si_code == SI_QUEUE);
  user_saw_value = (si != NULL && si->si_value.sival_int == 0x1234);
  user_saw_pointer = (si == &user_info);
}

/* The consumer's "handler kind" deciders: the install kind declines so
   the raise reaches the pre-LLVM user handler with the original
   siginfo; the claim kind resumes the interrupted code.  */
static enum sig_decision
declining_kind_decider (struct stdc_siginfo *rsi)
{
  (void) rsi;
  return sig_decision_next_decider;
}

static enum sig_decision
claiming_kind_decider (struct stdc_siginfo *rsi)
{
  sigfence (rsi->signo, rsi->value);
  return sig_decision_resume_execution;
}

static int abort_recovered;
static int abort_recovered_signo;

static enum sig_decision
abort_frame_decider (struct stdc_siginfo *rsi)
{
  (void) rsi;
  return sig_decision_call_recovery;
}

static union stdc_siginfo_value
abort_recovery (const struct stdc_siginfo *rsi)
{
  abort_recovered++;
  abort_recovered_signo = rsi->signo;
  return rsi->value;
}

static union stdc_siginfo_value
call_abort (union stdc_siginfo_value value)
{
  /* abort() raises SIGABRT via raise(); with SIGABRT siginstall'ed the
     kernel delivers into the filtering raw handler, the frame decider
     claims call_recovery, and the longjmp unwinds back into sigguarded()
     - abort() never returns.  */
  abort ();
  return value;
}

static int
do_test (void)
{
  /* Consumer ordering: np-setters run before the first siginstall() and
     apply to every later install.  SA_NOCLDWAIT is dropped by the
     embedder (tst-wg14-sa-flags-np verifies that in detail); the other
     flags are effective.  */
  TEST_COMPARE (siginstall_set_sa_flags_np (SA_SIGINFO | SA_NODEFER), 0);
  TEST_COMPARE (siginstall_set_default_action_np (NULL), 0);

  /* User-handler wrap/decline/restore round-trip.  */
  struct sigaction sa;
  memset (&sa, 0, sizeof (sa));
  sa.sa_sigaction = user_handler;
  sa.sa_flags = SA_SIGINFO;
  sigemptyset (&sa.sa_mask);
  TEST_COMPARE (sigaction (SIGUSR1, &sa, NULL), 0);

  sigset_t guarded;
  sigemptyset (&guarded);
  sigaddset (&guarded, SIGUSR1);
  void *handlers = siginstall (&guarded);
  TEST_VERIFY (handlers != NULL);
  if (handlers == NULL)
    return 1;

  memset (&user_info, 0, sizeof (user_info));
  user_info.si_signo = SIGUSR1;
  user_info.si_code = SI_QUEUE;
  user_info.si_value.sival_int = 0x1234;

  /* Decline: the raise falls through to the pre-existing SA_SIGINFO
     handler with the ORIGINAL siginfo preserved (raw_info is passed
     through when non-NULL).  */
  void *decline
      = signal_decider_create (&guarded, false, declining_kind_decider,
			       (union stdc_siginfo_value){ 0 });
  TEST_VERIFY (decline != NULL);
  TEST_VERIFY (stdc_raise (SIGUSR1, &user_info, NULL) == true);
  TEST_COMPARE (user_handler_calls, 1);
  TEST_VERIFY (user_saw_signo);
  TEST_VERIFY (user_saw_si_code);
  TEST_VERIFY (user_saw_value);
  TEST_VERIFY (user_saw_pointer);
  TEST_COMPARE (signal_decider_destroy (decline), 0);

  /* Claim: the decider resumes the interrupted code and the user handler
     is not consulted.  */
  void *claim = signal_decider_create (&guarded, false, claiming_kind_decider,
				       (union stdc_siginfo_value){ 0 });
  TEST_VERIFY (claim != NULL);
  TEST_VERIFY (stdc_raise (SIGUSR1, NULL, NULL) == true);
  TEST_COMPARE (user_handler_calls, 1);
  TEST_COMPARE (signal_decider_destroy (claim), 0);

  /* Restore: siguninstall puts the user handler back as the kernel
     disposition.  */
  TEST_COMPARE (siguninstall (handlers), 0);
  memset (&sa, 0, sizeof (sa));
  TEST_COMPARE (sigaction (SIGUSR1, NULL, &sa), 0);
  TEST_VERIFY ((sa.sa_flags & SA_SIGINFO) != 0);
  TEST_VERIFY (sa.sa_sigaction == user_handler);

  /* abort() re-entry: recovering out of a sigguarded(SIGABRT) frame.  */
  sigset_t abrt;
  sigemptyset (&abrt);
  sigaddset (&abrt, SIGABRT);
  void *abrt_handlers = siginstall (&abrt);
  TEST_VERIFY (abrt_handlers != NULL);
  union stdc_siginfo_value value = { .int_value = 7 };
  const union stdc_siginfo_value result = sigguarded (
      &abrt, call_abort, abort_recovery, abort_frame_decider, value);
  TEST_COMPARE (abort_recovered, 1);
  TEST_COMPARE (abort_recovered_signo, SIGABRT);
  TEST_COMPARE (result.int_value, 7);
  TEST_COMPARE (siguninstall (abrt_handlers), 0);

  /* Restore the np-setters to the defaults for any later tests in this
     binary.  */
  TEST_COMPARE (
      siginstall_set_sa_flags_np (SA_SIGINFO | SA_NOCLDWAIT | SA_NODEFER), 0);
  TEST_COMPARE (siginstall_set_default_action_np (NULL), 0);
  return 0;
}

#include <support/test-driver.c>