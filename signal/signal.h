/* Copyright (C) 1991-2026 Free Software Foundation, Inc.
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

/*
 *	ISO C99 Standard: 7.14 Signal handling <signal.h>
 */

#ifndef	_SIGNAL_H
#define _SIGNAL_H

#include <features.h>

__BEGIN_DECLS

#include <bits/types.h>
#include <bits/signum-generic.h>

#include <bits/types/sig_atomic_t.h>

#if defined __USE_POSIX
#include <bits/types/sigset_t.h>
#endif

#if defined __USE_XOPEN || defined __USE_XOPEN2K
# ifndef __pid_t_defined
typedef __pid_t pid_t;
#  define __pid_t_defined
#endif
#ifdef __USE_XOPEN
# endif
# ifndef __uid_t_defined
typedef __uid_t uid_t;
#  define __uid_t_defined
# endif
#endif	/* Unix98 */

#ifdef __USE_POSIX199309
/* We need `struct timespec' later on.  */
# include <bits/types/struct_timespec.h>
#endif

#if defined __USE_POSIX199309 || defined __USE_XOPEN_EXTENDED
# include <bits/types/siginfo_t.h>
# include <bits/siginfo-consts.h>
#endif

#ifdef __USE_MISC
# include <bits/types/sigval_t.h>
#endif

#ifdef __USE_POSIX199309
# include <bits/types/sigevent_t.h>
# include <bits/sigevent-consts.h>
#endif


/* Type of a signal handler.  */
typedef void (*__sighandler_t) (int);

/* The X/Open definition of `signal' specifies the SVID semantic.  Use
   the additional function `sysv_signal' when X/Open compatibility is
   requested.  */
extern __sighandler_t __sysv_signal (int __sig, __sighandler_t __handler)
     __THROW;
#ifdef __USE_GNU
extern __sighandler_t sysv_signal (int __sig, __sighandler_t __handler)
     __THROW;
#endif

/* Set the handler for the signal SIG to HANDLER, returning the old
   handler, or SIG_ERR on error.
   By default `signal' has the BSD semantic.  */
#ifdef __USE_MISC
extern __sighandler_t signal (int __sig, __sighandler_t __handler)
     __THROW;
#else
/* Make sure the used `signal' implementation is the SVID version. */
# ifdef __REDIRECT_NTH
extern __sighandler_t __REDIRECT_NTH (signal,
				      (int __sig, __sighandler_t __handler),
				      __sysv_signal);
# else
#  define signal __sysv_signal
# endif
#endif

#if defined __USE_XOPEN_EXTENDED && !defined __USE_XOPEN2K8
/* The X/Open definition of `signal' conflicts with the BSD version.
   So they defined another function `bsd_signal'.  */
extern __sighandler_t bsd_signal (int __sig, __sighandler_t __handler)
     __THROW;
#endif

/* Send signal SIG to process number PID.  If PID is zero,
   send SIG to all processes in the current process's process group.
   If PID is < -1, send SIG to all processes in process group - PID.  */
#ifdef __USE_POSIX
extern int kill (__pid_t __pid, int __sig) __THROW;
#endif /* Use POSIX.  */

#if defined __USE_MISC || defined __USE_XOPEN_EXTENDED
/* Send SIG to all processes in process group PGRP.
   If PGRP is zero, send SIG to all processes in
   the current process's process group.  */
extern int killpg (__pid_t __pgrp, int __sig) __THROW;
#endif /* Use misc || X/Open Unix.  */

/* Raise signal SIG, i.e., send SIG to yourself.  */
extern int raise (int __sig) __THROW;

#ifdef __USE_MISC
/* SVID names for the same things.  */
extern __sighandler_t ssignal (int __sig, __sighandler_t __handler)
     __THROW;
extern int gsignal (int __sig) __THROW;
#endif /* Use misc.  */

#ifdef __USE_XOPEN2K8
/* Print a message describing the meaning of the given signal number.  */
extern void psignal (int __sig, const char *__s);

/* Print a message describing the meaning of the given signal information.  */
extern void psiginfo (const siginfo_t *__pinfo, const char *__s);
#endif /* POSIX 2008.  */



/* The `sigpause' function in X/Open defines the argument as the
   signal number.  This requires redirecting to another function
   because the default version in glibc uses an old BSD interface.

   This function is a cancellation point and therefore not marked with
   __THROW.  */

#ifdef __USE_XOPEN_EXTENDED
# ifdef __GNUC__
extern int sigpause (int __sig) __asm__ ("__xpg_sigpause")
  __attribute_deprecated_msg__ ("Use the sigsuspend function instead");
# else
extern int __sigpause (int __sig_or_mask, int __is_sig);
/* Remove a signal from the signal mask and suspend the process.  */
#  define sigpause(sig) __sigpause ((sig), 1)
# endif
#endif


#ifdef __USE_MISC
/* None of the following functions should be used anymore.  They are here
   only for compatibility.  A single word (`int') is not guaranteed to be
   enough to hold a complete signal mask and therefore these functions
   simply do not work in many situations.  Use `sigprocmask' instead.  */

