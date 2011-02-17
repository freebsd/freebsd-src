/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (c) 2009 Mojatatu Networks, Inc
 *
 */

/*
 * Per draft-ietf-forces-protocol-22
*/
#define	ForCES_VERS	1
#define	ForCES_HDRL	24
#define	ForCES_ALNL	4U
#define TLV_HDRL	4
#define ILV_HDRL	8

#define TOM_RSVD 	0x0
#define TOM_ASSNSETUP 	0x1
#define TOM_ASSNTEARD 	0x2
#define TOM_CONFIG 	0x3
#define TOM_QUERY 	0x4
#define TOM_EVENTNOT 	0x5
#define TOM_PKTREDIR 	0x6
#define TOM_HEARTBT 	0x0F
#define TOM_ASSNSETREP 	0x11
#define TOM_CONFIGREP 	0x13
#define TOM_QUERYREP 	0x14

/*
 * tom_h Flags: resv1(8b):maxtlvs(4b):resv2(2b):mintlv(2b)
*/
#define ZERO_TTLV	0x01
#define ZERO_MORE_TTLV	0x02
#define ONE_MORE_TTLV	0x04
#define ZERO_TLV	0x00
#define ONE_TLV		0x10
#define TWO_TLV		0x20
#define MAX_TLV		0xF0

#define TTLV_T1		(ONE_MORE_TTLV|ONE_TLV)
#define TTLV_T2		(ONE_MORE_TTLV|MAX_TLV)

struct tom_h {
	u_int32_t v;
	u_int16_t flags;
	u_int16_t op_msk;
	const char *s;
	int (*print) (register const u_char * pptr, register u_int len,
		      u_int16_t op_msk, int indent);
};

enum {
	TOM_RSV_I,
	TOM_ASS_I,
	TOM_AST_I,
	TOM_CFG_I,
	TOM_QRY_I,
	TOM_EVN_I,
	TOM_RED_I,
	TOM_HBT_I,
	TOM_ASR_I,
	TOM_CNR_I,
	TOM_QRR_I,
	_TOM_RSV_MAX
};
#define TOM_MAX_IND (_TOM_RSV_MAX - 1)

int lfbselect_print(register const u_char * pptr, register u_int len,
		    u_int16_t op_msk, int indent);
int redirect_print(register const u_char * pptr, register u_int len,
		   u_int16_t op_msk, int indent);
int asrtlv_print(register const u_char * pptr, register u_int len,
		 u_int16_t op_msk, int indent);
int asttlv_print(register const u_char * pptr, register u_int len,
		 u_int16_t op_msk, int indent);
int gentltlv_print(register const u_char * pptr, register u_int len,
		   u_int16_t op_msk, int indent);
int print_metailv(register const u_char * pptr, register u_int len,
	      u_int16_t op_msk, int indent);
int print_metatlv(register const u_char * pptr, register u_int len,
	      u_int16_t op_msk, int indent);
int print_reddata(register const u_char * pptr, register u_int len,
	      u_int16_t op_msk, int indent);

static inline int tom_valid(u_int8_t tom)
{
	if (tom > 0) {
		if (tom >= 0x7 && tom <= 0xe)
			return 0;
		if (tom == 0x10)
			return 0;
		if (tom > 0x14)
			return 0;
		return 1;
	} else
		return 0;
}

static inline const char *ForCES_node(u_int32_t node)
{
	if (node <= 0x3FFFFFFF)
		return "FE";
	if (node >= 0x40000000 && node <= 0x7FFFFFFF)
		return "CE";
	if (node >= 0xC0000000 && node <= 0xFFFFFFEF)
		return "AllMulticast";
	if (node == 0xFFFFFFFD)
		return "AllCEsBroadcast";
	if (node == 0xFFFFFFFE)
		return "AllFEsBroadcast";
	if (node == 0xFFFFFFFF)
		return "AllBroadcast";

	return "ForCESreserved";

}

