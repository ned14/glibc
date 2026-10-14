/* Embedder override layer for the wg14_signals submodule inside glibc.
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

/* The WG14 N3924 improved-C-signals reference implementation
   (wg14_signals, Apache-2.0) is mounted as a pristine git submodule at
   wg14_signals/ and consumed unmodified: its four POSIX C sources under
   wg14_signals/src/wg14_signals are compiled as libc objects by the wg14
   subdir.  This header is force-included (-include) ahead of every one of
   those translation units and defines the embedder override hooks the
   submodule's config.h documents (WG14_SIGNALS_SIGACTION / ABORT /
   KILL_SELF / GETTID and the memory/sigset/setjmp/pthread-key families).
   It must be included BEFORE wg14_signals/config.h, hence the force
   include.  This file is part of the fork, never of the submodule: the
   submodule is never patched.

   The POSIX backend calls, by name, the very functions this libc is
   implementing on top of it - sigaction(), abort(),
   pthread_kill(pthread_self(), ...), syscall(SYS_gettid) - backed by
   glibc's own internal entrypoints (see plans/glibc-fork.md,
   "Embedder override layer").  Only ONE hook is functionally overridden:
   WG14_SIGNALS_SIGACTION is routed through __sigaction (glibc's
   validating, internal-signal-rejecting, abort-lock-taking entrypoint)
   with SA_NOCLDWAIT stripped so POSIX wait()/zombie semantics survive.
   Everything else resolves naturally against glibc's real entrypoints:
   abort() itself already meets the hook's contract (it resets SIGABRT to
   SIG_DFL before re-raising and cannot loop), pthread_kill/self and
   syscall() are glibc's own, and the sigset/setjmp/memory helpers are
   glibc's public or hidden symbols.  */

#ifndef WG14_SIGNALS_GLIBC_EMBEDDER_SHIM_H
#define WG14_SIGNALS_GLIBC_EMBEDDER_SHIM_H

/* NULL for the wrapper below (glibc's own headers provide it too).  */
#include <stddef.h>

/* Expose NSIG, SIGSYS/SIGXCPU/SIGXFSZ, ucontext_t, syscall() and the
   internal-signal-free sigset helpers in <signal.h>/<unistd.h>;
   belt-and-braces for the libc objects (include/libc-symbols.h already
   defines it), load-bearing for the subdir's tests.  */
#define _GNU_SOURCE 1

/* Reference library build mode.  */
#define WG14_SIGNALS_SOURCE 1

/* glibc provides _setjmp/_longjmp, so the reference selects them
   (config.h:273-285, thrd_signal_handle_posix.c.ipp:43-55).  */
#define WG14_SIGNALS_HAVE__SETJMP 1

/* glibc supplies __cxa_thread_atexit_impl (GLIBC_2.18); map the name the
   reference calls onto it.  The pthread-key fallback in thread_atexit.c.ipp
   is excluded.  */
#define WG14_SIGNALS_HAVE__CXA_THREAD_ATEXIT 1
#define __cxa_thread_atexit __cxa_thread_atexit_impl

/* glibc's own <signal.h> provides the sigfence macro and
   SIGGUARDED_FAILURE_VALUE mirrors (signal/signal.h, __USE_GNU); suppress
   the submodule's copies so there is no macro/duplicate-symbol
   redefinition in translation units that see both headers (the embedder
   guards upstreamed in the submodule's config.h/thrd_signal_handle.h).  */
#define WG14_SIGNALS_DISABLE_SIGFENCE_MACRO 1
#define WG14_SIGNALS_DISABLE_SIGGUARDED_FAILURE_VALUE 1
/* Identical repeated struct/union/enum TAG definitions in one
   translation unit are a constraint violation in GCC and Clang even when
   compatible (typedef and function redeclarations are fine), and the
   wg14 translation units see both glibc's <signal.h> mirror and the
   submodule's own thrd_signal_handle.h.  The mirror therefore excludes
   its whole declarations region (types, typedefs, SIGGUARDED_FAILURE_VALUE
   and function declarations) from translation units that define this
   marker - only the embedder shim defines it; consumer translation units
   never do, so the installed header is unaffected
   (plans/glibc-fork.md "Public API surface", conflicts).  */
