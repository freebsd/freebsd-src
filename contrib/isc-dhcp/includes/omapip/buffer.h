/* buffer.h

   Definitions for the object management API protocol buffering... */

/*
 * Copyright (c) 1996-1999 Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about the Internet Software Consortium, see
 * ``http://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

/* OMAPI buffers are ring buffers, which means that the beginning of the
   buffer and the end of the buffer chase each other around.   As long as
   the tail never catches up to the head, there's room in the buffer for
   data.

	- If the tail and the head are equal, the buffer is empty.

	- If the tail is less than the head, the contents of the buffer
	  are the bytes from the head to the end of buffer, and in addition,
	  the bytes between the beginning of the buffer and the tail, not
	  including the byte addressed by the tail.

	- If the tail is greater than the head, then the buffer contains
	  valid bytes starting with the byte addressed by the head, and
	  ending with the byte before the byte addressed by the tail.

   There will always be at least one byte of waste, because the tail can't
   increase so that it's equal to the head (that would represent an empty
   buffer. */
#define OMAPI_BUF_SIZE 4048
typedef struct _omapi_buffer {
	struct _omapi_buffer *next;	/* Buffers can be chained. */
	u_int32_t refcnt;		/* Buffers are reference counted. */
	u_int16_t head, tail;		/* Buffers are organized in a ring. */
	char buf [OMAPI_BUF_SIZE];	/* The actual buffer is included in
					   the buffer data structure. */
} omapi_buffer_t;	

#define BUFFER_BYTES_FREE(x)	\
	((x) -> tail > (x) -> head \
	  ? sizeof ((x) -> buf) - ((x) -> tail - (x) -> head) \
	  : (x) -> head - (x) -> tail)

#define BYTES_IN_BUFFER(x)	\
	((x) -> tail > (x) -> head \
	 ? (x) -> tail - (x) -> head - 1 \
	 : sizeof ((x) -> buf) - ((x) -> head - (x) -> tail) - 1)

isc_result_t omapi_connection_require (omapi_object_t *, unsigned);
isc_result_t omapi_connection_copyout (unsigned char *,
				       omapi_object_t *, unsigned);
isc_result_t omapi_connection_copyin (omapi_object_t *,
				      const unsigned char *, unsigned);
isc_result_t omapi_connection_flush (omapi_object_t *);
isc_result_t omapi_connection_get_uint32 (omapi_object_t *, u_int32_t *);
isc_result_t omapi_connection_put_uint32 (omapi_object_t *, u_int32_t);
isc_result_t omapi_connection_get_uint16 (omapi_object_t *, u_int16_t *);
isc_result_t omapi_connection_put_uint16 (omapi_object_t *, u_int32_t);