static inline const char *ForCES_ACKp(u_int32_t flg)
{
	if (flg == 0x0)
		return "NoACK";
	if (flg == 0x1)
		return "SuccessACK";
	if (flg == 0x2)
		return "FailureACK";
	if (flg == 0x3)
		return "AlwaysACK";
	return "ACKUnknown";
}

static inline const char *ForCES_EMp(u_int32_t flg)
{
	if (flg == 0x0)
		return "EMReserved";
	if (flg == 0x1)
		return "execute-all-or-none";
	if (flg == 0x2)
		return "execute-until-failure";
	if (flg == 0x3)
		return "continue-execute-on-failure";
	return "EMUnknown";
}

static inline const char *ForCES_ATp(u_int32_t flg)
{
	if (flg == 0x0)
		return "Standalone";
	if (flg == 0x1)
		return "2PCtransaction";
	return "ATUnknown";
}

static inline const char *ForCES_TPp(u_int32_t flg)
{
	if (flg == 0x0)
		return "StartofTransaction";
	if (flg == 0x1)
		return "MiddleofTransaction";
	if (flg == 0x2)
		return "EndofTransaction";
	if (flg == 0x3)
		return "abort";
	return "TPUnknown";
}

/*
 * Structure of forces header, naked of TLVs.
 */
struct forcesh {
	u_int8_t fm_vrsvd;	/* version and reserved */
#define ForCES_V(forcesh)	((forcesh)->fm_vrsvd >> 4)
	u_int8_t fm_tom;	/* type of message */
	u_int16_t fm_len;	/* total length * 4 bytes */
#define ForCES_BLN(forcesh)	((u_int32_t)(EXTRACT_16BITS(&(forcesh)->fm_len) << 2))
	u_int32_t fm_sid;	/* Source ID */
#define ForCES_SID(forcesh)	EXTRACT_32BITS(&(forcesh)->fm_sid)
	u_int32_t fm_did;	/* Destination ID */
#define ForCES_DID(forcesh)	EXTRACT_32BITS(&(forcesh)->fm_did)
	u_int8_t fm_cor[8];	/* correlator */
	u_int32_t fm_flags;	/* flags */
#define ForCES_ACK(forcesh)	((EXTRACT_32BITS(&(forcesh)->fm_flags)&0xC0000000) >> 30)
#define ForCES_PRI(forcesh)	((EXTRACT_32BITS(&(forcesh)->fm_flags)&0x38000000) >> 27)
#define ForCES_RS1(forcesh)	((EXTRACT_32BITS(&(forcesh)->fm_flags)&0x07000000) >> 24)
#define ForCES_EM(forcesh)	((EXTRACT_32BITS(&(forcesh)->fm_flags)&0x00C00000) >> 22)
#define ForCES_AT(forcesh)	((EXTRACT_32BITS(&(forcesh)->fm_flags)&0x00200000) >> 21)
#define ForCES_TP(forcesh)	((EXTRACT_32BITS(&(forcesh)->fm_flags)&0x00180000) >> 19)
#define ForCES_RS2(forcesh)	((EXTRACT_32BITS(&(forcesh)->fm_flags)&0x0007FFFF) >> 0)
};

#define ForCES_HLN_VALID(fhl,tlen) ((tlen) >= ForCES_HDRL && \
				   (fhl) >= ForCES_HDRL && \
				   (fhl) == (tlen))

#define F_LFB_RSVD 0x0
#define F_LFB_FEO 0x1
#define F_LFB_FEPO 0x2
const struct tok ForCES_LFBs[] = {
	{F_LFB_RSVD, "Invalid TLV"},
	{F_LFB_FEO, "FEObj LFB"},
	{F_LFB_FEPO, "FEProtoObj LFB"},
	{0, NULL}
};

int forces_type_print(register const u_char * pptr, const struct forcesh *fhdr,
		  register u_int mlen, const struct tom_h *tops);

enum {
	F_OP_RSV,
	F_OP_SET,
	F_OP_SETPROP,
	F_OP_SETRESP,
	F_OP_SETPRESP,
	F_OP_DEL,
	F_OP_DELRESP,
	F_OP_GET,
	F_OP_GETPROP,
	F_OP_GETRESP,
	F_OP_GETPRESP,
	F_OP_REPORT,
	F_OP_COMMIT,
	F_OP_RCOMMIT,
	F_OP_RTRCOMP,
	_F_OP_MAX
};