#define WG14_SIGNALS_GLIBC_EMBEDDED 1

/* The INTERNAL <signal.h> (include/signal.h), not the public
   <signal/signal.h>: the internal header re-includes the public one and
   additionally declares the libc_hidden_proto hidden aliases for the
   sigset helpers, __libc_current_sigrtmin/max, __sigaction etc., so the
   wg14 objects' host calls bind hidden (no PLT) exactly like every other
   libc object.  This is safe because the shim is force-included AFTER
   libc-symbols.h (per-basename CPPFLAGS slot, Makeconfig:1130) - the
   macro libc_hidden_proto is already defined when the shim parses; the
   fork's original include of the public header (before the force-include
   order fix) pre-defined _SIGNAL_H and silently disabled the internal
   header's whole hidden-prototype and sigsetops section.  The mirror's
   sigfence()/SIGGUARDED_FAILURE_VALUE/declarations are inactive in the
   wg14 translation units (WG14_SIGNALS_GLIBC_EMBEDDED is defined above
   and the DISABLE defines keep the submodule's own copies suppressed).  */
#include <signal.h>

/* Bind to glibc's hidden internal alias of __sigaction (created by
   libc_hidden_def in signal/sigaction.c) exactly as libc_hidden_proto
   in include/signal.h would, so the wg14 machinery's sigaction calls
   cannot be diverted by LD_PRELOAD or an interposed definition - no
   PLT, no PLT interposition.  The later libc_hidden_proto (__sigaction)
   in the .ipps' own <signal.h> include declares the same asm-named
   symbol, which is a compatible duplicate declaration.  */
extern int __sigaction (int, const struct sigaction *__restrict,
			struct sigaction *__restrict)
  __asm__ ("__GI___sigaction") __attribute__ ((visibility ("hidden")));

/* The reference's default WG14_SIGNALS_GETTID() is syscall(SYS_gettid)
   (config.h:213).  glibc maps the asm-generic syscall numbers to SYS_*
   names only for the architectures with a dedicated bits/syscall.h list,
   so on aarch64 and other asm-generic targets SYS_gettid is NOT defined
   by <sys/syscall.h> and the default does not compile inside glibc (the
   standalone suite is unaffected - it builds against host installed
   headers).  glibc's own thread-id query is the hidden __gettid
   (syscalls.list generated syscall wrapper, weak-aliased to the public
   gettid GLIBC_2.30; declared with libc_hidden_proto in
   include/bits/unistd_ext.h): an rt_syscall stub that is async-signal-
   safe and thread-safe, binds via the hidden alias (no PLT) and is what
   nptl itself uses (nptl/pthread_kill.c:79).  */

#define WG14_SIGNALS_GETTID() __gettid ()

/* Route raw-handler installs through glibc's own entrypoint, stripping
   SA_NOCLDWAIT so POSIX wait()/zombie semantics survive (llvm-libc fork
   precedent; N3924 does not specify the raw handler's sa_flags).
   siginstall_set_sa_flags_np() cannot restore the stripped bit (the
   stripped value never reaches the kernel); all other np-set flags
   (SA_NODEFER, SA_ONSTACK, SA_SIGINFO) are effective.  __sigaction
   validates the signal number, rejects glibc's internal signals
   (SIGCANCEL/SIGSETXID) with EINVAL, and takes the abort write lock for
   SIGABRT (query and set alike); the lock interplay is analysed in
   plans/glibc-fork.md "Comprehensive review" item 1.  */
#define WG14_SIGNALS_SIGACTION(signum, act, oldact)                           \
  wg14_glibc_sigaction ((signum), (act), (oldact))

static inline int
wg14_glibc_sigaction (int signum, const struct sigaction *act,
		      struct sigaction *oldact)
{
  struct sigaction act2;
  if (act != NULL)
    {
      act2 = *act;
      act2.sa_flags &= ~SA_NOCLDWAIT;
      act = &act2;
    }
  return __sigaction (signum, act, oldact);
}

#endif /* WG14_SIGNALS_GLIBC_EMBEDDER_SHIM_H */