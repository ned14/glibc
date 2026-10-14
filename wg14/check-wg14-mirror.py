#!/usr/bin/env python3
# Copyright (C) 2026 Free Software Foundation, Inc.
# This file is part of the GNU C Library.
#
# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of the
# License, or (at your option) any later version.
#
# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, see
# <https://www.gnu.org/licenses/>.

"""Sync check for the N3924 declaration mirrors in <signal.h>/<threads.h>.

The WG14 N3924 improved-C-signals API is declared verbatim in glibc's
installed <signal.h> (signal/signal.h) and <threads.h>
(sysdeps/pthread/threads.h), mirrored from the wg14_signals submodule's
own headers (wg14_signals/include/wg14_signals/{thrd_signal_handle,
tss_async_signal_safe,current_thread_id}.h).  The mirror must not drift
from the reference: this script extracts the declaration lines of the
reference headers' PUBLIC API regions and verifies that a normalised form
of each appears in the corresponding glibc header.

Normalisation (applied identically to both sides; the deviations from a
plain textual diff are the documented small list from plans/glibc-fork.md
"Public API surface", "Sync test"):

  * comments removed (the reference uses `//!` doc comments, glibc
    headers use `/* */`; doc text is not API surface)
  * preprocessor-directive lines removed (mode-guard `#if` spellings are
    allowed to differ, e.g. the C++98/constexpr gating of
    SIGGUARDED_FAILURE_VALUE and the reference's `#if __GLIBC__` blocks;
    the code lines under them must still match)
  * WG14_SIGNALS_PREFIX(x) un-macroed to x
  * WG14_SIGNALS_EXTERN -> extern, WG14_SIGNALS_INLINE -> inline
  * WG14_SIGNALS_C23_CONSTEXPR_OR_CONST -> const (the mirror spells its
    own C23/C++ constexpr-vs-const logic glibc-style)
  * WG14_SIGNALS_STATIC_ASSERT -> _Static_assert (the mirror spells the
    sigfence argument-count check glibc-style)
  * `sigfence_sink` renamed to `__wg14_sigfence_sink` (the mirror must
    use a glibc-`__`-prefixed internal name); both spellings normalise to
    the single token SIGFENCE_SINK
  * `(`, `)`, `*`, `{` and `}` tokens removed and all whitespace
    collapsed to single spaces, so glibc's `(*sig_func_t) (...)` and
    `{ .int_value = -99 };` spellings and the reference's macro-derived
    `(sig_func_t))(union ...` line-splits normalise to the same token
    sequence
  * the reference's async-signal-safe-TLS spellings map onto glibc's:
    WG14_SIGNALS_ASYNC_SAFE_THREAD_LOCAL -> __thread, and
    __attribute__ ((tls_model ("initial-exec"))) (any spacing) is
    removed, so `<threads.h>`'s `__thread ... tls_model` declaration
    matches the submodule's current_thread_id_cached declaration;
    WG14_SIGNALS_DEFAULT_VISIBILITY and
    WG14_SIGNALS_IGNORE_MULTIPLE_DEFINITIONS (header-only/Windows
    hygiene, not mirrored) are removed

Every reference line's normalised token sequence must occur contiguously
in the glibc header's normalised token sequence.  A submodule pin bump
that changes any declaration fails this check, prompting a deliberate
mirror update.
"""

import re
import sys


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    return text


def unmacro(text):
    # WG14_SIGNALS_PREFIX(x) -> x (arguments never contain parentheses).
    text = re.sub(r"WG14_SIGNALS_PREFIX\(([^()]*)\)", r"\1", text, flags=re.S)
    text = text.replace("WG14_SIGNALS_EXTERN", "extern")
    text = text.replace("WG14_SIGNALS_INLINE", "inline")
    text = text.replace("WG14_SIGNALS_C23_CONSTEXPR_OR_CONST", "const")
    text = text.replace("WG14_SIGNALS_STATIC_ASSERT", "_Static_assert")
    # The mirror's glibc-`__`-prefixed sink name and the reference's
    # original name converge on one canonical token (the mirror name
    # contains the reference name as a substring, so the longer spelling
    # must be mapped first).
    text = text.replace("__wg14_sigfence_sink", "sigfence_sink")
    text = text.replace("sigfence_sink", "SIGFENCE_SINK")
    # The async-signal-safe TLS idiom: the reference's macro spellings map
    # onto the mirror's glibc spelling (attribute removed; glibc's
    # -ftls-model=initial-exec and the tls_model attribute are equivalent
    # in effect, and <threads.h> spells it the GNU way).
    text = re.sub(r"__attribute__\s*\(\(\s*tls_model\s*\("
                  r'\s*"initial-exec"\s*\)\s*\)\)', " ", text)
    text = text.replace("WG14_SIGNALS_ASYNC_SAFE_THREAD_LOCAL", "__thread")
    text = text.replace("WG14_SIGNALS_DEFAULT_VISIBILITY", " ")
    text = text.replace("WG14_SIGNALS_IGNORE_MULTIPLE_DEFINITIONS", " ")
    return text


def canonical_lines(text):
    text = strip_comments(text)
    text = unmacro(text)
    out = []
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        # Drop parenthesisation, pointer-dereference and brace tokens, then
        # collapse whitespace: glibc's spacing/pointer spelling, the
        # reference's line-split macro artifacts, and the reference's
        # brace-attached `{.int_value = -99};` spellings then match.
        line = line.replace("(", " ").replace(")", " ")
        line = line.replace("*", " ").replace("{", " ").replace("}", " ")
        toks = [t for t in line.split()]
        if toks:
            out.append(" ".join(toks))
    return out


