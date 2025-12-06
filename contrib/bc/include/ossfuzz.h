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
 * Declarations for the OSS-Fuzz build of bc and dc.
 *
 */

#include <stdint.h>
#include <stdlib.h>

#ifndef BC_OSSFUZZ_H
#define BC_OSSFUZZ_H

/// The number of args in fuzzer arguments, including the NULL terminator.
extern const size_t bc_fuzzer_args_len;

/// The standard arguments for the bc fuzzer with the -c argument.
extern const char* bc_fuzzer_args_c[];

/// The standard arguments for the bc fuzzer with the -C argument.
extern const char* bc_fuzzer_args_C[];

/// The standard arguments for the dc fuzzer with the -c argument.
extern const char* dc_fuzzer_args_c[];

/// The standard arguments for the dc fuzzer with the -C argument.
extern const char* dc_fuzzer_args_C[];

/// The data pointer.
extern uint8_t* bc_fuzzer_data;

/**
 * The function that the fuzzer runs.
 * @param Data  The data.
 * @param Size  The number of bytes in @a Data.
 * @return      0 on success, -1 on error.
 * @pre         @a Data must not be equal to NULL if @a Size > 0.
 */
int
LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size);

/**
 * The initialization function for the fuzzer.
 * @param argc  A pointer to the argument count.
 * @param argv  A pointer to the argument list.
 * @return      0 on success, -1 on error.
 */
int
LLVMFuzzerInitialize(int* argc, char*** argv);

#endif // BC_OSSFUZZ_H
