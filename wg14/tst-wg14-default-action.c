/* Port of wg14_signals siginstall_default_action_test.c to glibc's
   harness, with the siginfo-preserving SIGSEGV handoff REQUIRED (the
   LLVM consumer's default-action callback re-delivers with the original
   siginfo via SYS_rt_tgsigqueueinfo so the core dump keeps the faulting
   address - plans/glibc-fork.md "Consumer validation").  The N3924 API
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
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include <support/check.h>

/* The glibc-native shape of the LLVM consumer's default-action callback:
   reset the kernel disposition to SIG_DFL, then re-deliver with the
   original siginfo preserved (Linux SYS_rt_tgsigqueueinfo), falling back
   to a plain self-delivery.  Async-signal-safe throughout.  */
static void
siginfo_preserving_cb (int signo, siginfo_t *raw_info, void *raw_context)
{
  (void) raw_context;
  struct sigaction dfl;
  memset (&dfl, 0, sizeof (dfl));
  dfl.sa_handler = SIG_DFL;
  sigemptyset (&dfl.sa_mask);
  sigaction (signo, &dfl, NULL);
  if (raw_info != NULL)
    syscall (SYS_rt_tgsigqueueinfo, getpid (), syscall (SYS_gettid), signo,
	     raw_info);
  else
    raise (signo);
}

static int callback_calls;
static int callback_signo;
static intptr_t callback_info_addr;

static void
default_action_cb (int signo, siginfo_t *info, void *context)
{
  (void) context;
  callback_calls++;
  callback_signo = signo;
  callback_info_addr = (intptr_t) info;
}

static void
child_provoke_segv (void)
{
  (void) siginstall_set_default_action_np (siginfo_preserving_cb);
  sigset_t guarded;
  sigemptyset (&guarded);
  sigaddset (&guarded, SIGSEGV);
  if (siginstall (&guarded) == NULL)
    _exit (42);
  /* A genuine fault: the raw handler runs, no decider claims, the
     SIG_DFL default-action callback re-delivers with the original
     siginfo (si_addr == 1), and the child dies by SIGSEGV.  */
  *(volatile int *) (uintptr_t) 1 = 1;
  _exit (43); /* The SIG_DFL handoff must have killed us.  */
}

static int
do_test (void)
{
  /* Phase 1: a custom default-action callback is invoked with the
     original siginfo in place of the built-in reset-and-re-raise for a
     raise falling through to a SIG_DFL disposition.  */
  {
    callback_calls = 0;
    TEST_COMPARE (siginstall_set_default_action_np (default_action_cb), 0);
    sigset_t guarded;
    sigemptyset (&guarded);
    sigaddset (&guarded, SIGUSR2);
    void *handlers = siginstall (&guarded);
    TEST_VERIFY (handlers != NULL);
    if (handlers == NULL)
      return 1;
    siginfo_t info;
    memset (&info, 0, sizeof (info));
    info.si_signo = SIGUSR2;
    info.si_code = SI_USER;
    TEST_VERIFY (stdc_raise (SIGUSR2, &info, NULL) == true);
    TEST_COMPARE (callback_calls, 1);
    TEST_COMPARE (callback_signo, SIGUSR2);
    TEST_COMPARE (callback_info_addr, (intptr_t) &info);
    /* The callback ran instead of a reset-and-re-raise, so the library's
       handler is still installed and the process survived SIGUSR2.  */
    struct sigaction installed;
    memset (&installed, 0, sizeof (installed));
    TEST_COMPARE (sigaction (SIGUSR2, NULL, &installed), 0);
    TEST_VERIFY (installed.sa_sigaction != NULL);
    TEST_COMPARE (siguninstall (handlers), 0);

    /* Phase 2: resetting to NULL restores the built-in default action;
       SIGCONT's default never terminates and the built-in
       reset-and-re-raise preserves the installed handler.  */
    TEST_COMPARE (siginstall_set_default_action_np (NULL), 0);
    sigset_t guarded2;
    sigemptyset (&guarded2);
    sigaddset (&guarded2, SIGCONT);
    void *handlers2 = siginstall (&guarded2);
    TEST_VERIFY (handlers2 != NULL);
    if (handlers2 == NULL)
      return 1;
    TEST_VERIFY (stdc_raise (SIGCONT, NULL, NULL) == true);
    TEST_COMPARE (callback_calls, 1); /* The custom callback was not
					 invoked again.  */
    struct sigaction after;
    memset (&after, 0, sizeof (after));
    TEST_COMPARE (sigaction (SIGCONT, NULL, &after), 0);
    TEST_VERIFY (after.sa_sigaction != NULL);
    TEST_COMPARE (siguninstall (handlers2), 0);
  }

  /* Phase 3 (REQUIRED): the siginfo-preserving handoff.  A genuine
     SIGSEGV is re-delivered by the default-action callback with the
     original siginfo, so the death's si_addr matches the faulting
     address.  */
  {
    pid_t pid = fork ();
    TEST_VERIFY (pid >= 0);
    if (pid == 0)
      child_provoke_segv ();
    siginfo_t sinfo;
    memset (&sinfo, 0, sizeof (sinfo));
    TEST_COMPARE (waitid (P_PID, pid, &sinfo, WEXITED), 0);
    TEST_VERIFY (WIFSIGNALED (sinfo.si_status));
    TEST_COMPARE (WTERMSIG (sinfo.si_status), SIGSEGV);
    TEST_VERIFY (sinfo.si_addr == (void *) (uintptr_t) 1);
  }

  return 0;
}

#include <support/test-driver.c>