/* Compute mask for signal SIG.  */
# define sigmask(sig) \
  __glibc_macro_warning ("sigmask is deprecated") \
  ((int)(1u << ((sig) - 1)))

/* Block signals in MASK, returning the old mask.  */
extern int sigblock (int __mask) __THROW __attribute_deprecated__;

/* Set the mask of blocked signals to MASK, returning the old mask.  */
extern int sigsetmask (int __mask) __THROW __attribute_deprecated__;

/* Return currently selected signal mask.  */
extern int siggetmask (void) __THROW __attribute_deprecated__;
#endif /* Use misc.  */


#ifdef __USE_MISC
# define NSIG	_NSIG
#endif

#ifdef __USE_GNU
typedef __sighandler_t sighandler_t;
#endif

/* 4.4 BSD uses the name `sig_t' for this.  */
#ifdef __USE_MISC
typedef __sighandler_t sig_t;
#endif

#ifdef __USE_POSIX

/* Clear all signals from SET.  */
extern int sigemptyset (sigset_t *__set) __THROW __nonnull ((1));

/* Set all signals in SET.  */
extern int sigfillset (sigset_t *__set) __THROW __nonnull ((1));

/* Add SIGNO to SET.  */
extern int sigaddset (sigset_t *__set, int __signo) __THROW __nonnull ((1));

/* Remove SIGNO from SET.  */
extern int sigdelset (sigset_t *__set, int __signo) __THROW __nonnull ((1));

/* Return 1 if SIGNO is in SET, 0 if not.  */
extern int sigismember (const sigset_t *__set, int __signo)
     __THROW __nonnull ((1));

# ifdef __USE_GNU
/* Return non-empty value is SET is not empty.  */
extern int sigisemptyset (const sigset_t *__set) __THROW __nonnull ((1));

/* Build new signal set by combining the two inputs set using logical AND.  */
extern int sigandset (sigset_t *__set, const sigset_t *__left,
		      const sigset_t *__right) __THROW __nonnull ((1, 2, 3));

/* Build new signal set by combining the two inputs set using logical OR.  */
extern int sigorset (sigset_t *__set, const sigset_t *__left,
		     const sigset_t *__right) __THROW __nonnull ((1, 2, 3));
# endif /* GNU */

/* Get the system-specific definitions of `struct sigaction'
   and the `SA_*' and `SIG_*'. constants.  */
# include <bits/sigaction.h>

/* Get and/or change the set of blocked signals.  */
extern int sigprocmask (int __how, const sigset_t *__restrict __set,
			sigset_t *__restrict __oset) __THROW;

/* Change the set of blocked signals to SET,
   wait until a signal arrives, and restore the set of blocked signals.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
extern int sigsuspend (const sigset_t *__set) __nonnull ((1));

/* Get and/or set the action for signal SIG.  */
extern int sigaction (int __sig, const struct sigaction *__restrict __act,
		      struct sigaction *__restrict __oact) __THROW;

/* Put in SET all signals that are blocked and waiting to be delivered.  */
extern int sigpending (sigset_t *__set) __THROW __nonnull ((1));


# ifdef __USE_POSIX199506
/* Select any of pending signals from SET or wait for any to arrive.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
extern int sigwait (const sigset_t *__restrict __set, int *__restrict __sig)
     __nonnull ((1, 2));
# endif /* Use POSIX 1995.  */

# ifdef __USE_POSIX199309
/* Select any of pending signals from SET and place information in INFO.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
extern int sigwaitinfo (const sigset_t *__restrict __set,
			siginfo_t *__restrict __info) __nonnull ((1));

/* Select any of pending signals from SET and place information in INFO.
   Wait the time specified by TIMEOUT if no signal is pending.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
#  ifndef __USE_TIME64_REDIRECTS
extern int sigtimedwait (const sigset_t *__restrict __set,
			 siginfo_t *__restrict __info,
			 const struct timespec *__restrict __timeout)
     __nonnull ((1));
#  else
#   ifdef __REDIRECT
extern int __REDIRECT (sigtimedwait,
                       (const sigset_t *__restrict __set,
                        siginfo_t *__restrict __info,
                        const struct timespec *__restrict __timeout),
                       __sigtimedwait64)
     __nonnull ((1));
#   else
#    define sigtimedwait __sigtimedwait64
#   endif
#  endif

/* Send signal SIG to the process PID.  Associate data in VAL with the
   signal.  */
extern int sigqueue (__pid_t __pid, int __sig, const union sigval __val)
     __THROW;
# endif	/* Use POSIX 199306.  */

#endif /* Use POSIX.  */

#ifdef __USE_MISC

/* Get machine-dependent `struct sigcontext' and signal subcodes.  */
# include <bits/sigcontext.h>

/* Restore the state saved in SCP.  */
extern int sigreturn (struct sigcontext *__scp) __THROW;

#endif /* Use misc.  */


#if defined __USE_XOPEN_EXTENDED || defined __USE_XOPEN2K8
# define __need_size_t
# include <stddef.h>

