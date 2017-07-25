/*
 * Utility routines for writing floating point tests.
 *
 * Currently provides only one function, which checks whether a double is
 * equal to an expected value within a given epsilon.  This is broken into a
 * separate source file from the rest of the basic C TAP library because it
 * may require linking with -lm on some platforms, and the package may not
 * otherwise care about floating point.
 *
 * This file is part of C TAP Harness.  The current version plus supporting
 * documentation is at <https://www.eyrie.org/~eagle/software/c-tap-harness/>.
 *
 * Copyright 2008, 2010, 2012, 2013, 2014, 2015, 2016
 *     Russ Allbery <eagle@eyrie.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/* Required for isnan() and isinf(). */
#if defined(__STRICT_ANSI__) || defined(PEDANTIC)
# ifndef _XOPEN_SOURCE
#  define _XOPEN_SOURCE 600
# endif
#endif

#include <math.h>
#include <stdarg.h>
#include <stdio.h>

#include <tests/tap/basic.h>
#include <tests/tap/float.h>

/*
 * Takes an expected double and a seen double and assumes the test passes if
 * those two numbers are within delta of each other.
 */
int
is_double(double wanted, double seen, double epsilon, const char *format, ...)
{
    va_list args;
    int success;

    va_start(args, format);
    fflush(stderr);
    if ((isnan(wanted) && isnan(seen))
        || (isinf(wanted) && isinf(wanted) == isinf(seen))
        || fabs(wanted - seen) <= epsilon) {
        success = 1;
        okv(1, format, args);
    } else {
        success = 0;
        diag("wanted: %g", wanted);
        diag("  seen: %g", seen);
        okv(0, format, args);
    }
    va_end(args);
    return success;
}