#define F_OP_MAX	(_F_OP_MAX - 1)
enum {
	B_OP_SET = 1 << (F_OP_SET - 1),
	B_OP_SETPROP = 1 << (F_OP_SETPROP - 1),
	B_OP_SETRESP = 1 << (F_OP_SETRESP - 1),
	B_OP_SETPRESP = 1 << (F_OP_SETPRESP - 1),
	B_OP_DEL = 1 << (F_OP_DEL - 1),
	B_OP_DELRESP = 1 << (F_OP_DELRESP - 1),
	B_OP_GET = 1 << (F_OP_GET - 1),
	B_OP_GETPROP = 1 << (F_OP_GETPROP - 1),
	B_OP_GETRESP = 1 << (F_OP_GETRESP - 1),
	B_OP_GETPRESP = 1 << (F_OP_GETPRESP - 1),
	B_OP_REPORT = 1 << (F_OP_REPORT - 1),
	B_OP_COMMIT = 1 << (F_OP_COMMIT - 1),
	B_OP_RCOMMIT = 1 << (F_OP_RCOMMIT - 1),
	B_OP_RTRCOMP = 1 << (F_OP_RTRCOMP - 1),
};

struct optlv_h {
	u_int16_t flags;
	u_int16_t op_msk;
	const char *s;
	int (*print) (register const u_char * pptr, register u_int len,
		      u_int16_t op_msk, int indent);
};

int genoptlv_print(register const u_char * pptr, register u_int len,
		   u_int16_t op_msk, int indent);
int recpdoptlv_print(register const u_char * pptr, register u_int len,
		     u_int16_t op_msk, int indent);
int invoptlv_print(register const u_char * pptr, register u_int len,
		   u_int16_t op_msk, int indent);

#define OP_MIN_SIZ 8
struct pathdata_h {
	u_int16_t pflags;
	u_int16_t pIDcnt;
};

#define	B_FULLD		0x1
#define	B_SPARD 	0x2
#define B_RESTV		0x4
#define B_KEYIN		0x8

static const struct optlv_h OPTLV_msg[F_OP_MAX + 1] = {
	/* F_OP_RSV */ {ZERO_TTLV, 0, "Invalid OPTLV", invoptlv_print},
	/* F_OP_SET */ {TTLV_T2, B_FULLD | B_SPARD, " Set", recpdoptlv_print},
	/* F_OP_SETPROP */
	    {TTLV_T2, B_FULLD | B_SPARD, " SetProp", recpdoptlv_print},
	/* F_OP_SETRESP */ {TTLV_T2, B_RESTV, " SetResp", recpdoptlv_print},
	/* F_OP_SETPRESP */ {TTLV_T2, B_RESTV, " SetPropResp", recpdoptlv_print},
	/* F_OP_DEL */ {ZERO_TTLV, 0, " Del", recpdoptlv_print},
	/* F_OP_DELRESP */ {TTLV_T2, B_RESTV, " DelResp", recpdoptlv_print},
	/* F_OP_GET */ {ZERO_TTLV, 0, " Get", recpdoptlv_print},
	/* F_OP_GETPROP */ {ZERO_TTLV, 0, " GetProp", recpdoptlv_print},
	/* F_OP_GETRESP */
	    {TTLV_T2, B_FULLD | B_RESTV, " GetResp", recpdoptlv_print},
	/* F_OP_GETPRESP */
	    {TTLV_T2, B_FULLD | B_RESTV, " GetPropResp", recpdoptlv_print},
	/* F_OP_REPORT */
	    {TTLV_T2, B_FULLD | B_SPARD, " Report", recpdoptlv_print},
	/* F_OP_COMMIT */ {ZERO_TTLV, 0, " Commit", NULL},
	/* F_OP_RCOMMIT */ {TTLV_T1, B_RESTV, " RCommit", genoptlv_print},
	/* F_OP_RTRCOMP */ {ZERO_TTLV, 0, " RTRCOMP", NULL},
};