# include <bits/types/stack_t.h>
# if defined __USE_XOPEN || defined __USE_XOPEN2K8
/* This will define `ucontext_t' and `mcontext_t'.  */
#  include <sys/ucontext.h>
# endif
#endif /* Use POSIX.1-2008 or X/Open Unix.  */

#if defined __USE_XOPEN_EXTENDED || defined __USE_MISC
/* If INTERRUPT is nonzero, make signal SIG interrupt system calls
   (causing them to fail with EINTR); if INTERRUPT is zero, make system
   calls be restarted after signal SIG.  */
extern int siginterrupt (int __sig, int __interrupt) __THROW
  __attribute_deprecated_msg__ ("Use sigaction with SA_RESTART instead");

# include <bits/sigstack.h>
# include <bits/sigstksz.h>
# include <bits/ss_flags.h>

/* Alternate signal handler stack interface.
   This interface should always be preferred over `sigstack'.  */
extern int sigaltstack (const stack_t *__restrict __ss,
			stack_t *__restrict __oss) __THROW;
#endif /* __USE_XOPEN_EXTENDED || __USE_MISC */

#if ((defined __USE_XOPEN_EXTENDED && !defined __USE_XOPEN2K8)	\
     || defined __USE_MISC)
# include <bits/types/struct_sigstack.h>
#endif

#if ((defined __USE_XOPEN_EXTENDED && !defined __USE_XOPEN2K)	\
     || defined __USE_MISC)
/* Run signals handlers on the stack specified by SS (if not NULL).
   If OSS is not NULL, it is filled in with the old signal stack status.
   This interface is obsolete and on many platform not implemented.  */
extern int sigstack (struct sigstack *__ss, struct sigstack *__oss)
     __THROW __attribute_deprecated__;
#endif

#ifdef __USE_XOPEN_EXTENDED
/* Simplified interface for signal management.  */

/* Add SIG to the calling process' signal mask.  */
extern int sighold (int __sig) __THROW
  __attribute_deprecated_msg__ ("Use the sigprocmask function instead");

/* Remove SIG from the calling process' signal mask.  */
extern int sigrelse (int __sig) __THROW
  __attribute_deprecated_msg__ ("Use the sigprocmask function instead");

/* Set the disposition of SIG to SIG_IGN.  */
extern int sigignore (int __sig) __THROW
  __attribute_deprecated_msg__ ("Use the signal function instead");

/* Set the disposition of SIG.  */
extern __sighandler_t sigset (int __sig, __sighandler_t __disp) __THROW
  __attribute_deprecated_msg__
    ("Use the signal and sigprocmask functions instead");
#endif

#if defined __USE_POSIX199506 || defined __USE_UNIX98
/* Some of the functions for handling signals in threaded programs must
   be defined here.  */
# include <bits/pthreadtypes.h>
# include <bits/sigthread.h>
#endif /* use Unix98 */

/* The following functions are used internally in the C library and in
   other code which need deep insights.  */

/* Return number of available real-time signal with highest priority.  */
extern int __libc_current_sigrtmin (void) __THROW;
/* Return number of available real-time signal with lowest priority.  */
extern int __libc_current_sigrtmax (void) __THROW;

#define SIGRTMIN        (__libc_current_sigrtmin ())
#define SIGRTMAX        (__libc_current_sigrtmax ())

/* System-specific extensions.  */
#include <bits/signal_ext.h>

#ifdef __USE_GNU
/*
 * WG14 N3924 improved-C-signals API.
 *
 * This block is a mirror of the declarations and macros in the
 * wg14_signals submodule (wg14_signals/include/wg14_signals/
 * thrd_signal_handle.h; WG14 N3924 rev-5), which the wg14 subdir compiles
 * into libc as the N3924 entrypoints.  The mirror must stay in sync with
 * the pinned submodule: the sync test wg14/check-wg14-mirror.py (part of
 * make check) verifies every declaration below against the submodule's
 * header.  sigfence() and SIGGUARDED_FAILURE_VALUE are defined here and
 * suppressed inside the submodule's own translation units via the
 * WG14_SIGNALS_DISABLE_* embedder guards (wg14/embedder_shim.h).
 */

#include <stdbool.h>
#include <stdint.h>

struct sig_global_state_tss_state_per_frame_t;
struct sighandler_info;
struct global_signal_decider_t;

#define WG14_SIGNALS_SIGFENCE_GLUE(x, y) x y
#define WG14_SIGNALS_SIGFENCE_RETURN_ARG_COUNT(_1_, _2_, _3_, _4_, _5_, _6_,   \
                                               _7_, _8_, count, ...)           \
  count
#define WG14_SIGNALS_SIGFENCE_EXPAND_ARGS(args)                                \
  WG14_SIGNALS_SIGFENCE_RETURN_ARG_COUNT args

