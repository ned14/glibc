/* Port of wg14_signals tss_destroy_reentrancy_test.c plus the
   basics and the thread-exit cleanup path, to glibc's harness.  The N3924
   async-signal-safe TSS API is declared in the installed <threads.h>
   (__USE_GNU) and implemented in libc by the wg14 subdir; thread-exit
   deinit runs through glibc's __cxa_thread_atexit_impl.
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

#include <stdatomic.h>
#include <stdlib.h>
#include <threads.h>

#include <support/check.h>

static tss_async_signal_safe_t g_tls;
static int destroy_calls;
static int get_never_returned_being_destroyed = 1;

static int
create_cb (void **dest)
{
  unsigned *v = (unsigned *) malloc (sizeof (unsigned));
  if (v == NULL)
    return -1;
  *v = 42;
  *dest = v;
  return 0;
}

static int
destroy_cb (void *v)
{
  /* Re-entrant get() on the same handle mid-destroy: must not deadlock,
     and must never return the value whose destroy callback is running
     (that value was erased from the map before its callback ran)
     (wg14_signals analysis.md UCLK).  */
  if (tss_async_signal_safe_get (g_tls) == v)
    get_never_returned_being_destroyed = 0;
  destroy_calls++;
  free (v);
  return 0;
}

static atomic_int worker_ready;
static atomic_int destroy_done;
static atomic_int worker_exit_destroyed;

/* Thread-exit cleanup: the worker's exit-time deinit (registered via
   __cxa_thread_atexit_impl) erases this thread's map entry and runs
   attr.destroy before the thread fully exits.  */
static int
worker_thr (void *unused)
{
  (void) unused;
  const int init_ret = tss_async_signal_safe_thread_init (g_tls);
  atomic_store_explicit (&worker_ready, 1, memory_order_release);
  if (init_ret != 0)
    return -1;
  /* Stay registered (do not run the exit-time deinit, which would erase
     this thread's map entry) until the main thread has destroyed the
     handle, so destroy() iterates two live entries.  */
  while (atomic_load_explicit (&destroy_done, memory_order_acquire) == 0)
    ;
  return 0;
}

static int
exit_worker_thr (void *unused)
{
  (void) unused;
  if (tss_async_signal_safe_thread_init (g_tls) != 0)
    return -1;
  void *v = tss_async_signal_safe_get (g_tls);
  if (v == NULL || *(unsigned *) v != 42)
    return -1;
  /* The exit-time deinit registered via __cxa_thread_atexit_impl runs
     attr.destroy for this thread's entry when the thread exits.  */
  return 0;
}

static int
do_test (void)
{
  atomic_store_explicit (&worker_ready, 0, memory_order_relaxed);
  atomic_store_explicit (&destroy_done, 0, memory_order_relaxed);
  atomic_store_explicit (&worker_exit_destroyed, 0, memory_order_relaxed);

  struct tss_async_signal_safe_attr attr
      = { .create = create_cb, .destroy = destroy_cb };
  TEST_COMPARE (tss_async_signal_safe_create (&g_tls, &attr), 0);
  TEST_COMPARE (tss_async_signal_safe_thread_init (g_tls), 0);
  unsigned *main_val = (unsigned *) tss_async_signal_safe_get (g_tls);
  TEST_VERIFY (main_val != NULL);
  TEST_COMPARE (*main_val, 42);

  /* Reentrancy arm: two entries are live; destroy runs both attr.destroy
     callbacks with the lock released, and each callback re-enters get()
     on the same handle.  */
  {
    thrd_t thr;
    TEST_COMPARE (thrd_create (&thr, worker_thr, NULL), thrd_success);
    while (atomic_load_explicit (&worker_ready, memory_order_acquire) == 0)
      ;
    TEST_COMPARE (tss_async_signal_safe_destroy (g_tls), 0);
    TEST_COMPARE (destroy_calls, 2);
    TEST_VERIFY (get_never_returned_being_destroyed);
    atomic_store_explicit (&destroy_done, 1, memory_order_release);
    int res = 0;
    TEST_COMPARE (thrd_join (thr, &res), thrd_success);
    TEST_COMPARE (res, 0);
    destroy_calls = 0;
  }

  /* Thread-exit cleanup arm: a fresh handle; the worker thread
     initialises it and exits; the exit-time deinit runs attr.destroy for
     the worker's entry on the way out, so the map is empty when
     destroy() runs and no second callback is invoked.  */
  {
    struct tss_async_signal_safe_attr attr2
	= { .create = create_cb, .destroy = destroy_cb };
    TEST_COMPARE (tss_async_signal_safe_create (&g_tls, &attr2), 0);
    thrd_t thr;
    TEST_COMPARE (thrd_create (&thr, exit_worker_thr, NULL), thrd_success);
    int res = 0;
    TEST_COMPARE (thrd_join (thr, &res), thrd_success);
    TEST_COMPARE (res, 0);
    /* The worker's entry was deinitialised at exit (via
       __cxa_thread_atexit_impl); destroy() now has nothing to erase.  */
    TEST_COMPARE (destroy_calls, 1);
    TEST_COMPARE (tss_async_signal_safe_destroy (g_tls), 0);
    TEST_COMPARE (destroy_calls, 1);
  }

  return 0;
}

#include <support/test-driver.c>