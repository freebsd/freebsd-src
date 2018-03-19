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

#ifndef PARSE_H
#define PARSE_H

#include "yasm.h"

#include "intel-pt.h"

#if defined(FEATURE_PEVENT)
#  include "pevent.h"
#endif /* defined(FEATURE_PEVENT) */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>


#if defined(FEATURE_SIDEBAND)

/* The sideband format. */
enum sb_format {
	sbf_raw,

#if defined(FEATURE_PEVENT)
	sbf_pevent,
#endif /* defined(FEATURE_PEVENT) */
};

/* A sideband file. */
struct sb_file {
	/* The file name. */
	char *name;

	/* The file pointer. */
	FILE *file;

	/* The sideband format. */
	enum sb_format format;

	/* The number of bytes written into the sideband file. */
	int bytes_written;

	/* Format-specific information. */
	union {
		/* A dummy entry. */
		uint64_t dummy;

#if defined(FEATURE_PEVENT)
		/* format = sbf_pevent. */
		struct {
			/* The perf_event sideband configuration. */
			struct pev_config config;

			/* If set, the configuration can't be changed. */
			uint32_t is_final:1;
		} pevent;
#endif /* defined(FEATURE_PEVENT) */
	} variant;
};

/* A list of sideband files. */
struct sb_filelist {
	/* The next file in the list. */
	struct sb_filelist *next;

	/* The sideband file. */
	struct sb_file sbfile;
};

#endif /* defined(FEATURE_SIDEBAND) */

/* Represents the parser.  */
struct parser {
	/* File pointer to the trace output file.  */
	FILE *ptfile;

	/* Filename of the trace output file.  The filename is
	 * determined from the .asm file given during p_alloc.
	 */
	char *ptfilename;

#if defined(FEATURE_SIDEBAND)
	/* A list of open sideband files. */
	struct sb_filelist *sbfiles;

	/* The currently active sideband file. */
	struct sb_file *current_sbfile;
#endif /* defined(FEATURE_SIDEBAND) */

	/* The yasm structure, initialized with pttfile in p_alloc.  */
	struct yasm *y;

	/* Current pt directive.  */
	struct pt_directive *pd;

	/* The encoder configuration, passed during p_alloc.  */
	const struct pt_config *conf;

	/* Labels for @pt or @sb directives.  */
	struct label *pt_labels;

	/* Number of bytes written to pt file.  */
	int pt_bytes_written;
};

/* Instantiates a parser and starts parsing of @pttfile and writes PT
 * stream using @conf.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 */
extern int parse(const char *pttfile, const struct pt_config *conf);

/* Parses an empty payload.
 *
 * Returns 0 on success; a negative enum errcode othewise.
 * Returns -err_parse_trailing_tokens if @payload has non whitespace
 * characters.
 */
extern int parse_empty(char *payload);

/* Parses tnt @payload.  Takens are expressed with 't' and Not-Takens
 * with 'n'.  The t's and n's can be separated with spaces, periods or
 * directly concatenated.
 *
 * On success the TNT bitfield will be stored in the location of @tnt; the
 * number of T's and N's is stored in the location of @size.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_internal if @payload or @tnt or @size is the NULL
 * pointer.
 * Returns -err_parse_unknown_char if there is an unrecognized character
 * in the payload.
 */
extern int parse_tnt(uint64_t *tnt, uint8_t *size, char *payload);

/* Parses an address and a ipc from @payload and stores it in the
 * location of @ip and @ipc respectively.  The ipc is separated from the
 * address with space or comma.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_internal if @p or @ip or @ipc is the NULL pointer.
 * Returns -err_parse_int if ip or ipc in the @payload could not be
 * parsed as integer.
 * Returns -err_parse_ipc if the ipc argument is missing or malformed.
 * Returns -err_parse_trailing_tokens if the @payload contains more than
 * 2 arguments.
 */
extern int parse_ip(struct parser *p, uint64_t *ip,
		    enum pt_ip_compression *ipc, char *payload);

/* Parses a uint64_t value from @payload and stores it in the memory
 * location where @x points to.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_internal if @x is the NULL pointer.
 * Returns -err_parse_no_args if @payload contains no arguments.
 * Returns -err_parse_int if @payload cannot be parsed as integer.
 */
extern int parse_uint64(uint64_t *x, char *payload);

/* Parses a uint8_t value from @payload and stores it in the memory
 * location where @x points to.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_internal if @x is the NULL pointer.
 * Returns -err_parse_no_args if @payload contains no arguments.
 * Returns -err_parse_int if @payload cannot be parsed as integer.
 * Returns -err_parse_int_too_big if the integer parsed from @payload
 * cannot be represented in uint8_t.
 */
extern int parse_uint8(uint8_t *x, char *payload);

/* Parses a uint16_t value from @payload and stores it in the memory
 * location where @x points to.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_internal if @x is the NULL pointer.
 * Returns -err_parse_no_args if @payload contains no arguments.
 * Returns -err_parse_int if @payload cannot be parsed as integer.
 * Returns -err_parse_int_too_big if the integer parsed from @payload
 * cannot be represented in uint16_t.
 */
extern int parse_uint16(uint16_t *x, char *payload);

/* Parses a uint32_t value from @payload and stores it in the memory
 * location where @x points to.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_internal if @x is the NULL pointer.
 * Returns -err_parse_no_args if @payload contains no arguments.
 * Returns -err_parse_int if @payload cannot be parsed as integer.
 * Returns -err_parse_int_too_big if the integer parsed from @payload
 * cannot be represented in uint32_t.
 */
extern int parse_uint32(uint32_t *x, char *payload);

/* Parses the comma-separated ctc and fc arguments of a tma packet.
 *
 * Returns 0 on success; a negative enum errcode otherwise.
 * Returns -err_internal if @ctc or @fc is the NULL pointer.
 * Returns -err_parse_int if ctc or fc in the @payload could not be
 * parsed as integer.
 * Returns -err_parse_trailing_tokens if the @payload contains more than
 * 2 arguments.
 */
extern int parse_tma(uint16_t *ctc, uint16_t *fc, char *payload);

#endif /* PARSE_H */