/* The argument counting below uses __VA_OPT__ only for the zero-argument */
/* sigfence() form (the comma-suppression case it exists for). __VA_OPT__ is */
/* C23/C++20, provided as an extension by GCC/Clang in all modes and by MSVC's */
/* conforming preprocessor in C++20 and C11/C17 modes only. MSVC in C++14/17 */
/* mode has no __VA_OPT__ at all, so use a plain comma-list counting there: */
/* it dispatches 1..8 arguments correctly (and the compile-time assert below */
/* checks exactly those), while the zero-argument sigfence() form is */
/* unavailable on such compilers (plans/analysis.md 4.10). */
#if defined(__GNUC__) || defined(__clang__)
#define WG14_SIGNALS_HAVE_VA_OPT 1
#elif defined(_MSC_VER) && defined(_MSVC_TRADITIONAL) &&                       \
(0 == _MSVC_TRADITIONAL)
#if defined(__cplusplus)
#if defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L)
#define WG14_SIGNALS_HAVE_VA_OPT 1
#endif
#else
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#define WG14_SIGNALS_HAVE_VA_OPT 1
#endif
#endif
#endif
#ifdef WG14_SIGNALS_HAVE_VA_OPT
#define WG14_SIGNALS_SIGFENCE_COUNT_ARGS_MAX8(...)                             \
  WG14_SIGNALS_SIGFENCE_EXPAND_ARGS(                                           \
  (__VA_ARGS__ __VA_OPT__(, ) 8, 7, 6, 5, 4, 3, 2, 1, 0))
#else
#define WG14_SIGNALS_SIGFENCE_COUNT_ARGS_MAX8(...)                             \
  WG14_SIGNALS_SIGFENCE_EXPAND_ARGS((__VA_ARGS__, 8, 7, 6, 5, 4, 3, 2, 1, 0))
#endif
#define WG14_SIGNALS_SIGFENCE_OVERLOAD_MACRO2(name, count) name##count
#define WG14_SIGNALS_SIGFENCE_OVERLOAD_MACRO1(name, count)                     \
  WG14_SIGNALS_SIGFENCE_OVERLOAD_MACRO2(name, count)
#define WG14_SIGNALS_SIGFENCE_OVERLOAD_MACRO(name, count)                      \
  WG14_SIGNALS_SIGFENCE_OVERLOAD_MACRO1(name, count)
#define WG14_SIGNALS_SIGFENCE_CALL_OVERLOAD(name, ...)                         \
  WG14_SIGNALS_SIGFENCE_GLUE(                                                  \
  WG14_SIGNALS_SIGFENCE_OVERLOAD_MACRO(                                        \
  name, WG14_SIGNALS_SIGFENCE_COUNT_ARGS_MAX8(__VA_ARGS__)),                   \
  (__VA_ARGS__))

  /* The arg counting above depends on __VA_OPT__ (C23/C++20, supported as a */
  /* GNU/Clang/MSVC extension in older modes). Verify at compile time that the */
  /* counting machinery returns the right counts: on a compiler without */
  /* __VA_OPT__ the expansion below is a hard preprocessing error, and on a */
  /* compiler whose counting is broken the assertion fails -- either way the */
  /* defect surfaces instead of silently mis-dispatching. (The zero-argument */
  /* path -- the comma-suppression case __VA_OPT__ exists for -- is exercised by */
  /* test/sigfence_fence_test.c, whose TU suppresses the -Wpedantic diagnostic */
  /* that an empty variadic-macro call triggers; a public header must not. */
  /* The counts asserted here -- 3 and 5 -- are in the 1..8 range that the */
  /* non-__VA_OPT__ fallback counting also handles, so the assert is valid on */
  /* every supported compiler, including MSVC C++14/17.) */
#if defined __cplusplus && __cplusplus >= 201103L
  static_assert(
  ((WG14_SIGNALS_SIGFENCE_COUNT_ARGS_MAX8(a, b, c) == 3) &&
   (WG14_SIGNALS_SIGFENCE_COUNT_ARGS_MAX8(a, b, c, d, e) == 5)),
  "wg14_signals: sigfence() argument counting is broken (requires __VA_OPT__ "
  "on this compiler, or a counting-broken preprocessor)");
#elif defined __STDC_VERSION__ && __STDC_VERSION__ >= 201112L && \
        !defined __cplusplus
  _Static_assert(
  ((WG14_SIGNALS_SIGFENCE_COUNT_ARGS_MAX8(a, b, c) == 3) &&
   (WG14_SIGNALS_SIGFENCE_COUNT_ARGS_MAX8(a, b, c, d, e) == 5)),
  "wg14_signals: sigfence() argument counting is broken (requires __VA_OPT__ "
  "on this compiler, or a counting-broken preprocessor)");
#endif

#if (defined(__GNUC__) || defined(__clang__)) && !defined(DISABLE_INLINE_ASM)
/* On compilers with extended inline asm, we can tell the compiler that a */
/* specific list of variables must be specifically written out and reloaded */
/* around the fence. You may find https://godbolt.org/z/chh8ee6Mj useful to */
/* review. DISABLE_INLINE_ASM (defined by cmake/filc-toolchain.cmake for the */
/* Fil-C memory-safe compiler) selects the portable volatile-sink fallback */
/* below instead, because Fil-C cannot compile these asm forms (analysis.md */
/* AA2). */
#define WG14_SIGNALS_SIGFENCE_IMPL_0() __asm__ volatile(";" : : : "memory")
#define WG14_SIGNALS_SIGFENCE_IMPL_1(a)                                        \
  __asm__ volatile(";" : "+m"(a) : : "memory")
