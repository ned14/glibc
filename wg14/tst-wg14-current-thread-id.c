/* current_thread_id(): the reference-implementation thread-id API
   declared in the installed <threads.h> (__USE_GNU) and backed by the
   initial-exec TLS cache symbol current_thread_id_cached in libc
   (implemented by the wg14 subdir).  The cache's first call per thread
   performs the gettid syscall; the inline must agree with gettid() and
   differ between threads.
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

#include <sys/syscall.h>
#include <threads.h>
#include <unistd.h>

#include <support/check.h>

static thread_id_t worker_tid;
static int worker_tid_ok;

static int
worker_thr (void *unused)
{
  (void) unused;
  worker_tid = current_thread_id ();
  worker_tid_ok = (worker_tid == (thread_id_t) syscall (SYS_gettid));
  return 0;
}

static int
do_test (void)
{
  /* The inline cache read agrees with the kernel thread id.  */
  const thread_id_t tid = current_thread_id ();
  TEST_COMPARE (tid, (thread_id_t) syscall (SYS_gettid));
  TEST_COMPARE (current_thread_id (), tid);
  /* The tombstone is distinct from every real thread id.  */
  TEST_VERIFY (tid != thread_id_t_tombstone);

  /* A second thread gets its own cached id, distinct per thread.  */
  thrd_t th;
  TEST_COMPARE (thrd_create (&th, worker_thr, NULL), thrd_success);
  int res = 0;
  TEST_COMPARE (thrd_join (th, &res), thrd_success);
  TEST_COMPARE (res, 0);
  TEST_VERIFY (worker_tid_ok);
  TEST_VERIFY (worker_tid != tid);

  return 0;
}

#include <support/test-driver.c>