static inline const struct optlv_h *get_forces_optlv_h(u_int16_t opt)
{
	if (opt > F_OP_MAX || opt <= F_OP_RSV)
		return &OPTLV_msg[F_OP_RSV];

	return &OPTLV_msg[opt];
}

#define IND_SIZE 256
#define IND_CHR ' '
#define IND_PREF '\n'
#define IND_SUF 0x0
char ind_buf[IND_SIZE];

static inline char *indent_pr(int indent, int nlpref)
{
	int i = 0;
	char *r = ind_buf;

	if (indent > (IND_SIZE - 1))
		indent = IND_SIZE - 1;

	if (nlpref) {
		r[i] = IND_PREF;
		i++;
		indent--;
	}

	while (--indent >= 0)
		r[i++] = IND_CHR;

	r[i] = IND_SUF;
	return r;
}

static inline int op_valid(u_int16_t op, u_int16_t mask)
{
	int opb = 1 << (op - 1);

	if (op == 0)
		return 0;
	if (opb & mask)
		return 1;
	/* I guess we should allow vendor operations? */
	if (op >= 0x8000)
		return 1;
	return 0;
}

#define F_TLV_RSVD	0x0000
#define F_TLV_REDR	0x0001
#define F_TLV_ASRS	0x0010
#define F_TLV_ASRT	0x0011
#define F_TLV_LFBS	0x1000
#define F_TLV_PDAT	0x0110
#define F_TLV_KEYI	0x0111
#define F_TLV_FULD	0x0112
#define F_TLV_SPAD	0x0113
#define F_TLV_REST	0x0114
#define F_TLV_METD	0x0115
#define F_TLV_REDD	0x0116
#define F_TLV_VNST	0x8000

static const struct tok ForCES_TLV[] = {
	{F_TLV_RSVD, "Invalid TLV"},
	{F_TLV_REDR, "REDIRECT TLV"},
	{F_TLV_ASRS, "ASResult TLV"},
	{F_TLV_ASRT, "ASTreason TLV"},
	{F_TLV_LFBS, "LFBselect TLV"},
	{F_TLV_PDAT, "PATH-DATA TLV"},
	{F_TLV_KEYI, "KEYINFO TLV"},
	{F_TLV_FULD, "FULLDATA TLV"},
	{F_TLV_SPAD, "SPARSEDATA TLV"},
	{F_TLV_REST, "RESULT TLV"},
	{F_TLV_METD, "METADATA TLV"},
	{F_TLV_REDD, "REDIRECTDATA TLV"},
	{0, NULL}
};

#define TLV_HLN	4
static inline int ttlv_valid(u_int16_t ttlv)
{
	if (ttlv > 0) {
		if (ttlv == 1 || ttlv == 0x1000)
			return 1;
		if (ttlv >= 0x10 && ttlv <= 0x11)
			return 1;
		if (ttlv >= 0x110 && ttlv <= 0x116)
			return 1;
		if (ttlv >= 0x8000)
			return 0;	/* XXX: */
	}

	return 0;
}

struct forces_ilv {
	u_int32_t type;
	u_int32_t length;
};

struct forces_tlv {
	u_int16_t type;
	u_int16_t length;
};

int otlv_print(const struct forces_tlv *otlv, u_int16_t op_msk, int indent);

#define F_ALN_LEN(len) ( ((len)+ForCES_ALNL-1) & ~(ForCES_ALNL-1) )
#define	GET_TOP_TLV(fhdr) ((struct forces_tlv *)((fhdr) + sizeof (struct forcesh)))
#define TLV_SET_LEN(len)  (F_ALN_LEN(TLV_HDRL) + (len))
#define TLV_ALN_LEN(len)  F_ALN_LEN(TLV_SET_LEN(len))
#define TLV_RDAT_LEN(tlv) ((int)(EXTRACT_16BITS(&(tlv)->length) - TLV_SET_LEN(0))
#define TLV_DATA(tlvp)   ((void*)(((char*)(tlvp)) + TLV_SET_LEN(0)))
#define GO_NXT_TLV(tlv,rlen) ((rlen) -= F_ALN_LEN(EXTRACT_16BITS(&(tlv)->length)), \
		              (struct forces_tlv*)(((char*)(tlv)) \
				      + F_ALN_LEN(EXTRACT_16BITS(&(tlv)->length))))
