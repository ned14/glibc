/* ISO C11 Standard: 7.26 - Thread support library  <threads.h>.
   Copyright (C) 2018-2026 Free Software Foundation, Inc.
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

#ifndef _THREADS_H
#define _THREADS_H	1

#include <features.h>
#include <time.h>

__BEGIN_DECLS

#include <bits/thread-shared-types.h>
#include <bits/types/once_flag.h>
#include <bits/types/struct_timespec.h>

#if (!defined __STDC_VERSION__				\
     || __STDC_VERSION__ <= 201710L			\
     || !__GNUC_PREREQ (13, 0)) && !defined __cplusplus
# define thread_local _Thread_local
#endif

#define TSS_DTOR_ITERATIONS 4
typedef __tss_t tss_t;
typedef void (*tss_dtor_t) (void*);

typedef __thrd_t thrd_t;
typedef int (*thrd_start_t) (void*);

/* Exit and error codes.  */
enum
{
  thrd_success  = 0,
  thrd_busy     = 1,
  thrd_error    = 2,
  thrd_nomem    = 3,
  thrd_timedout = 4
};

/* Mutex types.  */
enum
{
  mtx_plain     = 0,
  mtx_recursive = 1,
  mtx_timed     = 2
};

typedef union
{
  char __size[__SIZEOF_PTHREAD_MUTEX_T];
  long int __align __LOCK_ALIGNMENT;
} mtx_t;

typedef union
{
  char __size[__SIZEOF_PTHREAD_COND_T];
  __extension__ long long int __align __LOCK_ALIGNMENT;
} cnd_t;

/* Threads functions.  */

/* Create a new thread executing the function __FUNC.  Arguments for __FUNC
   are passed through __ARG.  If successful, __THR is set to new thread
   identifier.  */
extern int thrd_create (thrd_t *__thr, thrd_start_t __func, void *__arg);

/* Check if __LHS and __RHS point to the same thread.  */
extern int thrd_equal (thrd_t __lhs, thrd_t __rhs);

/* Return current thread identifier.  */
extern thrd_t thrd_current (void);

/* Block current thread execution for at least the time pointed by
   __TIME_POINT.  The current thread may resume if receives a signal.  In
   that case, if __REMAINING is not NULL, the remaining time is stored in
   the object pointed by it.  */
#ifndef __USE_TIME64_REDIRECTS
extern int thrd_sleep (const struct timespec *__time_point,
		       struct timespec *__remaining);
#else
# ifdef __REDIRECT
extern int __REDIRECT (thrd_sleep, (const struct timespec *__time_point,
                                    struct timespec *__remaining),
                       __thrd_sleep64);
# else
#  define thrd_sleep __thrd_sleep64
# endif
#endif

/* Terminate current thread execution, cleaning up any thread local
   storage and freeing resources.  Returns the value specified in __RES.  */
extern void thrd_exit (int __res) __attribute__ ((__noreturn__));

/* Detach the thread identified by __THR from the current environment
   (it does not allow join or wait for it).  */
extern int thrd_detach (thrd_t __thr);

/* Block current thread until execution of __THR is complete.  In case that
   __RES is not NULL, will store the return value of __THR when exiting.  */
extern int thrd_join (thrd_t __thr, int *__res);

/* Stop current thread execution and call the scheduler to decide which
   thread should execute next.  The current thread may be selected by the
   scheduler to keep running.  */
extern void thrd_yield (void);

#ifdef __USE_EXTERN_INLINES
/* Optimizations.  */
__extern_inline int
thrd_equal (thrd_t __thread1, thrd_t __thread2)
{
  return __thread1 == __thread2;
}
#endif


/* Mutex functions.  */

/* Creates a new mutex object with type __TYPE.  If successful the new
   object is pointed by __MUTEX.  */
extern int mtx_init (mtx_t *__mutex, int __type);

/* Block the current thread until the mutex pointed to by __MUTEX is
   unlocked.  In that case current thread will not be blocked.  */
extern int mtx_lock (mtx_t *__mutex);

/* Block the current thread until the mutex pointed by __MUTEX is unlocked
   or time pointed by __TIME_POINT is reached.  In case the mutex is unlock,
   the current thread will not be blocked.  */
#ifndef __USE_TIME64_REDIRECTS
extern int mtx_timedlock (mtx_t *__restrict __mutex,
			  const struct timespec *__restrict __time_point);
#else
# ifdef __REDIRECT
extern int __REDIRECT (mtx_timedlock, (mtx_t *__restrict __mutex,
                                       const struct timespec *__restrict
                                       __time_point),
                       __mtx_timedlock64);
# else
#  define mtx_timedlock __mtx_timedlock64
# endif
#endif

/* Try to lock the mutex pointed by __MUTEX without blocking.  If the mutex
   is free the current threads takes control of it, otherwise it returns
   immediately.  */
extern int mtx_trylock (mtx_t *__mutex);

/* Unlock the mutex pointed by __MUTEX.  It may potentially awake other
   threads waiting on this mutex.  */
extern int mtx_unlock (mtx_t *__mutex);

/* Destroy the mutex object pointed by __MUTEX.  */
extern void mtx_destroy (mtx_t *__mutex);


/* Call function __FUNC exactly once, even if invoked from several threads.
   All calls must be made with the same __FLAGS object.  */
extern void call_once (once_flag *__flag, void (*__func)(void));


/* Condition variable functions.  */

/* Initialize new condition variable pointed by __COND.  */
extern int cnd_init (cnd_t *__cond);

