/*
 * Copyright (c) 1998-2001 Katsushi Kobayashi and Hidetoshi Shimokawa
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the acknowledgement as bellow:
 *
 *    This product includes software developed by K. Kobayashi and H. Shimokawa
 *
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * $FreeBSD$
 *
 */
struct ciphdr {
#if BYTE_ORDER == LITTLE_ENDIAN
	u_int32_t src:8,
		  len:8,
		  dummy:8,
		  dbc:8;
	u_int32_t :16,
		  cyc:16;
#else
	u_int32_t dbc:8,
		  dummy:8,
		  len:8,
		  src:8;
	u_int32_t cyc:16,
		  :16;
#endif
};
struct dvdbc{
#if BYTE_ORDER == BIG_ENDIAN
	u_int32_t :8,
		  dbn:8,
		  rsv0:3,
		  z:1,
		  dseq:4,
		  seq:4,
		  rsv1:1,
		  sct:3;
#else
	u_int32_t seq:4,
		  :1,
		  sct:3,
		  :3,
		  z:1,
		  dseq:4,
		  dbn:8,
		  :8;
#endif
	u_int32_t ld[19];
};
