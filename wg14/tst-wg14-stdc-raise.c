/* Port of the wg14_signals stdc_raise() test (stdc_raise_test.c) to glibc's
   harness.  stdc_raise() (N3924 7.14.2.10) behaves as if it called raise(),
   except that the caller's raw_info/raw_context is made available to the
   signal deciders; a NULL argument is replaced by a constructed
   implementation-defined default so a decider never sees a NULL raw_info or
   raw_context.  The N3924 API under test is declared in the installed
   <signal.h> (__USE_GNU) and implemented in libc by the wg14 subdir.
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
#include <string.h>

#include <support/check.h>

/* SIGILL is not a glibc-internal signal and is safe to raise for real.  */
#define TEST_SIGNAL SIGILL

static struct observed_t
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

static int
do_test (void)
{
  siginfo_t fake_info;
  ucontext_t fake_context;

  /* Not a valid signal number: unsuccessful, and no signal is raised
     (7.14.2.10 p8).  */
  errno = 0;
  TEST_VERIFY (stdc_raise (0, NULL, NULL) != 0);
  TEST_COMPARE (errno, EINVAL);
  errno = 0;
  TEST_VERIFY (stdc_raise (-1, NULL, NULL) != 0);
  TEST_COMPARE (errno, EINVAL);
  errno = 0;
  TEST_VERIFY (stdc_raise (NSIG, NULL, NULL) != 0);
  TEST_COMPARE (errno, EINVAL);

  /* A signal this library has not installed is raised for real as raise()
     would: no decider runs, and SIGCONT's default action for a running
     process is to do nothing.  */
  memset (&fake_info, 0, sizeof (fake_info));
  memset (&fake_context, 0, sizeof (fake_context));
  TEST_COMPARE (stdc_raise (SIGCONT, &fake_info, &fake_context), 0);

  union stdc_siginfo_value value = { .int_value = 7 };
  void *handlers = siginstall (NULL);
  TEST_VERIFY (handlers != NULL);
  if (handlers == NULL)
    return 1;
  /* The documented signo == 0 setup form.  */
  TEST_VERIFY (sighandle (0, NULL, NULL) == false);

  /* A global decider observing the raise sees exactly the caller-supplied
     raw_info and raw_context, not the kernel's own (7.14.2.10 p4).  */
  memset (&observed, 0, sizeof (observed));
  memset (&fake_info, 0, sizeof (fake_info));
  fake_info.si_signo = TEST_SIGNAL;
  fake_info.si_code = SI_QUEUE;
  fake_info.si_errno = 3;
  fake_info.si_addr = (void *) (intptr_t) 0x5678;
  memset (&fake_context, 0, sizeof (fake_context));
  sigset_t guarded;
  sigemptyset (&guarded);
  sigaddset (&guarded, TEST_SIGNAL);
  void *decider
      = signal_decider_create (&guarded, false, capture_decider, value);
  TEST_VERIFY (decider != NULL);
  TEST_COMPARE (stdc_raise (TEST_SIGNAL, &fake_info, &fake_context), 0);
  TEST_COMPARE (observed.calls, 1);
  TEST_COMPARE (observed.signo, TEST_SIGNAL);
  TEST_VERIFY (observed.raw_info == &fake_info);
  TEST_VERIFY (observed.raw_context == &fake_context);
  TEST_COMPARE (observed.error_code, fake_info.si_errno);
  TEST_VERIFY (observed.addr == fake_info.si_addr);
  TEST_VERIFY (signal_decider_destroy (decider) == 0);

  /* NULL raw_info and raw_context are replaced by a constructed default, so
     the decider sees a non-NULL raw_info (with si_signo set and si_code
     SI_USER) and a non-NULL raw_context, error_code zero and addr NULL
     (7.14.2.10 p5/p6/p7).  */
  memset (&observed, 0, sizeof (observed));
  decider = signal_decider_create (&guarded, false, capture_decider, value);
  TEST_VERIFY (decider != NULL);
  TEST_COMPARE (stdc_raise (TEST_SIGNAL, NULL, NULL), 0);
  TEST_COMPARE (observed.calls, 1);
  TEST_VERIFY (observed.raw_info != NULL);
  TEST_VERIFY (observed.raw_context != NULL);
  TEST_COMPARE (observed.raw_si_signo, TEST_SIGNAL);
  TEST_COMPARE (observed.raw_si_code, SI_USER);
  TEST_COMPARE (observed.error_code, 0);
  TEST_VERIFY (observed.addr == NULL);
  TEST_VERIFY (signal_decider_destroy (decider) == 0);

  /* A NULL raw_info with a caller raw_context: the context is still surfaced,
     and the default signal information is constructed as above.  */
  memset (&observed, 0, sizeof (observed));
  memset (&fake_context, 0, sizeof (fake_context));
  decider = signal_decider_create (&guarded, false, capture_decider, value);
  TEST_VERIFY (decider != NULL);
  TEST_COMPARE (stdc_raise (TEST_SIGNAL, NULL, &fake_context), 0);
  TEST_COMPARE (observed.calls, 1);
  TEST_VERIFY (observed.raw_info != NULL);
  TEST_VERIFY (observed.raw_context == &fake_context);
  TEST_COMPARE (observed.error_code, 0);
  TEST_VERIFY (observed.addr == NULL);
  TEST_VERIFY (signal_decider_destroy (decider) == 0);

  TEST_VERIFY (siguninstall (handlers) == 0);
  return 0;
}

#include <support/test-driver.c>