#define ILV_SET_LEN(len)  (F_ALN_LEN(ILV_HDRL) + (len))
#define ILV_ALN_LEN(len)  F_ALN_LEN(ILV_SET_LEN(len))
#define ILV_RDAT_LEN(ilv) ((int)(EXTRACT_32BITS(&(ilv)->length)) - ILV_SET_LEN(0))
#define ILV_DATA(ilvp)   ((void*)(((char*)(ilvp)) + ILV_SET_LEN(0)))
#define GO_NXT_ILV(ilv,rlen) ((rlen) -= F_ALN_LEN(EXTRACT_32BITS(&(ilv)->length)), \
		              (struct forces_ilv *)(((char*)(ilv)) \
				      + F_ALN_LEN(EXTRACT_32BITS(&(ilv)->length))))
#define INVALID_RLEN -1
#define INVALID_STLN -2
#define INVALID_LTLN -3
#define INVALID_ALEN -4

static const struct tok ForCES_TLV_err[] = {
	{INVALID_RLEN, "Invalid total length"},
	{INVALID_STLN, "xLV too short"},
	{INVALID_LTLN, "xLV too long"},
	{INVALID_ALEN, "data padding missing"},
	{0, NULL}
};

static inline int tlv_valid(const struct forces_tlv *tlv, u_int rlen)
{
	if (rlen < TLV_HDRL)
		return INVALID_RLEN;
	if (EXTRACT_16BITS(&tlv->length) < TLV_HDRL)
		return INVALID_STLN;
	if (EXTRACT_16BITS(&tlv->length) > rlen)
		return INVALID_LTLN;
	if (rlen < F_ALN_LEN(EXTRACT_16BITS(&tlv->length)))
		return INVALID_ALEN;

	return 0;
}

static inline int ilv_valid(const struct forces_ilv *ilv, u_int rlen)
{
	if (rlen < ILV_HDRL)
		return INVALID_RLEN;
	if (EXTRACT_32BITS(&ilv->length) < ILV_HDRL)
		return INVALID_STLN;
	if (EXTRACT_32BITS(&ilv->length) > rlen)
		return INVALID_LTLN;
	if (rlen < F_ALN_LEN(EXTRACT_32BITS(&ilv->length)))
		return INVALID_ALEN;

	return 0;
}

struct forces_lfbsh {
	u_int32_t class;
	u_int32_t instance;
};

#define ASSNS_OPS (B_OP_REPORT)
#define CFG_OPS	(B_OP_SET|B_OP_SETPROP|B_OP_DEL|B_OP_COMMIT|B_OP_RTRCOMP)
#define CFG_ROPS (B_OP_SETRESP|B_OP_SETPRESP|B_OP_DELRESP|B_OP_RCOMMIT)
#define CFG_QY (B_OP_GET|B_OP_GETPROP)
#define CFG_QYR (B_OP_GETRESP|B_OP_GETPRESP)
#define CFG_EVN (B_OP_REPORT)

