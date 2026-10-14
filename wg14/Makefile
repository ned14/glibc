# Makefile for the WG14 N3924 improved-C-signals API (wg14_signals).
# Copyright (C) 2026 Free Software Foundation, Inc.
# This file is part of the GNU C Library.

# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.

# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, see
# <https://www.gnu.org/licenses/>.

# The WG14 N3924 improved-C-signals reference implementation
# (https://github.com/ned14/wg14_signals, Apache-2.0, pinned at
# 2217d72eb0d1266eb8e97f2b5df0e83792b73867) is mounted as a pristine git
# submodule at wg14_signals/ and compiled into libc unmodified: the four
# POSIX C sources under wg14_signals/src/wg14_signals are this subdir's
# routines, with `-I $(..)wg14_signals/include` and the force-included
# embedder shim (embedder_shim.h) defining the override hooks.  The objects
# default to MODULE_NAME=libc and therefore get glibc's
# -ftls-model=initial-exec - exactly the TLS model the reference's
# async-signal-safe TLS requires.  All N3924 declarations live in the
# existing installed <signal.h> / <threads.h> mirrors; the public ABI is
# the Versions file here.  See plans/glibc-fork.md.
subdir := wg14

include ../Makeconfig

# Hurd: gated out.  The reference's default WG14_SIGNALS_GETTID
# (syscall(SYS_gettid)) does not compile on Hurd (no syscall()), and Hurd's
# signal semantics are mach-message based rather than rt_sigaction based;
# the coexistence analysis in plans/glibc-fork.md "Comprehensive review" is
# Linux-specific.  This gating must not require the submodule to exist at
# all, so the missing-submodule guard lives inside the non-Hurd branch
# below.  Note the Hurd host_os `gnu' is mutated to `gnu-gnu' by
# configure.ac ("The abi-tags file uses a fairly simplistic model...").
ifeq ($(config-os),gnu-gnu)
# Nothing to build or test on Hurd: the subdir contributes no routines, no
# tests, and no installed headers.  The <signal.h>/<threads.h> mirror
# blocks stay declared (glibc tolerates declared-but-unimplemented APIs)
# until the Hurd backend is assessed (plans/glibc-fork.md Phase 6).
else

# Missing-submodule guard: a non-recursive clone must fail with a clear
# error and recovery instructions instead of puzzling compile failures.
ifneq ($(wildcard $(..)wg14_signals/include/wg14_signals/config.h),)

# The four POSIX backend sources; the Windows backend and the C++ fallback
# are not compiled (glibc is POSIX-only and supplies
# __cxa_thread_atexit_impl).  The objects reach libc automatically: each
# subdir's routines objects are archived per-subdir and concatenated into
# libc.a/libc_pic.a, which is what libc.so links.
routines := current_thread_id tss_async_signal_safe thrd_signal_handle_posix \
	    thread_atexit

# Resolve the routines' .c files from the submodule's source directory
# (glibc's object rules are pattern rules subject to vpath).
vpath %.c $(..)wg14_signals/src/wg14_signals

# Subdir-wide flags: the submodule's include directory (the .c sources
# include "wg14_signals/detail/impl/<name>.c.ipp", the .ipps pull the public
# headers relatively).  Applies to the subdir's tests too.
wg14-CPPFLAGS = -I$(..)wg14_signals/include

# The embedder shim must NOT be delivered via $(subdir)-CPPFLAGS: that slot
# precedes the libc-modules.h/libc-symbols.h force-includes in the CPPFLAGS
# composition (Makeconfig:1123-1131), and the shim's own #include <signal.h>
# would then parse the internal headers (include/sys/cdefs.h etc.) before
# IS_IN/libc_hidden_proto/rtld_hidden_proto are defined - a cascade of
# -Werror failures.  The per-basename CPPFLAGS slot is appended AFTER those
# force-includes (Makeconfig:1130), so the shim (and its <signal.h>) parses
# with the libc build macros already in effect.
# The shim is applied ONLY to the four library objects, NOT to the tests:
# the shim defines WG14_SIGNALS_GLIBC_EMBEDDED, which excludes the mirror's
# type/typedef/declaration block from <signal.h> - and the tests need that
# block (they use the installed-header declaration of the N3924 API).
CPPFLAGS-current_thread_id += -include $(..)wg14/embedder_shim.h
CPPFLAGS-tss_async_signal_safe += -include $(..)wg14/embedder_shim.h
CPPFLAGS-thrd_signal_handle_posix += -include $(..)wg14/embedder_shim.h
CPPFLAGS-thread_atexit += -include $(..)wg14/embedder_shim.h

# Config.h tests `#if __STDC_VERSION__ >= 202300L || __cplusplus` with a
# bare `__cplusplus` (a numeric preprocessor test), which glibc's
# -Wundef (fatal via -Werror here) rejects in C mode.  The submodule is
# zero-drift, so downgrade -Wundef to a warning for the vendored TUs (the
# plan's anticipated per-source -Wno-* carve-out).
CFLAGS-.o += -Wno-error=undef
CFLAGS-.os += -Wno-error=undef

tests := \
  tst-wg14-abort \
  tst-wg14-chain \
  tst-wg14-current-thread-id \
  tst-wg14-default-action \
  tst-wg14-guard-abandon \
  tst-wg14-handoff \
  tst-wg14-install-semantics \
  tst-wg14-raise-semantics \
  tst-wg14-sa-flags-np \
  tst-wg14-sigfence \
  tst-wg14-sigfillsets \
  tst-wg14-sigguarded \
  tst-wg14-stdc-raise \
  tst-wg14-thread-handle \
  tst-wg14-tss \
# tests

# Mirror sync check (tests-special): diffs the N3924 declaration blocks in
# the installed <signal.h>/<threads.h> against the submodule's headers
# after a small normalisation, so the mirrors cannot drift from the
# reference (plans/glibc-fork.md "Public API surface").
tests-special += $(objpfx)check-wg14-mirror.out

$(objpfx)check-wg14-mirror.out: \
  $(..)wg14/check-wg14-mirror.py \
  $(..)signal/signal.h \
  $(..)sysdeps/pthread/threads.h \
  $(..)wg14_signals/include/wg14_signals/thrd_signal_handle.h \
  $(..)wg14_signals/include/wg14_signals/tss_async_signal_safe.h \
  $(..)wg14_signals/include/wg14_signals/current_thread_id.h
	$(PYTHON) $< $(realpath $(..)) > $@ 2>&1; \
	$(evaluate-test)

else

$(error The wg14_signals submodule is missing at $(..)wg14_signals. \
Run `git submodule update --init wg14_signals` in the glibc tree root, \
then rebuild.)

endif # submodule present

endif # not Hurd

include ../Rules