#define WG14_SIGNALS_SIGFENCE_IMPL_2(a, b)                                     \
  __asm__ volatile(";" : "+m"(a), "+m"(b) : : "memory")
#define WG14_SIGNALS_SIGFENCE_IMPL_3(a, b, c)                                  \
  __asm__ volatile(";" : "+m"(a), "+m"(b), "+m"(c) : : "memory")
#define WG14_SIGNALS_SIGFENCE_IMPL_4(a, b, c, d)                               \
  __asm__ volatile(";" : "+m"(a), "+m"(b), "+m"(c), "+m"(d) : : "memory")
#define WG14_SIGNALS_SIGFENCE_IMPL_5(a, b, c, d, e)                            \
  __asm__ volatile(";"                                                         \
                   : "+m"(a), "+m"(b), "+m"(c), "+m"(d), "+m"(e)               \
                   :                                                           \
                   : "memory")
#define WG14_SIGNALS_SIGFENCE_IMPL_6(a, b, c, d, e, f)                         \
  __asm__ volatile(";"                                                         \
                   : "+m"(a), "+m"(b), "+m"(c), "+m"(d), "+m"(e), "+m"(f)      \
                   :                                                           \
                   : "memory")
#define WG14_SIGNALS_SIGFENCE_IMPL_7(a, b, c, d, e, f, g)                      \
  __asm__ volatile(";"                                                         \
                   : "+m"(a), "+m"(b), "+m"(c), "+m"(d), "+m"(e), "+m"(f),     \
                     "+m"(g)                                                   \
                   :                                                           \
                   : "memory")
#define WG14_SIGNALS_SIGFENCE_IMPL_8(a, b, c, d, e, f, g, h)                   \
  __asm__ volatile(";"                                                         \
                   : "+m"(a), "+m"(b), "+m"(c), "+m"(d), "+m"(e), "+m"(f),     \
                     "+m"(g), "+m"(h)                                          \
                   :                                                           \
                   : "memory")
#else
  /* Compilers without extended inline asm (e.g. MSVC), or with it disabled via */
  /* -DDISABLE_INLINE_ASM (Fil-C): force the listed local variables to be */
  /* memory-resident, and their values reloaded afterwards, as the "+m" operands */
  /* and "memory" clobber above do. */
  /* WG14_SIGNALS_SIGFENCE_ESCAPE() (1) stores each variable's address into a */
  /* volatile sink so the address escapes to observable memory, and (2) performs */
  /* a volatile read of one byte of the object -- char may alias any object */
  /* (C11 6.5p7), and volatile accesses are observable behaviour (C11 5.1.2.3), */
  /* so no optimizer, link-time code generation (/GL /LTCG) included, may */
  /* eliminate or reorder them: the value is committed to memory before the */
  /* fence and must be reloaded after it. No out-of-line function is needed, so */
  /* the fence cannot be defeated by the optimizer inlining a helper away, and */
  /* the sink is per-TU static, so header-only consumers need no library */
  /* symbols. */
  static void *volatile __wg14_sigfence_sink[9];
#define WG14_SIGNALS_SIGFENCE_BARRIER()                                        \
  ((void) (__wg14_sigfence_sink[8] =                             \
           __wg14_sigfence_sink[8]))
#define WG14_SIGNALS_SIGFENCE_ESCAPE(a, i)                                     \
  do                                                                           \
  {                                                                            \
    __wg14_sigfence_sink[(i)] = (void *) &(a);                   \
    (void) *(volatile unsigned char *) &(a);                                   \
  } while(0)
#define WG14_SIGNALS_SIGFENCE_IMPL_0() WG14_SIGNALS_SIGFENCE_BARRIER()
#define WG14_SIGNALS_SIGFENCE_IMPL_1(a)                                        \
  do                                                                           \
  {                                                                            \
    WG14_SIGNALS_SIGFENCE_BARRIER();                                           \
    WG14_SIGNALS_SIGFENCE_ESCAPE(a, 0);                                        \
    WG14_SIGNALS_SIGFENCE_BARRIER();                                           \
  } while(0)
#define WG14_SIGNALS_SIGFENCE_IMPL_2(a, b)                                     \
  do                                                                           \
  {                                                                            \
    WG14_SIGNALS_SIGFENCE_BARRIER();                                           \
    WG14_SIGNALS_SIGFENCE_ESCAPE(a, 0);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(b, 1);                                        \
    WG14_SIGNALS_SIGFENCE_BARRIER();                                           \
  } while(0)
