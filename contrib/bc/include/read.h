/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2021 Gavin D. Howard and contributors.
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
 * Code to handle special I/O for bc.
 *
 */

#ifndef BC_READ_H
#define BC_READ_H

#include <stdlib.h>

#include <status.h>
#include <vector.h>

/**
 * Returns true if @a c is a non-ASCII (invalid) char.
 * @param c  The character to test.
 * @return   True if @a c is an invalid char.
 */
#define BC_READ_BIN_CHAR(c) (!(c))

/**
 * Reads a line from stdin after printing prompt, if desired.
 * @param vec     The vector to put the stdin data into.
 * @param prompt  The prompt to print, if desired.
 */
BcStatus
bc_read_line(BcVec* vec, const char* prompt);

/**
 * Read a file and return a buffer with the data. The buffer must be freed by
 * the caller.
 * @param path  The path to the file to read.
 */
char*
bc_read_file(const char* path);

/**
 * Helper function for reading characters from stdin. This takes care of a bunch
 * of complex error handling. Thus, it returns a status instead of throwing an
 * error, except for fatal errors.
 * @param vec     The vec to put the stdin into.
 * @param prompt  The prompt to print, if desired.
 */
BcStatus
bc_read_chars(BcVec* vec, const char* prompt);

/**
 * Read a line from buf into vec.
 * @param vec      The vector to read data into.
 * @param buf      The buffer to read from.
 * @param buf_len  The length of the buffer.
 */
bool
bc_read_buf(BcVec* vec, char* buf, size_t* buf_len);

#endif // BC_READ_H
