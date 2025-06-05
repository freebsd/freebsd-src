/*
 * Utility functions for tests that use subprocesses.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2009-2010, 2013
 *     The Board of Trustees of the Leland Stanford Junior University
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
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TAP_PROCESS_H
#define TAP_PROCESS_H 1

#include <config.h>
#include <tests/tap/macros.h>

/* Opaque data type for process_start and friends. */
struct process;

BEGIN_DECLS

/*
 * Run a function in a subprocess and check the exit status and expected
 * output (stdout and stderr combined) against the provided values.  Expects
 * the function to always exit (not die from a signal).  data is optional data
 * that's passed into the function as its only argument.
 *
 * This reports as three separate tests: whether the function exited rather
 * than was killed, whether the exit status was correct, and whether the
 * output was correct.
 */
typedef void (*test_function_type)(void *);
void is_function_output(test_function_type, void *data, int status,
                        const char *output, const char *format, ...)
    __attribute__((__format__(printf, 5, 6), __nonnull__(1)));

/*
 * Run a setup program.  Takes the program to run and its arguments as an argv
 * vector, where argv[0] must be either the full path to the program or the
 * program name if the PATH should be searched.  If the program does not exit
 * successfully, call bail, with the error message being the output from the
 * program.
 */
void run_setup(const char *const argv[]) __attribute__((__nonnull__));

/*
 * process_start starts a process in the background, returning an opaque data
 * struct that can be used to stop the process later.  The standard output and
 * standard error of the process will be sent to a log file registered with
 * diag_file_add, so its output will be properly interleaved with the test
 * case output.
 *
 * The process should create a PID file in the path given as the second
 * argument when it's finished initialization.
 *
 * process_start_fakeroot is the same but starts the process under fakeroot.
 * PATH_FAKEROOT must be defined (generally by Autoconf).  If fakeroot is not
 * found, process_start_fakeroot will call skip_all, so be sure to call this
 * function before plan.
 *
 * process_stop can be called to explicitly stop the process.  If it isn't
 * called by the test program, it will be called automatically when the
 * program exits.
 */
struct process *process_start(const char *const argv[], const char *pidfile)
    __attribute__((__nonnull__));
struct process *process_start_fakeroot(const char *const argv[],
                                       const char *pidfile)
    __attribute__((__nonnull__));
void process_stop(struct process *);

END_DECLS

#endif /* TAP_PROCESS_H */