static const struct tom_h ForCES_msg[TOM_MAX_IND + 1] = {
	/* TOM_RSV_I */ {TOM_RSVD, ZERO_TTLV, 0, "Invalid message", NULL},
	/* TOM_ASS_I */ {TOM_ASSNSETUP, ZERO_MORE_TTLV | TWO_TLV, ASSNS_OPS,
		       "Association Setup", lfbselect_print},
	/* TOM_AST_I */
	    {TOM_ASSNTEARD, TTLV_T1, 0, "Association TearDown", asttlv_print},
	/* TOM_CFG_I */ {TOM_CONFIG, TTLV_T2, CFG_OPS, "Config", lfbselect_print},
	/* TOM_QRY_I */ {TOM_QUERY, TTLV_T2, CFG_QY, "Query", lfbselect_print},
	/* TOM_EVN_I */ {TOM_EVENTNOT, TTLV_T1, CFG_EVN, "Event Notification",
		       lfbselect_print},
	/* TOM_RED_I */
	    {TOM_PKTREDIR, TTLV_T2, 0, "Packet Redirect", redirect_print},
	/* TOM_HBT_I */ {TOM_HEARTBT, ZERO_TTLV, 0, "HeartBeat", NULL},
	/* TOM_ASR_I */
	    {TOM_ASSNSETREP, TTLV_T1, 0, "Association Response", asrtlv_print},
	/* TOM_CNR_I */ {TOM_CONFIGREP, TTLV_T2, CFG_ROPS, "Config Response",
		       lfbselect_print},
	/* TOM_QRR_I */
	    {TOM_QUERYREP, TTLV_T2, CFG_QYR, "Query Response", lfbselect_print},
};

static inline const struct tom_h *get_forces_tom(u_int8_t tom)
{
	int i;
	for (i = TOM_RSV_I; i <= TOM_MAX_IND; i++) {
		const struct tom_h *th = &ForCES_msg[i];
		if (th->v == tom)
			return th;
	}
	return &ForCES_msg[TOM_RSV_I];
}

struct pdata_ops {
	u_int32_t v;
	u_int16_t flags;
	u_int16_t op_msk;
	const char *s;
	int (*print) (register const u_char * pptr, register u_int len,
		      u_int16_t op_msk, int indent);
};

enum {
	PD_RSV_I,
	PD_SEL_I,
	PD_FDT_I,
	PD_SDT_I,
	PD_RES_I,
	PD_PDT_I,
	_PD_RSV_MAX
};
#define PD_MAX_IND (_TOM_RSV_MAX - 1)

static inline int pd_valid(u_int16_t pd)
{
	if (pd >= F_TLV_PDAT && pd <= F_TLV_REST)
		return 1;
	return 0;
}

static inline void chk_op_type(u_int16_t type, u_int16_t msk, u_int16_t omsk)
{
	if (type != F_TLV_PDAT) {
		if (msk & B_KEYIN) {
			if (type != F_TLV_KEYI) {
				printf
				    ("Based on flags expected KEYINFO TLV!\n");
			}
		} else {
			if (!(msk & omsk)) {
				printf
				    ("Illegal DATA encoding for type 0x%x programmed %x got %x \n",
				     type, omsk, msk);
			}
		}
	}

}

int fdatatlv_print(register const u_char * pptr, register u_int len,
		   u_int16_t op_msk, int indent);
int sdatailv_print(register const u_char * pptr, register u_int len,
	       u_int16_t op_msk, int indent);
int sdatatlv_print(register const u_char * pptr, register u_int len,
		   u_int16_t op_msk, int indent);
int pdatatlv_print(register const u_char * pptr, register u_int len,
		   u_int16_t op_msk, int indent);
int pkeyitlv_print(register const u_char * pptr, register u_int len,
		   u_int16_t op_msk, int indent);

int pdatacnt_print(register const u_char * pptr, register u_int len,
	       u_int32_t IDcnt, u_int16_t op_msk, int indent);
int pdata_print(register const u_char * pptr, register u_int len,
	    u_int16_t op_msk, int indent);

int prestlv_print(register const u_char * pptr, register u_int len,
		  u_int16_t op_msk, int indent);
#define F_SELKEY 1

struct res_val {
	u_int8_t result;
	u_int8_t resv1;
	u_int16_t resv2;
};

