/*
 * Copyright (c) 1998-2002 Katsushi Kobayashi and Hidetoshi Shimokawa
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
#define CSRKEY_MVID	0x3
#define CSRKEY_NCAP	0xc
#define CSRKEY_NUNQ	0x8d
#define CSRKEY_NPWR	0x30
#define CSRKEY_SPEC	0x12
#define	CSRVAL_1394TA	0x00a02d
#define	CSRVAL_ANSIT10	0x00609e
#define CSRKEY_VER	0x13
#define	CSR_PROTAVC	0x010001
#define	CSR_PROTCAL	0x010002
#define	CSR_PROTEHS	0x010004
#define	CSR_PROTHAVI	0x010008
#define	CSR_PROTCAM104	0x000100
#define	CSR_PROTCAM120	0x000101
#define	CSR_PROTCAM130	0x000102
#define	CSR_PROTDPP	0x0a6be2
#define	CSR_PROTIICP	0x4b661f

#define	CSRVAL_T10SBP2	0x010483

struct csrreg {
	u_int32_t val:24,
		  key:8;
};
struct csrhdr {
	u_int32_t crc:16,
		  crc_len:8,
		  info_len:8;
};
struct csrdirectory {
	u_int32_t crc:16,
		  crc_len:16;
	struct csrreg entry[0];
};
struct csrtext {
	u_int32_t crc:16,
		  crc_len:16;
	u_int32_t spec_id:16,
		  spec_type:16;
	u_int32_t lang_id;
	u_int32_t text[0];
};
struct businfo {
	u_int32_t crc:16,
		  crc_len:8,
		  :12,
		  max_rec:4,
		  clk_acc:8,
		  :4,
		  bmc:1,
		  isc:1,
		  cmc:1,
		  irmc:1;
	u_int32_t c_id_hi:8,
		  v_id:24;
	u_int32_t c_id_lo;
};
