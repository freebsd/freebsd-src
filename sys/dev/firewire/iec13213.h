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

#define CSRTYPE_SHIFT	6
#define CSRTYPE_MASK	(3 << CSRTYPE_SHIFT)
#define CSRTYPE_I	(0 << CSRTYPE_SHIFT) /* Immediate */
#define CSRTYPE_C	(1 << CSRTYPE_SHIFT) /* CSR offset */
#define CSRTYPE_L	(2 << CSRTYPE_SHIFT) /* Leaf */
#define CSRTYPE_D	(3 << CSRTYPE_SHIFT) /* Directory */

#define CSRKEY_MASK	0x3f
#define CSRKEY_DESC	0x01 /* Descriptor */
#define CSRKEY_BDINFO	0x02 /* Bus_Dependent_Info */
#define CSRKEY_VENDOR	0x03 /* Vendor */
#define CSRKEY_HW	0x04 /* Hardware_Version */
#define CSRKEY_MODULE	0x07 /* Module */
#define CSRKEY_NCAP	0x0c /* Node_Capabilities */
#define CSRKEY_EUI64	0x0d /* EUI_64 */
#define CSRKEY_UNIT	0x11 /* Unit */
#define CSRKEY_SPEC	0x12 /* Specifier_ID */
#define CSRKEY_VER	0x13 /* Version */
#define CSRKEY_DINFO	0x14 /* Dependent_Info */
#define CSRKEY_ULOC	0x15 /* Unit_Location */
#define CSRKEY_MODEL	0x17 /* Model */
#define CSRKEY_INST	0x18 /* Instance */
#define CSRKEY_KEYW	0x19 /* Keyword */
#define CSRKEY_FEAT	0x1a /* Feature */
#define CSRKEY_EROM	0x1b /* Extended_ROM */
#define CSRKEY_EKSID	0x1c /* Extended_Key_Specifier_ID */
#define CSRKEY_EKEY	0x1d /* Extended_Key */
#define CSRKEY_EDATA	0x1e /* Extended_Data */
#define CSRKEY_MDESC	0x1f /* Modifiable_Descriptor */
#define CSRKEY_DID	0x20 /* Directory_ID */
#define CSRKEY_REV	0x21 /* Revision */

#define CROM_TEXTLEAF	(CSRTYPE_L | CSRKEY_DESC)	/* 0x81 */
#define CROM_LUN	(CSRTYPE_I | CSRKEY_DINFO)	/* 0x14 */

/* ???
#define CSRKEY_MVID	0x3
#define CSRKEY_NUNQ	0x8d
#define CSRKEY_NPWR	0x30
*/

#define	CSRVAL_1394TA	0x00a02d
#define	CSRVAL_ANSIT10	0x00609e
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
	u_int32_t spec_id:24,
		  spec_type:8;
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

#define CROM_MAX_DEPTH	10
struct crom_ptr {
	struct csrdirectory *dir;
	int index;
};

struct crom_context {
	int depth;
	struct crom_ptr stack[CROM_MAX_DEPTH];
};

void crom_init_context(struct crom_context *, u_int32_t *);
struct csrreg *crom_get(struct crom_context *);
void crom_next(struct crom_context *);
void crom_parse_text(struct crom_context *, char *, int);
u_int16_t crom_crc(u_int32_t *r, int);
struct csrreg *crom_search_key(struct crom_context *, u_int8_t);
#ifndef _KERNEL
char *crom_desc(struct crom_context *, char *, int);
#endif
