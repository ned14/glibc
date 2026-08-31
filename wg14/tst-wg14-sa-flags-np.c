/* Port of wg14_signals siginstall_sa_flags_test.c to glibc's harness,
   with the glibc-specific arms: SA_ONSTACK + sigaltstack delivers on the
   alt stack, and SA_NOCLDWAIT is permanently stripped by the embedder so
   POSIX wait()/zombie semantics survive.  The N3924 API under test is
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
#include <sys/wait.h>
#include <unistd.h>

#include <support/check.h>

/* SA_NOCLDWAIT is stripped by the embedder shim (wg14/embedder_shim.h)
   at every raw-handler install, so it must NEVER be reported back on
   glibc - the flag set via siginstall_set_sa_flags_np() is otherwise
   honoured exactly (plans/glibc-fork.md risks 2/3).  */
#define STRIPPED_FLAG SA_NOCLDWAIT

static int
check_installed_flags (int expected_present, int expected_absent)
{
  sigset_t guarded;
  sigemptyset (&guarded);
  sigaddset (&guarded, SIGUSR2);
  void *handlers = siginstall (&guarded);
  TEST_VERIFY (handlers != NULL);
  if (handlers == NULL)
    return 1;
  struct sigaction installed;
  memset (&installed, 0, sizeof (installed));
  TEST_COMPARE (sigaction (SIGUSR2, NULL, &installed), 0);
  TEST_VERIFY (installed.sa_sigaction != NULL);
  TEST_VERIFY ((installed.sa_flags & expected_present) == expected_present);
  TEST_VERIFY ((installed.sa_flags & expected_absent) == 0);
  TEST_COMPARE (siguninstall (handlers), 0);
  return 0;
}

struct alt_stack_t
{
  char *ss_sp;
  size_t ss_size;
  volatile int inside_alt_stack;
};

/* Runs inside the raw kernel handler (the SIGUSR1 delivery below is a
   real kill(), so the decider chain executes in signal context on the
   alternative stack).  */
static enum sig_decision
alt_stack_decider (struct stdc_siginfo *rsi)
{
  struct alt_stack_t *t = (struct alt_stack_t *) rsi->value.ptr_value;
  char local;
  const uintptr_t sp = (uintptr_t) &local;
  const uintptr_t base = (uintptr_t) t->ss_sp;
  t->inside_alt_stack = (sp >= base && sp < base + t->ss_size);
  return sig_decision_resume_execution;
}

static union stdc_siginfo_value
alt_stack_raise (union stdc_siginfo_value value)
{
  /* A genuine kernel delivery: kill() interrupts this thread, the kernel
     invokes the raw handler (with SA_ONSTACK on the alternative stack),
     and the frame decider chain runs inside that handler.  */
  (void) kill (getpid (), SIGUSR1);
  return value;
}