static const struct pdata_ops ForCES_pdata[PD_MAX_IND + 1] = {
	/* PD_RSV_I */ {0, 0, 0, "Invalid message", NULL},
	/* PD_SEL_I */ {F_TLV_KEYI, 0, 0, "KEYINFO TLV", pkeyitlv_print},
	/* PD_FDT_I */ {F_TLV_FULD, 0, B_FULLD, "FULLDATA TLV", fdatatlv_print},
	/* PD_SDT_I */ {F_TLV_SPAD, 0, B_SPARD, "SPARSEDATA TLV", sdatatlv_print},
	/* PD_RES_I */ {F_TLV_REST, 0, B_RESTV, "RESULT TLV", prestlv_print},
	/* PD_PDT_I */
	    {F_TLV_PDAT, 0, 0, "Inner PATH-DATA TLV", recpdoptlv_print},
};

static inline const struct pdata_ops *get_forces_pd(u_int16_t pd)
{
	int i;
	for (i = PD_RSV_I + 1; i <= PD_MAX_IND; i++) {
		const struct pdata_ops *pdo = &ForCES_pdata[i];
		if (pdo->v == pd)
			return pdo;
	}
	return &ForCES_pdata[TOM_RSV_I];
}

enum {
	E_SUCCESS,
	E_INVALID_HEADER,
	E_LENGTH_MISMATCH,
	E_VERSION_MISMATCH,
	E_INVALID_DESTINATION_PID,
	E_LFB_UNKNOWN,
	E_LFB_NOT_FOUND,
	E_LFB_INSTANCE_ID_NOT_FOUND,
	E_INVALID_PATH,
	E_COMPONENT_DOES_NOT_EXIST,
	E_EXISTS,
	E_NOT_FOUND,
	E_READ_ONLY,
	E_INVALID_ARRAY_CREATION,
	E_VALUE_OUT_OF_RANGE,
	E_CONTENTS_TOO_LONG,
	E_INVALID_PARAMETERS,
	E_INVALID_MESSAGE_TYPE,
	E_INVALID_FLAGS,
	E_INVALID_TLV,
	E_EVENT_ERROR,
	E_NOT_SUPPORTED,
	E_MEMORY_ERROR,
	E_INTERNAL_ERROR,
	/* 0x18-0xFE are reserved .. */
	E_UNSPECIFIED_ERROR = 0XFF
};

const struct tok ForCES_errs[] = {
	{E_SUCCESS, "SUCCESS"},
	{E_INVALID_HEADER, "INVALID HEADER"},
	{E_LENGTH_MISMATCH, "LENGTH MISMATCH"},
	{E_VERSION_MISMATCH, "VERSION MISMATCH"},
	{E_INVALID_DESTINATION_PID, "INVALID DESTINATION PID"},
	{E_LFB_UNKNOWN, "LFB UNKNOWN"},
	{E_LFB_NOT_FOUND, "LFB NOT FOUND"},
	{E_LFB_INSTANCE_ID_NOT_FOUND, "LFB INSTANCE ID NOT FOUND"},
	{E_INVALID_PATH, "INVALID PATH"},
	{E_COMPONENT_DOES_NOT_EXIST, "COMPONENT DOES NOT EXIST"},
	{E_EXISTS, "EXISTS ALREADY"},
	{E_NOT_FOUND, "NOT FOUND"},
	{E_READ_ONLY, "READ ONLY"},
	{E_INVALID_ARRAY_CREATION, "INVALID ARRAY CREATION"},
	{E_VALUE_OUT_OF_RANGE, "VALUE OUT OF RANGE"},
	{E_CONTENTS_TOO_LONG, "CONTENTS TOO LONG"},
	{E_INVALID_PARAMETERS, "INVALID PARAMETERS"},
	{E_INVALID_MESSAGE_TYPE, "INVALID MESSAGE TYPE"},
	{E_INVALID_FLAGS, "INVALID FLAGS"},
	{E_INVALID_TLV, "INVALID TLV"},
	{E_EVENT_ERROR, "EVENT ERROR"},
	{E_NOT_SUPPORTED, "NOT SUPPORTED"},
	{E_MEMORY_ERROR, "MEMORY ERROR"},
	{E_INTERNAL_ERROR, "INTERNAL ERROR"},
	{E_UNSPECIFIED_ERROR, "UNSPECIFIED ERROR"},
	{0, NULL}
};
