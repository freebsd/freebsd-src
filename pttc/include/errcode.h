/*
 * Copyright (c) 2013-2018, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ERRCODE_H
#define ERRCODE_H

/* Error codes.  */
enum errcode {
	success,

	err_file_open,
	err_file_read,
	err_file_size,
	err_file_write,
	err_out_of_range,

	err_label_addr,
	err_no_org_directive,
	err_no_directive,
	err_no_label,
	err_label_name,
	err_label_not_unique,

	err_section_no_name,
	err_section_attribute_no_value,
	err_section_unknown_attribute,

	err_missing_closepar,
	err_missing_openpar,

	err_parse,
	err_parse_int,
	err_parse_int_too_big,
	err_parse_ipc,
	err_parse_ip_missing,
	err_parse_no_args,
	err_parse_trailing_tokens,
	err_parse_unknown_char,
	err_parse_unknown_directive,
	err_parse_missing_directive,

	err_parse_c_state_sub,
	err_parse_c_state_invalid,

	err_sb_missing,
	err_sb_mix,
	err_sb_final,

	err_pt_lib,

	err_run,

	err_other,

	err_no_mem,

	/* Used for all invalid function arguments.  */
	err_internal,

	/* Special return value used in p_process to signal that the
	 * rest of the file should go into a .exp file.
	 */
	stop_process,

	/* Maximum error code.
	 *
	 * This must always be the last element in the enum.
	 * It must not be used as error code.
	 */
	err_max
};

/* Map error codes to descriptions.
 *
 * Note, all error codes, that are returned by functions, are negative,
 * so usually error codes must be negated when accessing this array.
 */
extern const char *errstr[];

#endif /* ERRCODE_H */
