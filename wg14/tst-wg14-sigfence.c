/* Port of wg14_signals sigfence_fence_test.c to glibc's harness: the
   sigfence() macro's argument-count dispatch and compiler barrier
   behaviour.  On GNU/Clang compilers the extended-inline-asm path is
   compiled; the volatile-sink fallback is for non-GNU compilers, which
   this test only checks for directly where that branch exists.
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

#include <signal.h>

static int
do_test (void)
{
  int a = 1, b = 2, c = 3, d = 4, e = 5, f = 6, g = 7, h = 8;
  /* sigfence() (no args) expands a variadic macro with an empty argument
     list, which is a GNU extension outside C23; -Wpedantic warns.
     Suppress exactly that diagnostic around the zero-arg overload.  */
#if defined __clang__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wunknown-warning-option"
#  pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#  pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#  pragma clang diagnostic ignored "-Wc23-extensions"
#elif defined __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wvariadic-macros"
#endif
  sigfence ();
#if defined __clang__
#  pragma clang diagnostic pop
#elif defined __GNUC__
#  pragma GCC diagnostic pop
#endif
  sigfence (a);
#if !defined __GNUC__ && !defined __clang__
  /* Volatile-sink fallback path: the address of a must have escaped into
     the sink at slot 0 (the barrier only touches slot 8).  */
  if (__wg14_sigfence_sink[0] != (void *) &a)
    return 2;
#endif
  sigfence (a, b);
  sigfence (a, b, c);
  sigfence (a, b, c, d);
  sigfence (a, b, c, d, e);
  sigfence (a, b, c, d, e, f);
  sigfence (a, b, c, d, e, f, g);
  sigfence (a, b, c, d, e, f, g, h);
  return (a == 1 && b == 2 && c == 3 && d == 4 && e == 5 && f == 6 && g == 7
	  && h == 8)
	     ? 0
	     : 1;
}

#include <support/test-driver.c>