#define WG14_SIGNALS_SIGFENCE_IMPL_3(a, b, c)                                  \
  do                                                                           \
  {                                                                            \
    WG14_SIGNALS_SIGFENCE_BARRIER();                                           \
    WG14_SIGNALS_SIGFENCE_ESCAPE(a, 0);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(b, 1);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(c, 2);                                        \
    WG14_SIGNALS_SIGFENCE_BARRIER();                                           \
  } while(0)
#define WG14_SIGNALS_SIGFENCE_IMPL_4(a, b, c, d)                               \
  do                                                                           \
  {                                                                            \
    WG14_SIGNALS_SIGFENCE_BARRIER();                                           \
    WG14_SIGNALS_SIGFENCE_ESCAPE(a, 0);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(b, 1);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(c, 2);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(d, 3);                                        \
    WG14_SIGNALS_SIGFENCE_BARRIER();                                           \
  } while(0)
#define WG14_SIGNALS_SIGFENCE_IMPL_5(a, b, c, d, e)                            \
  do                                                                           \
  {                                                                            \
    WG14_SIGNALS_SIGFENCE_BARRIER();                                           \
    WG14_SIGNALS_SIGFENCE_ESCAPE(a, 0);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(b, 1);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(c, 2);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(d, 3);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(e, 4);                                        \
    WG14_SIGNALS_SIGFENCE_BARRIER();                                           \
  } while(0)
#define WG14_SIGNALS_SIGFENCE_IMPL_6(a, b, c, d, e, f)                         \
  do                                                                           \
  {                                                                            \
    WG14_SIGNALS_SIGFENCE_BARRIER();                                           \
    WG14_SIGNALS_SIGFENCE_ESCAPE(a, 0);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(b, 1);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(c, 2);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(d, 3);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(e, 4);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(f, 5);                                        \
    WG14_SIGNALS_SIGFENCE_BARRIER();                                           \
  } while(0)
#define WG14_SIGNALS_SIGFENCE_IMPL_7(a, b, c, d, e, f, g)                      \
  do                                                                           \
  {                                                                            \
    WG14_SIGNALS_SIGFENCE_BARRIER();                                           \
    WG14_SIGNALS_SIGFENCE_ESCAPE(a, 0);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(b, 1);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(c, 2);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(d, 3);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(e, 4);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(f, 5);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(g, 6);                                        \
    WG14_SIGNALS_SIGFENCE_BARRIER();                                           \
  } while(0)
#define WG14_SIGNALS_SIGFENCE_IMPL_8(a, b, c, d, e, f, g, h)                   \
  do                                                                           \
  {                                                                            \
    WG14_SIGNALS_SIGFENCE_BARRIER();                                           \
    WG14_SIGNALS_SIGFENCE_ESCAPE(a, 0);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(b, 1);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(c, 2);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(d, 3);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(e, 4);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(f, 5);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(g, 6);                                        \
    WG14_SIGNALS_SIGFENCE_ESCAPE(h, 7);                                        \
    WG14_SIGNALS_SIGFENCE_BARRIER();                                           \
  } while(0)
#endif
/*! \brief A compiler-only memory barrier, including for local variables in the */
/*! argument list. Any variable in the argument list MUST be a lvalue. */
#define sigfence(...)                                                          \
  WG14_SIGNALS_SIGFENCE_CALL_OVERLOAD(WG14_SIGNALS_SIGFENCE_IMPL_, __VA_ARGS__)

#if !defined WG14_SIGNALS_GLIBC_EMBEDDED
/* The declarations below (types, typedefs, SIGGUARDED_FAILURE_VALUE and
   function declarations) are excluded from translation units that define
   WG14_SIGNALS_GLIBC_EMBEDDED (only wg14/embedder_shim.h does): the wg14
   libc objects also include the submodule's own thrd_signal_handle.h with
   the same definitions, and GCC and Clang reject repeated struct/union/
   enum tag definitions in one translation unit even when compatible
   (typedef and function redeclarations would be legal, but the mirror's
   types are incomplete there, so the conditionals would still conflict).
   Consumer translation units never define the marker and see the full
   declaration surface below.  */


  /* The user defined value.  */
  union stdc_siginfo_value
  {
    intptr_t int_value;
    void *ptr_value;
#if defined __cplusplus && __cplusplus >= 201103L
    constexpr stdc_siginfo_value ()
        : int_value (0)
    {
    }
    constexpr stdc_siginfo_value (int v)
        : int_value (v)
    {
    }
    constexpr stdc_siginfo_value (void *v)
        : ptr_value (v)
    {
    }
#endif
  };
  /* A variable of type `union stdc_siginfo_value' whose int_value member
     is -99, returned by sigguarded() when it fails to install the guard.
     On C++11-or-later compilers it is a constexpr variable; on other
     compilers a plain const object initialised to the same value.  */
#if defined __cplusplus && __cplusplus >= 201103L
  constexpr stdc_siginfo_value
  SIGGUARDED_FAILURE_VALUE{-99};
#elif defined __cplusplus
  /* C++98/03: constexpr is unavailable; the first union member
     (int_value) is initialised.  */
  static const union stdc_siginfo_value SIGGUARDED_FAILURE_VALUE = {-99};
