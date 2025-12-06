/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2025 Gavin D. Howard and contributors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * *****************************************************************************
 *
 * Definitions for processing command-line arguments.
 *
 */

#ifndef BC_ARGS_H
#define BC_ARGS_H

#include <status.h>
#include <opt.h>
#include <vm.h>

/**
 * Processes command-line arguments.
 * @param argc        How many arguments there are.
 * @param argv        The array of arguments.
 * @param exit_exprs  True if bc/dc should exit when there are expressions,
 *                    false otherwise.
 * @param scale       A pointer to return the scale that the arguments set, if
 *                    any.
 * @param ibase       A pointer to return the ibase that the arguments set, if
 *                    any.
 * @param obase       A pointer to return the obase that the arguments set, if
 *                    any.
 */
void
bc_args(int argc, const char* argv[], bool exit_exprs, BcBigDig* scale,
        BcBigDig* ibase, BcBigDig* obase);

#if BC_ENABLED

#if DC_ENABLED

/// Returns true if the banner should be quieted.
#define BC_ARGS_SHOULD_BE_QUIET (BC_IS_DC || vm->exprs.len > 1)

#else // DC_ENABLED

/// Returns true if the banner should be quieted.
#define BC_ARGS_SHOULD_BE_QUIET (vm->exprs.len > 1)

#endif // DC_ENABLED

#else // BC_ENABLED

/// Returns true if the banner should be quieted.
#define BC_ARGS_SHOULD_BE_QUIET (BC_IS_DC)

#endif // BC_ENABLED

// A reference to the list of long options.
extern const BcOptLong bc_args_lopt[];

#endif // BC_ARGS_H