static int
do_test (void)
{
  /* Phase 1: no setter call - the raw handler is installed with the
     library default SA_SIGINFO | SA_NOCLDWAIT | SA_NODEFER, minus
     SA_NOCLDWAIT (stripped by the embedder).  */
  TEST_COMPARE (check_installed_flags (SA_SIGINFO | SA_NODEFER, STRIPPED_FLAG),
		0);

  /* Phase 2: a minimal flag set (SA_SIGINFO only) is honoured exactly,
     and the default's extra flags are not present.  */
  errno = 0;
  TEST_COMPARE (siginstall_set_sa_flags_np (SA_SIGINFO), 0);
  TEST_COMPARE (check_installed_flags (SA_SIGINFO, SA_NODEFER | STRIPPED_FLAG),
		0);

  /* Phase 2b: signals ALREADY installed when the setter runs keep the
     flags they were installed with - the np-setter applies only to later
     raw-handler installations (thrd_signal_handle.h contract).  */
  TEST_COMPARE (
      siginstall_set_sa_flags_np (SA_SIGINFO | SA_NODEFER | STRIPPED_FLAG), 0);
  sigset_t early_set;
  sigemptyset (&early_set);
  sigaddset (&early_set, SIGUSR1);
  void *early = siginstall (&early_set);
  TEST_VERIFY (early != NULL);
  if (early != NULL)
    {
      TEST_COMPARE (siginstall_set_sa_flags_np (SA_SIGINFO), 0);
      struct sigaction early_installed;
      memset (&early_installed, 0, sizeof (early_installed));
      TEST_COMPARE (sigaction (SIGUSR1, NULL, &early_installed), 0);
      /* Still the pre-setter flags: SA_NODEFER present, SA_NOCLDWAIT
	 stripped by the embedder.  */
      TEST_VERIFY ((early_installed.sa_flags & SA_NODEFER) != 0);
      TEST_VERIFY ((early_installed.sa_flags & STRIPPED_FLAG) == 0);
      TEST_COMPARE (siguninstall (early), 0);
    }

  /* Phase 3: a flag set without SA_SIGINFO is rejected with EINVAL and
     must not change the stored flags (the next install still uses
     SA_SIGINFO only).  */
  errno = 0;
  TEST_COMPARE (siginstall_set_sa_flags_np (0), -1);
  TEST_COMPARE (errno, EINVAL);
  errno = 0;
  TEST_COMPARE (siginstall_set_sa_flags_np (SA_NODEFER | STRIPPED_FLAG), -1);
  TEST_COMPARE (errno, EINVAL);
  TEST_COMPARE (check_installed_flags (SA_SIGINFO, SA_NODEFER | STRIPPED_FLAG),
		0);

  /* Phase 4: restoring the default flag set applies to the next install;
     SA_NOCLDWAIT stays stripped on glibc.  */
  errno = 0;
  TEST_COMPARE (
      siginstall_set_sa_flags_np (SA_SIGINFO | SA_NODEFER | STRIPPED_FLAG), 0);
  TEST_COMPARE (check_installed_flags (SA_SIGINFO | SA_NODEFER, STRIPPED_FLAG),
		0);

  /* glibc specific: SA_ONSTACK plus a sigaltstack delivers the raw
     handler on the alternative stack - observable from the decider, which
     runs inside the raw handler.  */
  {
    stack_t ss, oss;
    static char altstack_bytes[64 * 1024] __attribute__ ((aligned (16)));
    memset (&ss, 0, sizeof (ss));
    ss.ss_sp = altstack_bytes;
    ss.ss_size = sizeof (altstack_bytes);
    TEST_COMPARE (sigaltstack (&ss, &oss), 0);
    errno = 0;
    TEST_COMPARE (siginstall_set_sa_flags_np (SA_SIGINFO | SA_ONSTACK), 0);
    sigset_t guarded;
    sigemptyset (&guarded);
    sigaddset (&guarded, SIGUSR1);
    void *handlers = siginstall (&guarded);
    TEST_VERIFY (handlers != NULL);
    if (handlers == NULL)
      return 1;
    struct alt_stack_t t = { .ss_sp = altstack_bytes,
			     .ss_size = sizeof (altstack_bytes),
			     .inside_alt_stack = 0 };
    union stdc_siginfo_value value = { .ptr_value = &t };
    (void) sigguarded (&guarded, alt_stack_raise, NULL, alt_stack_decider,
		       value);
    TEST_VERIFY (t.inside_alt_stack);
    TEST_COMPARE (siguninstall (handlers), 0);
    TEST_COMPARE (sigaltstack (&oss, NULL), 0);
    errno = 0;
    TEST_COMPARE (
	siginstall_set_sa_flags_np (SA_SIGINFO | SA_NODEFER | STRIPPED_FLAG),
	0);
  }

  /* glibc specific: with SIGCHLD siginstall'ed and SA_NOCLDWAIT
     permanently stripped, waitpid still reaps the child - zombie
     semantics survive the raw-handler install.  */
  {
    sigset_t guarded;
    sigemptyset (&guarded);
    sigaddset (&guarded, SIGCHLD);
    void *handlers = siginstall (&guarded);
    TEST_VERIFY (handlers != NULL);
    if (handlers == NULL)
      return 1;
    pid_t pid = fork ();
    TEST_VERIFY (pid >= 0);
    if (pid == 0)
      _exit (3);
    int status;
    TEST_COMPARE (waitpid (pid, &status, 0), pid);
    TEST_VERIFY (WIFEXITED (status));
    TEST_COMPARE (WEXITSTATUS (status), 3);
    TEST_COMPARE (siguninstall (handlers), 0);
  }

  return 0;
}

#include <support/test-driver.c>