/* Unblock one thread that currently waits on condition variable pointed
   by __COND.  */
extern int cnd_signal (cnd_t *__cond);

/* Unblock all threads currently waiting on condition variable pointed by
   __COND.  */
extern int cnd_broadcast (cnd_t *__cond);

/* Block current thread on the condition variable pointed by __COND.  */
extern int cnd_wait (cnd_t *__cond, mtx_t *__mutex);

/* Block current thread on the condition variable until condition variable
   pointed by __COND is signaled or time pointed by __TIME_POINT is
   reached.  */
#ifndef __USE_TIME64_REDIRECTS
extern int cnd_timedwait (cnd_t *__restrict __cond,
			  mtx_t *__restrict __mutex,
			  const struct timespec *__restrict __time_point);
#else
# ifdef __REDIRECT
extern int __REDIRECT (cnd_timedwait, (cnd_t *__restrict __cond,
                                       mtx_t *__restrict __mutex,
                                       const struct timespec *__restrict
                                       __time_point),
                       __cnd_timedwait64);
# else
#  define cnd_timedwait __cnd_timedwait64
# endif
#endif

/* Destroy condition variable pointed by __cond and free all of its
   resources.  */
extern void cnd_destroy (cnd_t *__COND);


/* Thread specific storage functions.  */

/* Create new thread-specific storage key and stores it in the object pointed
   by __TSS_ID.  If __DESTRUCTOR is not NULL, the function will be called when
   the thread terminates.  */
extern int tss_create (tss_t *__tss_id, tss_dtor_t __destructor);

/* Return the value held in thread-specific storage for the current thread
   identified by __TSS_ID.  */
extern void *tss_get (tss_t __tss_id);

/* Sets the value of the thread-specific storage identified by __TSS_ID for
   the current thread to __VAL.  */
extern int tss_set (tss_t __tss_id, void *__val);

/* Destroys the thread-specific storage identified by __TSS_ID.  The
   destructor is not called until thrd_exit is called.  */
extern void tss_delete (tss_t __tss_id);

#ifdef __USE_GNU
/*
 * WG14 N3924 improved-C-signals API (async-signal-safe TSS, 7.30.6.5-8)
 * and the reference-implementation thread-id API.
 *
 * This block is a mirror of the declarations in the wg14_signals
 * submodule (wg14_signals/include/wg14_signals/tss_async_signal_safe.h
 * and current_thread_id.h), which the wg14 subdir compiles into libc as
 * the N3924 entrypoints.  The mirror must stay in sync with the pinned
 * submodule: the sync test wg14/check-wg14-mirror.py (part of make
 * check) verifies every declaration below against the submodule's
 * headers.  current_thread_id()/thread_id_t are reference-implementation
 * extensions, not part of the N3924 wording (exposed here per the
 * llvm-libc fork precedent).  */

#include <stdint.h>

/* The type of an async signal safe thread local.  */
typedef struct tss_async_signal_safe_s *tss_async_signal_safe_t;

/* The attributes for creating an async signal safe thread local.  */
struct tss_async_signal_safe_attr
{
  int (*const create) (void **dest);	/* Create an instance.  */
  int (*const destroy) (void *v);	/* Destroy an instance.  */
};

/* Create an async signal safe thread local instance.  */
extern int tss_async_signal_safe_create (tss_async_signal_safe_t *val,
					 const struct tss_async_signal_safe_attr *attr);

/* Destroy an async signal safe thread local instance.  All threads that
   initialised this instance must have exited before this is called: a
   thread's exit-time deinitialisation handler may still access the
   instance otherwise.  */
extern int tss_async_signal_safe_destroy (tss_async_signal_safe_t val);

/* THREADSAFE Initialise an async signal safe thread local instance for a
   specific thread.  The initialisation and registration of a thread local
   state for a specific thread is not async signal safe, and so must be
   manually performed by each thread before use.  */
extern int tss_async_signal_safe_thread_init (tss_async_signal_safe_t val);

/* THREADSAFE ASYNC-SIGNAL-SAFE Get the thread local value for the current
   thread.  */
extern void *tss_async_signal_safe_get (tss_async_signal_safe_t val);

/* The type of a thread id.  */
typedef uintptr_t thread_id_t;

static const thread_id_t
thread_id_t_tombstone = 0;

/* The cached current thread id, read by current_thread_id().  Initial-exec
   TLS so the read is async-signal-safe; consumer declarations must keep
   this spelling (see plans/glibc-fork.md "TLS policy inside glibc").  */
extern __thread thread_id_t current_thread_id_cached
  __attribute__ ((tls_model ("initial-exec")));

extern thread_id_t internal_current_thread_id_cached_set (void);

/* THREADSAFE ASYNC-SIGNAL-SAFE Retrieve the current thread id.
   `inline` is not valid in strict C89 modes that the installed-header
   checks cover, so the declaration is C99/C++-gated (the reference
   implementation itself is C11-minimum; the mirror-sync checker strips
   preprocessor lines, so this gate does not affect the verbatim match).  */
#if defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L || defined __cplusplus
static inline thread_id_t
current_thread_id (void)
{
#if defined __USE_GNU
  if (current_thread_id_cached == thread_id_t_tombstone)
    {
      current_thread_id_cached = internal_current_thread_id_cached_set ();
    }
  return current_thread_id_cached;
#else
  return internal_current_thread_id_cached_set ();
#endif
}
#endif

#endif /* __USE_GNU */

__END_DECLS

#endif /* _THREADS_H */