#else
  static const union stdc_siginfo_value
  SIGGUARDED_FAILURE_VALUE = { .int_value = -99 };
#endif

  /* Typedef to a system specific error code type.  */
  typedef int stdc_siginfo_error_code_t;

  /* Placeholder types for the OS specific `siginfo_t *' and `ucontext_t *'
     (POSIX).  */
  typedef siginfo_t stdc_siginfo_siginfo_t;
  typedef ucontext_t stdc_siginfo_context_t;

  struct sig_global_state_tss_state_per_frame_t;
  struct sighandler_info;
  struct global_signal_decider_t;

  /* A platform independent subset of siginfo_t.  */
  struct stdc_siginfo
  {
    int signo;  /* The signal raised.  */

    /* The system specific error code for this signal, the si_errno code
       (POSIX).  Zero when raw_info was null and an implementation-defined
       default signal information was constructed (e.g.
       stdc_raise(signo, NULL, NULL)).  */
    stdc_siginfo_error_code_t error_code;
    void *addr;  /* Memory location which caused fault, if appropriate.
                    NULL when raw_info was null.  */
    union stdc_siginfo_value value;  /* A user-defined value.  */

    /* The OS specific signal info.  A stdc_raise(signo, NULL, NULL) constructs
       an implementation-defined default signal information object and a
       context object representing the calling stack frame, and sets raw_info
       and raw_context to point at them.  */
    stdc_siginfo_siginfo_t *raw_info;
    /* The OS specific ucontext_t (POSIX).  */
    stdc_siginfo_context_t *raw_context;

    /* Used internally only.  */
    struct sig_global_state_tss_state_per_frame_t *internal_local_decider;
    struct sighandler_info *internal_sighandler;
    struct global_signal_decider_t *internal_global_decider;
    bool internal_decider_is_abandoned;
  };

  /* The type of the guarded function.  */
  typedef union stdc_siginfo_value (sig_func_t) (union stdc_siginfo_value);

  /* The type of the function called to recover from a signal being raised
     in a guarded section.  */
  typedef union stdc_siginfo_value (sig_recover_t) (
  const struct stdc_siginfo *);

  /* The decision taken by the decider function.  */
  enum sig_decision
  {
    /* We have decided to do nothing.  */
    sig_decision_next_decider,
    /* We have fixed the cause of the signal, please resume execution.  */
    sig_decision_resume_execution,
    /* Thread local signal deciders only: reset the stack and local state
       to entry to sigguarded(), and call the recovery function.  */
    sig_decision_call_recovery
  };

  /* The type of the function called when a signal is raised.  Returns a
     decision of how to handle the signal.  */
  typedef enum sig_decision (sig_decide_t) (struct stdc_siginfo *);

  /* The type of the function called by the library to take a signal's
     default action, from within its signal machinery, when the previously
     installed disposition for that signal was SIG_DFL.  `signo' is the
     signal whose default action is being taken, `raw_info' the raw OS
     siginfo_t * of the original delivery, and `raw_context' the raw
     ucontext_t *; either may be NULL when the original delivery carried no
     OS info.  The callback must be async-signal-safe.  */
  typedef void (sig_default_action_np_t) (
  int signo, stdc_siginfo_siginfo_t *raw_info, void *raw_context);

  /* THREADSAFE ASYNC-SIGNAL-SAFE Fills the set of synchronous signals for
     this platform (SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGPIPE, SIGSEGV,
     SIGSYS).  */
  extern int sigfillset_synchronous (sigset_t *set);

  /* THREADSAFE ASYNC-SIGNAL-SAFE Fills the set of non-debug asynchronous
     signals for this platform (SIGALRM, SIGCHLD, SIGCONT, SIGHUP, SIGINT,
     SIGKILL, SIGSTOP, SIGTERM, SIGTSTP, SIGTTIN, SIGTTOU, SIGUSR1, SIGUSR2,
     SIGPOLL, SIGPROF, SIGURG, SIGVTALRM).  */
  extern int sigfillset_asynchronous_nondebug (sigset_t *set);

  /* THREADSAFE ASYNC-SIGNAL-SAFE Fills the set of debug asynchronous
     signals for this platform (SIGQUIT, SIGTRAP, SIGXCPU, SIGXFSZ).  */
  extern int sigfillset_asynchronous_debug (sigset_t *set);

  /* THREADSAFE USUALLY ASYNC-SIGNAL-SAFE Installs a thread-local signal
     guard for the calling thread, and calls the guarded function
     `guarded'.  Returns the value returned by guarded, or recovery.  If
     the per-thread state required by this facility cannot be set up,
     SIGGUARDED_FAILURE_VALUE is returned.  If you will never return from
     `decider', you must call sigdecider_abandon() to let the runtime
     clean up its state.  */
  extern union stdc_siginfo_value sigguarded (const sigset_t *signals,
                                              sig_func_t guarded,
                                              sig_recover_t recovery,
                                              sig_decide_t decider,
                                              union stdc_siginfo_value value);

  /* THREADSAFE ASYNC-SIGNAL-SAFE Lets the decider machinery know you
     won't be returning into it.  Can be called from within sigguarded()'s
     decider functions or global deciders.  You must NOT call this from
     within a recovery.  */
  extern void sigdecider_abandon (struct stdc_siginfo *rsi);

  /* THREADSAFE ASYNC-SIGNAL-SAFE Undoes a prior call of
     sigdecider_abandon().  Do not call except from the same decider
     function previously abandoned.  */
  extern void sigdecider_abandon_resume (struct stdc_siginfo *rsi);

  /* THREADSAFE USUALLY ASYNC-SIGNAL-SAFE Raise a signal into this library's
     own signal handling chain: the calling thread's currently active
     sigguarded() guard frames are walked directly, then the global deciders
     installed for the signal are called, and if none of them handles the
     signal (returns a decision other than sig_decision_next_decider), the
     disposition captured when this library first installed the signal is
     handed the signal.  On Windows a Win32 structured exception is raised
     instead.

     On POSIX this function does NOT call raise(), and it does not require the
     raw signal handler to be installed: the guard-frame walk works with no
     siginstall() at all.  It returns false when no decider handles the signal
     and there is no installed handler for `signo' (i.e. no siginstall() ever
     installed that signal number), when `signo' is zero, when `signo' is not
     a valid signal number, or when the per-thread setup fails; it returns
     true when a decider handles the signal or the previously installed
     handler is handed the signal.  Specify zero for `signo' to ensure the
     calling thread's per-thread state is set up and return immediately doing
     nothing else.

     A NULL raw_info or raw_context is replaced by a constructed
     implementation-defined default: a NULL raw_info yields a default signal
     information object, and a NULL raw_context an object representing the
     calling stack frame; each decider is given a `struct stdc_siginfo' whose
     raw_info and raw_context members point at the supplied or constructed
     objects, which persist at least as long as it.  */
  extern bool sighandle (int signo, stdc_siginfo_siginfo_t *raw_info,
                          stdc_siginfo_context_t *raw_context);

  /* THREADSAFE ASYNC-SIGNAL-SAFE Behaves as if it called the `raise' function
     (7.14.2.4) with the argument `signo', except that the additional
     information raw_info and raw_context is made available to the signal
     deciders.  Because it behaves as if it called raise(), signal deciders
     are invoked for `signo' only if `signo' is activated, and if a signal
     handler is called this function does not return until after the signal
     handler (and any signal deciders) have completed.

     If raw_info is NULL, an implementation-defined default signal information
     object is constructed.  If raw_context is NULL, an object representing
     the calling stack frame is constructed.  The raw_info and raw_context
     members of the `struct stdc_siginfo' passed to deciders are never NULL.

     Returns zero if successful and nonzero if unsuccessful.  If `signo' is
     not a valid signal number, this function is unsuccessful and no signal is
     raised.  */
  extern int stdc_raise (int signo, stdc_siginfo_siginfo_t *raw_info,
                         stdc_siginfo_context_t *raw_context);

  /* THREADSAFE Installs, and potentially enables, the global signal
     handlers for the signals specified by `guarded'.  Each signal
     installed is threadsafe reference counted.  If `guarded' is null, all
     the standard POSIX signals are used.  Returns an opaque handle to
     pass to siguninstall(), or NULL on failure.  */
  extern void *siginstall (const sigset_t *guarded);

  /* THREADSAFE Uninstall a previously installed signal guard.  */
  extern int siguninstall (void *i);

  /* THREADSAFE Uninstall a previously system installed signal guard.  */
  extern int siguninstall_system (int version);

  /* THREADSAFE NOT REENTRANT Create a global signal continuation decider.
     Returns an opaque pointer to the registered decider, or NULL if
     malloc failed.  */
  extern void *signal_decider_create (const sigset_t *guarded,
                                      bool callfirst,
                                      sig_decide_t decider,
                                      union stdc_siginfo_value value);

  /* THREADSAFE Destroy a global signal continuation decider.  */
  extern int signal_decider_destroy (void *decider);

  /* THREADSAFE (POSIX only) Sets the sa_flags used when the library
     installs its raw signal handler.  The default is SA_SIGINFO |
     SA_NOCLDWAIT | SA_NODEFER; on glibc the embedder strips SA_NOCLDWAIT
     so POSIX wait()/zombie semantics survive.  The value set here applies
     to every raw-handler installation performed after this call;
     signals already installed keep the flags they were installed with.  */
  extern int siginstall_set_sa_flags_np (int sa_flags);

  /* THREADSAFE (POSIX only) Sets the function the library calls to take a
     signal's default action when the previously installed disposition for
     that signal was SIG_DFL, from within its signal machinery.  */
  extern int siginstall_set_default_action_np (sig_default_action_np_t fn);

#endif /* !WG14_SIGNALS_GLIBC_EMBEDDED */

#endif /* __USE_GNU */

__END_DECLS

#endif /* not signal.h */