# Canonical forms of the reference lines from the non-__GLIBC__ branches
# of the platform ladders, which the mirror deliberately omits (they are
# inert on glibc; plans/glibc-fork.md "Public API surface").
FORK_DROPS = [
    "typedef long stdc_siginfo_error_code_t;",
    "typedef struct _EXCEPTION_RECORD stdc_siginfo_siginfo_t;",
    "typedef struct siginfo stdc_siginfo_siginfo_t;",
    "typedef struct __siginfo stdc_siginfo_siginfo_t;",
    "typedef struct _CONTEXT stdc_siginfo_context_t;",
    "void internal_win_state;",
]

# The four handler-function typedefs are FUNCTION types in the reference
# (`typedef union stdc_siginfo_value (sig_func_t) (...)`), not pointer
# types: a mirror spelled `(*sig_func_t)` normalises to the same tokens
# (the checker drops `(`/`)`/`*`) but is a different C type, which would
# be a same-TU typedef-redefinition error in the wg14 libc objects that
# see both headers.  Forbid the pointer spellings in the raw mirror text.
BANNED_POINTER_TYPEDEFS = [
    "(*sig_func_t)", "(* sig_func_t)", "( *sig_func_t)",
    "(*sig_recover_t)", "( *sig_recover_t)",
    "(*sig_decide_t)", "( *sig_decide_t)",
    "(*sig_default_action_np_t)", "( *sig_default_action_np_t)",
]


def check_mirror(ref_region, glibc_header_path):
    glibc_lines = canonical_lines(open(glibc_header_path).read())
    glibc_blob = " " + " ".join(glibc_lines) + " "
    return [line for line in canonical_lines(ref_region)
            if line not in FORK_DROPS and line not in glibc_blob]


def check_no_pointer_typedefs(glibc_header_path):
    text = open(glibc_header_path).read()
    return [b for b in BANNED_POINTER_TYPEDEFS if b in text]


def region_of_thrd_signal_handle():
    """The sigfence machinery plus the declarations region of
    thrd_signal_handle.h."""
    text = open(REF_DIR + "/thrd_signal_handle.h").read()
    # The sigfence macro machinery: the embedder-guard block through the
    # closing #endif before the pragma blocks (the mirror keeps it
    # verbatim, sink renamed).
    i = text.index("#ifndef WG14_SIGNALS_DISABLE_SIGFENCE_MACRO")
    j = text.index("#ifdef _MSC_VER", i)
    machinery = text[i:j]
    # The declarations proper, from the stdc_siginfo_value union to the
    # closing of the siginstall_set_default_action_np declaration.
    i = text.index("/*! \\union stdc_siginfo_value")
    j = text.index("#if WG14_SIGNALS_ENABLE_HEADER_ONLY", i)
    decls = text[i:j]
    # Exclude the implementation helper sigguarded_failure_value(): it is
    # only defined so the .ipp implementation compiles when the
    # SIGGUARDED_FAILURE_VALUE definition is embedder-suppressed, and the
    # submodule's own header (which the wg14 libc objects also include)
    # still provides it there; a glibc mirror copy would be a same-TU
    # redefinition conflict.  Remove the whole statement.
    k = decls.index("static WG14_SIGNALS_INLINE union")
    rest = decls[k:]
    m = re.search(r"\n  \}\n", rest)
    decls = decls[:k] + rest[m.end():]
    # The platform ladder: the mirror mirrors only the __GLIBC__ forks
    # (the Windows/Android/BSD typedefs and the Windows-only struct
    # member are never compiled on glibc and are not mirrored); the
    # check_mirror() FORK_DROPS list below drops their canonical lines.
    return machinery + "\n" + decls


def region_of_tss_async_signal_safe():
    text = open(REF_DIR + "/tss_async_signal_safe.h").read()
    i = text.index("typedef struct WG14_SIGNALS_PREFIX(tss_async_signal_safe_s)")
    j = text.index("#if WG14_SIGNALS_ENABLE_HEADER_ONLY", i)
    return text[i:j]


def region_of_current_thread_id():
    text = open(REF_DIR + "/current_thread_id.h").read()
    i = text.index("typedef uintptr_t WG14_SIGNALS_PREFIX(thread_id_t)")
    j = text.index("#if WG14_SIGNALS_ENABLE_HEADER_ONLY", i)
    return text[i:j]


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: check-wg14-mirror.py <glibc-srcdir>")
    srcdir = sys.argv[1].rstrip("/")
    global REF_DIR
    REF_DIR = srcdir + "/wg14_signals/include/wg14_signals"

    failures = []
    pairs = [
        ("signal.h", region_of_thrd_signal_handle(),
         srcdir + "/signal/signal.h"),
        ("threads.h", region_of_tss_async_signal_safe(),
         srcdir + "/sysdeps/pthread/threads.h"),
        ("threads.h", region_of_current_thread_id(),
         srcdir + "/sysdeps/pthread/threads.h"),
    ]
    for header, region, path in pairs:
        for line in check_mirror(region, path):
            failures.append((header, line))
    for banned in check_no_pointer_typedefs(srcdir + "/signal/signal.h"):
        failures.append(("signal.h", "banned pointer typedef " + banned))

    if failures:
        print("check-wg14-mirror: N3924 mirror drift detected:",
              file=sys.stderr)
        for header, line in failures:
            print("  %s: %s" % (header, line), file=sys.stderr)
        print("check-wg14-mirror: update the mirror block in "
              "signal/signal.h and sysdeps/pthread/threads.h to match the "
              "wg14_signals submodule pin.", file=sys.stderr)
        return 1

    print("check-wg14-mirror: N3924 mirror blocks in <signal.h> and "
          "<threads.h> match the wg14_signals submodule")
    return 0


if __name__ == "__main__":
    sys.exit(main())