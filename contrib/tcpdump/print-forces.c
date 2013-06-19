/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code
 * distributions retain the above copyright notice and this paragraph
 * in its entirety, and (2) distributions including binary code include
 * the above copyright notice and this paragraph in its entirety in
 * the documentation or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND
 * WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT
 * LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * Copyright (c) 2009 Mojatatu Networks, Inc
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <stdlib.h>

#include "interface.h"
#include "extract.h"

#include "forces.h"

#define RESLEN	4

int
prestlv_print(register const u_char * pptr, register u_int len,
	      u_int16_t op_msk _U_, int indent)
{
	const struct forces_tlv *tlv = (struct forces_tlv *)pptr;
	register const u_char *tdp = (u_char *) TLV_DATA(tlv);
	struct res_val *r = (struct res_val *)tdp;
	u_int dlen;

	/*
	 * pdatacnt_print() has ensured that len (the TLV length)
	 * >= TLV_HDRL.
	 */
	dlen = len - TLV_HDRL;
	if (dlen != RESLEN) {
		printf("illegal RESULT-TLV: %d bytes!\n", dlen);
		return -1;
	}

	TCHECK(*r);
	if (r->result >= 0x18 && r->result <= 0xFE) {
		printf("illegal reserved result code: 0x%x!\n", r->result);
		return -1;
	}

	if (vflag >= 3) {
		char *ib = indent_pr(indent, 0);
		printf("%s  Result: %s (code 0x%x)\n", ib,
		       tok2str(ForCES_errs, NULL, r->result), r->result);
	}
	return 0;

trunc:
	fputs("[|forces]", stdout);
	return -1;
}

int
fdatatlv_print(register const u_char * pptr, register u_int len,
	       u_int16_t op_msk _U_, int indent)
{
	const struct forces_tlv *tlv = (struct forces_tlv *)pptr;
	u_int rlen;
	register const u_char *tdp = (u_char *) TLV_DATA(tlv);
	u_int16_t type;

	/*
	 * pdatacnt_print() or pkeyitlv_print() has ensured that len
	 * (the TLV length) >= TLV_HDRL.
	 */
	rlen = len - TLV_HDRL;
	TCHECK(*tlv);
	type = EXTRACT_16BITS(&tlv->type);
	if (type != F_TLV_FULD) {
		printf("Error: expecting FULLDATA!\n");
		return -1;
	}

	if (vflag >= 3) {
		char *ib = indent_pr(indent + 2, 1);
		printf("%s[", &ib[1]);
		hex_print_with_offset(ib, tdp, rlen, 0);
		printf("\n%s]\n", &ib[1]);
	}
	return 0;

trunc:
	fputs("[|forces]", stdout);
	return -1;
}

int
sdatailv_print(register const u_char * pptr, register u_int len,
	       u_int16_t op_msk _U_, int indent)
{
	u_int rlen;
	const struct forces_ilv *ilv = (struct forces_ilv *)pptr;
	int invilv;

	if (len < ILV_HDRL) {
		printf("Error: BAD SPARSEDATA-TLV!\n");
		return -1;
	}
	rlen = len;
	indent += 1;
	while (rlen != 0) {
		char *ib = indent_pr(indent, 1);
		register const u_char *tdp = (u_char *) ILV_DATA(ilv);
		TCHECK(*ilv);
		invilv = ilv_valid(ilv, rlen);
		if (invilv) {
			printf("%s[", &ib[1]);
			hex_print_with_offset(ib, tdp, rlen, 0);
			printf("\n%s]\n", &ib[1]);
			return -1;
		}
		if (vflag >= 3) {
			int ilvl = EXTRACT_32BITS(&ilv->length);
			printf("\n%s ILV: type %x length %d\n", &ib[1],
			       EXTRACT_32BITS(&ilv->type), ilvl);
			hex_print_with_offset("\t\t[", tdp, ilvl-ILV_HDRL, 0);
		}

		ilv = GO_NXT_ILV(ilv, rlen);
	}

	return 0;

trunc:
	fputs("[|forces]", stdout);
	return -1;
}

int
sdatatlv_print(register const u_char * pptr, register u_int len,
	       u_int16_t op_msk, int indent)
{
	const struct forces_tlv *tlv = (struct forces_tlv *)pptr;
	u_int rlen;
	register const u_char *tdp = (u_char *) TLV_DATA(tlv);
	u_int16_t type;

	/*
	 * pdatacnt_print() has ensured that len (the TLV length)
	 * >= TLV_HDRL.
	 */
	rlen = len - TLV_HDRL;
	TCHECK(*tlv);
	type = EXTRACT_16BITS(&tlv->type);
	if (type != F_TLV_SPAD) {
		printf("Error: expecting SPARSEDATA!\n");
		return -1;
	}

	return sdatailv_print(tdp, rlen, op_msk, indent);

trunc:
	fputs("[|forces]", stdout);
	return -1;
}

int
pkeyitlv_print(register const u_char * pptr, register u_int len,
	       u_int16_t op_msk, int indent)
{
	const struct forces_tlv *tlv = (struct forces_tlv *)pptr;
	register const u_char *tdp = (u_char *) TLV_DATA(tlv);
	register const u_char *dp = tdp + 4;
	const struct forces_tlv *kdtlv = (struct forces_tlv *)dp;
	u_int32_t id;
	char *ib = indent_pr(indent, 0);
	u_int16_t type, tll;
	int invtlv;

	TCHECK(*tdp);
	id = EXTRACT_32BITS(tdp);
	printf("%sKeyinfo: Key 0x%x\n", ib, id);
	TCHECK(*kdtlv);
	type = EXTRACT_16BITS(&kdtlv->type);
	invtlv = tlv_valid(kdtlv, len);

	if (invtlv) {
		printf("%s TLV type 0x%x len %d\n",
		       tok2str(ForCES_TLV_err, NULL, invtlv), type,
		       EXTRACT_16BITS(&kdtlv->length));
		return -1;
	}
	/*
	 * At this point, tlv_valid() has ensured that the TLV
	 * length is large enough but not too large (it doesn't
	 * go past the end of the containing TLV).
	 */
	tll = EXTRACT_16BITS(&kdtlv->length);
	dp = (u_char *) TLV_DATA(kdtlv);
	return fdatatlv_print(dp, tll, op_msk, indent);

trunc:
	fputs("[|forces]", stdout);
	return -1;
}

int
pdatacnt_print(register const u_char * pptr, register u_int len,
	       u_int16_t IDcnt, u_int16_t op_msk, int indent)
{
	u_int i;
	u_int32_t id;
	char *ib = indent_pr(indent, 0);

	for (i = 0; i < IDcnt; i++) {
		TCHECK2(*pptr, 4);
		if (len < 4)
			goto trunc;
		id = EXTRACT_32BITS(pptr);
		if (vflag >= 3)
			printf("%s  ID#%02u: %d\n", ib, i + 1, id);
		len -= 4;
		pptr += 4;
	}
	if (len) {
		const struct forces_tlv *pdtlv = (struct forces_tlv *)pptr;
		u_int16_t type;
		u_int16_t tll;
		int pad = 0;
		u_int aln;
		int invtlv;

		TCHECK(*pdtlv);
		type = EXTRACT_16BITS(&pdtlv->type);
		invtlv = tlv_valid(pdtlv, len);
		if (invtlv) {
			printf
			    ("%s Outstanding bytes %d for TLV type 0x%x TLV len %d\n",
			     tok2str(ForCES_TLV_err, NULL, invtlv), len, type,
			     EXTRACT_16BITS(&pdtlv->length));
			goto pd_err;
		}
		/*
		 * At this point, tlv_valid() has ensured that the TLV
		 * length is large enough but not too large (it doesn't
		 * go past the end of the containing TLV).
		 */
		tll = EXTRACT_16BITS(&pdtlv->length) - TLV_HDRL;
		aln = F_ALN_LEN(EXTRACT_16BITS(&pdtlv->length));
		if (aln > EXTRACT_16BITS(&pdtlv->length)) {
			if (aln > len) {
				printf
				    ("Invalid padded pathdata TLV type 0x%x len %d missing %d pad bytes\n",
				     type, EXTRACT_16BITS(&pdtlv->length), aln - len);
			} else {
				pad = aln - EXTRACT_16BITS(&pdtlv->length);
			}
		}
		if (pd_valid(type)) {
			const struct pdata_ops *ops = get_forces_pd(type);

			if (vflag >= 3 && ops->v != F_TLV_PDAT) {
				if (pad)
					printf
					    ("%s  %s (Length %d DataLen %d pad %d Bytes)\n",
					     ib, ops->s, EXTRACT_16BITS(&pdtlv->length),
					     tll, pad);
				else
					printf
					    ("%s  %s (Length %d DataLen %d Bytes)\n",
					     ib, ops->s, EXTRACT_16BITS(&pdtlv->length),
					     tll);
			}

			chk_op_type(type, op_msk, ops->op_msk);

			if (ops->print((const u_char *)pdtlv,
					tll + pad + TLV_HDRL, op_msk,
					indent + 2) == -1)
				return -1;
			len -= (TLV_HDRL + pad + tll);
		} else {
			printf("Invalid path data content type 0x%x len %d\n",
			       type, EXTRACT_16BITS(&pdtlv->length));
pd_err:
			if (EXTRACT_16BITS(&pdtlv->length)) {
				hex_print_with_offset("Bad Data val\n\t  [",
						      pptr, len, 0);
				printf("]\n");

				return -1;
			}
		}
	}
	return len;

trunc:
	fputs("[|forces]", stdout);
	return -1;
}

int
pdata_print(register const u_char * pptr, register u_int len,
	    u_int16_t op_msk, int indent)
{
	const struct pathdata_h *pdh = (struct pathdata_h *)pptr;
	char *ib = indent_pr(indent, 0);
	u_int minsize = 0;
	int more_pd = 0;
	u_int16_t idcnt = 0;

	TCHECK(*pdh);
	if (len < sizeof(struct pathdata_h))
		goto trunc;
	if (vflag >= 3) {
		printf("\n%sPathdata: Flags 0x%x ID count %d\n",
		       ib, EXTRACT_16BITS(&pdh->pflags), EXTRACT_16BITS(&pdh->pIDcnt));
	}

	if (EXTRACT_16BITS(&pdh->pflags) & F_SELKEY) {
		op_msk |= B_KEYIN;
	}
	pptr += sizeof(struct pathdata_h);
	len -= sizeof(struct pathdata_h);
	idcnt = EXTRACT_16BITS(&pdh->pIDcnt);
	minsize = idcnt * 4;
	if (len < minsize) {
		printf("\t\t\ttruncated IDs expected %uB got %uB\n", minsize,
		       len);
		hex_print_with_offset("\t\t\tID Data[", pptr, len, 0);
		printf("]\n");
		return -1;
	}
	more_pd = pdatacnt_print(pptr, len, idcnt, op_msk, indent);
	if (more_pd > 0) {
		int consumed = len - more_pd;
		pptr += consumed;
		len = more_pd; 
		/* XXX: Argh, recurse some more */
		return recpdoptlv_print(pptr, len, op_msk, indent+1);
	} else
		return 0;

trunc:
	fputs("[|forces]", stdout);
	return -1;
}

int
genoptlv_print(register const u_char * pptr, register u_int len,
	       u_int16_t op_msk, int indent)
{
	const struct forces_tlv *pdtlv = (struct forces_tlv *)pptr;
	u_int16_t type;
	int tll;
	int invtlv;
	char *ib = indent_pr(indent, 0);

	TCHECK(*pdtlv);
	type = EXTRACT_16BITS(&pdtlv->type);
	tll = EXTRACT_16BITS(&pdtlv->length) - TLV_HDRL;
	invtlv = tlv_valid(pdtlv, len);
	printf("genoptlvprint - %s TLV type 0x%x len %d\n",
	       tok2str(ForCES_TLV, NULL, type), type, EXTRACT_16BITS(&pdtlv->length));
	if (!invtlv) {
		/*
		 * At this point, tlv_valid() has ensured that the TLV
		 * length is large enough but not too large (it doesn't
		 * go past the end of the containing TLV).
		 */
		register const u_char *dp = (u_char *) TLV_DATA(pdtlv);
		if (!ttlv_valid(type)) {
			printf("%s TLV type 0x%x len %d\n",
			       tok2str(ForCES_TLV_err, NULL, invtlv), type,
			       EXTRACT_16BITS(&pdtlv->length));
			return -1;
		}
		if (vflag >= 3)
			printf("%s%s, length %d (data length %d Bytes)",
			       ib, tok2str(ForCES_TLV, NULL, type),
			       EXTRACT_16BITS(&pdtlv->length), tll);

		return pdata_print(dp, tll, op_msk, indent + 1);
	} else {
		printf("\t\t\tInvalid ForCES TLV type=%x", type);
		return -1;
	}

trunc:
	fputs("[|forces]", stdout);
	return -1;
}

int
recpdoptlv_print(register const u_char * pptr, register u_int len,
		 u_int16_t op_msk, int indent)
{
	const struct forces_tlv *pdtlv = (struct forces_tlv *)pptr;
	int tll;
	int invtlv;
	u_int16_t type;
	register const u_char *dp;
	char *ib;

	while (len != 0) {
		TCHECK(*pdtlv);
		invtlv = tlv_valid(pdtlv, len);
		if (invtlv) {
			break;
		}

		/*
		 * At this point, tlv_valid() has ensured that the TLV
		 * length is large enough but not too large (it doesn't
		 * go past the end of the containing TLV).
		 */
		ib = indent_pr(indent, 0);
		type = EXTRACT_16BITS(&pdtlv->type);
		dp = (u_char *) TLV_DATA(pdtlv);
		tll = EXTRACT_16BITS(&pdtlv->length) - TLV_HDRL;

		if (vflag >= 3)
			printf
			    ("%s%s, length %d (data encapsulated %d Bytes)",
			     ib, tok2str(ForCES_TLV, NULL, type),
			     EXTRACT_16BITS(&pdtlv->length),
			     EXTRACT_16BITS(&pdtlv->length) - TLV_HDRL);

		if (pdata_print(dp, tll, op_msk, indent + 1) == -1)
			return -1;
		pdtlv = GO_NXT_TLV(pdtlv, len);
	}

	if (len) {
		printf
		    ("\n\t\tMessy PATHDATA TLV header, type (0x%x)\n\t\texcess of %d Bytes ",
		     EXTRACT_16BITS(&pdtlv->type), len - EXTRACT_16BITS(&pdtlv->length));
		return -1;
	}

	return 0;

trunc:
	fputs("[|forces]", stdout);
	return -1;
}

int
invoptlv_print(register const u_char * pptr, register u_int len,
	       u_int16_t op_msk _U_, int indent)
{
	char *ib = indent_pr(indent, 1);

	if (vflag >= 3) {
		printf("%sData[", &ib[1]);
		hex_print_with_offset(ib, pptr, len, 0);
		printf("%s]\n", ib);
	}
	return -1;
}

int otlv_print(const struct forces_tlv *otlv, u_int16_t op_msk _U_, int indent)
{
	int rc = 0;
	register const u_char *dp = (u_char *) TLV_DATA(otlv);
	u_int16_t type;
	int tll;
	char *ib = indent_pr(indent, 0);
	const struct optlv_h *ops;

	/*
	 * lfbselect_print() has ensured that EXTRACT_16BITS(&otlv->length)
	 * >= TLV_HDRL.
	 */
	TCHECK(*otlv);
	type = EXTRACT_16BITS(&otlv->type);
	tll = EXTRACT_16BITS(&otlv->length) - TLV_HDRL;
	ops = get_forces_optlv_h(type);
	if (vflag >= 3) {
		printf("%sOper TLV %s(0x%x) length %d\n", ib, ops->s, type,
		       EXTRACT_16BITS(&otlv->length));
	}
	/* empty TLVs like COMMIT and TRCOMMIT are empty, we stop here .. */
	if (!ops->flags & ZERO_TTLV) {
		if (tll != 0)	/* instead of "if (tll)" - for readability .. */
			printf("%s: Illegal - MUST be empty\n", ops->s);
		return rc;
	}
	/* rest of ops must at least have 12B {pathinfo} */
	if (tll < OP_MIN_SIZ) {
		printf("\t\tOper TLV %s(0x%x) length %d\n", ops->s, type,
		       EXTRACT_16BITS(&otlv->length));
		printf("\t\tTruncated data size %d minimum required %d\n", tll,
		       OP_MIN_SIZ);
		return invoptlv_print(dp, tll, ops->op_msk, indent);

	}

	rc = ops->print(dp, tll, ops->op_msk, indent + 1);
	return rc;

trunc:
	fputs("[|forces]", stdout);
	return -1;
}

#define ASTDLN	4
#define ASTMCD	255
int
asttlv_print(register const u_char * pptr, register u_int len,
	     u_int16_t op_msk _U_, int indent)
{
	u_int32_t rescode;
	u_int dlen;
	char *ib = indent_pr(indent, 0);

	/*
	 * forces_type_print() has ensured that len (the TLV length)
	 * >= TLV_HDRL.
	 */
	dlen = len - TLV_HDRL;
	if (dlen != ASTDLN) {
		printf("illegal ASTresult-TLV: %d bytes!\n", dlen);
		return -1;
	}
	TCHECK2(*pptr, 4);
	rescode = EXTRACT_32BITS(pptr);
	if (rescode > ASTMCD) {
		printf("illegal ASTresult result code: %d!\n", rescode);
		return -1;
	}

	if (vflag >= 3) {
		printf("Teardown reason:\n%s", ib);
		switch (rescode) {
		case 0:
			printf("Normal Teardown");
			break;
		case 1:
			printf("Loss of Heartbeats");
			break;
		case 2:
			printf("Out of bandwidth");
			break;
		case 3:
			printf("Out of Memory");
			break;
		case 4:
			printf("Application Crash");
			break;
		default:
			printf("Unknown Teardown reason");
			break;
		}
		printf("(%x)\n%s", rescode, ib);
	}
	return 0;

trunc:
	fputs("[|forces]", stdout);
	return -1;
}

#define ASRDLN	4
#define ASRMCD	3
int
asrtlv_print(register const u_char * pptr, register u_int len,
	     u_int16_t op_msk _U_, int indent)
{
	u_int32_t rescode;
	u_int dlen;
	char *ib = indent_pr(indent, 0);

	/*
	 * forces_type_print() has ensured that len (the TLV length)
	 * >= TLV_HDRL.
	 */
	dlen = len - TLV_HDRL;
	if (dlen != ASRDLN) {	/* id, instance, oper tlv */
		printf("illegal ASRresult-TLV: %d bytes!\n", dlen);
		return -1;
	}
	TCHECK2(*pptr, 4);
	rescode = EXTRACT_32BITS(pptr);

	if (rescode > ASRMCD) {
		printf("illegal ASRresult result code: %d!\n", rescode);
		return -1;
	}

	if (vflag >= 3) {
		printf("\n%s", ib);
		switch (rescode) {
		case 0:
			printf("Success ");
			break;
		case 1:
			printf("FE ID invalid ");
			break;
		case 2:
			printf("permission denied ");
			break;
		default:
			printf("Unknown ");
			break;
		}
		printf("(%x)\n%s", rescode, ib);
	}
	return 0;

trunc:
	fputs("[|forces]", stdout);
	return -1;
}

/*
 * XXX - not used.
 */
int
gentltlv_print(register const u_char * pptr _U_, register u_int len,
	       u_int16_t op_msk _U_, int indent _U_)
{
	u_int dlen = len - TLV_HDRL;

	if (dlen < 4) {		/* at least 32 bits must exist */
		printf("truncated TLV: %d bytes missing! ", 4 - dlen);
		return -1;
	}
	return 0;
}

#define RD_MIN 8
int
print_metailv(register const u_char * pptr, register u_int len,
	      u_int16_t op_msk _U_, int indent)
{
	u_int dlen;
	u_int rlen;
	char *ib = indent_pr(indent, 0);
	/* XXX: check header length */
	const struct forces_ilv *ilv = (struct forces_ilv *)pptr;

	/*
	 * print_metatlv() has ensured that len (what remains in the
	 * ILV) >= ILV_HDRL.
	 */
	dlen = len - ILV_HDRL;
	rlen = dlen;
	TCHECK(*ilv);
	printf("\n%sMetaID 0x%x length %d\n", ib, EXTRACT_32BITS(&ilv->type),
	       EXTRACT_32BITS(&ilv->length));
	hex_print_with_offset("\n\t\t\t\t[", ILV_DATA(ilv), rlen, 0);
	return 0;

trunc:
	fputs("[|forces]", stdout);
	return -1;
}

int
print_metatlv(register const u_char * pptr, register u_int len,
	      u_int16_t op_msk _U_, int indent)
{
	u_int dlen;
	char *ib = indent_pr(indent, 0);
	u_int rlen;
	const struct forces_ilv *ilv = (struct forces_ilv *)pptr;
	int invilv;

	/*
	 * redirect_print() has ensured that len (what remains in the
	 * TLV) >= TLV_HDRL.
	 */
	dlen = len - TLV_HDRL;
	rlen = dlen;
	printf("\n%s METADATA\n", ib);
	while (rlen != 0) {
		TCHECK(*ilv);
		invilv = ilv_valid(ilv, rlen);
		if (invilv)
			break;

		/*
		 * At this point, ilv_valid() has ensured that the ILV
		 * length is large enough but not too large (it doesn't
		 * go past the end of the containing TLV).
		 */
		print_metailv((u_char *) ilv, rlen, 0, indent + 1);

		ilv = GO_NXT_ILV(ilv, rlen);
	}

	return 0;

trunc:
	fputs("[|forces]", stdout);
	return -1;
}

/*
*/
int
print_reddata(register const u_char * pptr, register u_int len,
	      u_int16_t op_msk _U_, int indent _U_)
{
	u_int dlen;
	u_int rlen;
	int invtlv;
	const struct forces_tlv *tlv = (struct forces_tlv *)pptr;

	/*
	 * redirect_print() has ensured that len (what remains in the
	 * TLV) >= TLV_HDRL.
	 */
	dlen = len - TLV_HDRL;
	printf("\n\t\t Redirect DATA\n");
	if (dlen <= RD_MIN) {
		printf("\n\t\ttruncated Redirect data: %d bytes missing! ",
		       RD_MIN - dlen);
		return -1;
	}

	rlen = dlen;
	TCHECK(*tlv);
	invtlv = tlv_valid(tlv, rlen);

	if (invtlv) {
		printf("Redir data type 0x%x len %d\n", EXTRACT_16BITS(&tlv->type),
		       EXTRACT_16BITS(&tlv->length));
		return -1;
	}

	/*
	 * At this point, tlv_valid() has ensured that the TLV
	 * length is large enough but not too large (it doesn't
	 * go past the end of the containing TLV).
	 */
	rlen -= TLV_HDRL;
	hex_print_with_offset("\n\t\t\t[", TLV_DATA(tlv), rlen, 0);
	return 0;

trunc:
	fputs("[|forces]", stdout);
	return -1;
}

int
redirect_print(register const u_char * pptr, register u_int len,
	       u_int16_t op_msk _U_, int indent)
{
	const struct forces_tlv *tlv = (struct forces_tlv *)pptr;
	u_int dlen;
	u_int rlen;
	int invtlv;

	/*
	 * forces_type_print() has ensured that len (the TLV length)
	 * >= TLV_HDRL.
	 */
	dlen = len - TLV_HDRL;
	if (dlen <= RD_MIN) {
		printf("\n\t\ttruncated Redirect TLV: %d bytes missing! ",
		       RD_MIN - dlen);
		return -1;
	}

	rlen = dlen;
	indent += 1;
	while (rlen != 0) {
		TCHECK(*tlv);
		invtlv = tlv_valid(tlv, rlen);
		if (invtlv)
			break;

		/*
		 * At this point, tlv_valid() has ensured that the TLV
		 * length is large enough but not too large (it doesn't
		 * go past the end of the containing TLV).
		 */
		if (EXTRACT_16BITS(&tlv->type) == F_TLV_METD) {
			print_metatlv((u_char *) TLV_DATA(tlv), rlen, 0, indent);
		} else if ((EXTRACT_16BITS(&tlv->type) == F_TLV_REDD)) {
			print_reddata((u_char *) TLV_DATA(tlv), rlen, 0, indent);
		} else {
			printf("Unknown REDIRECT TLV 0x%x len %d\n",
			       EXTRACT_16BITS(&tlv->type), EXTRACT_16BITS(&tlv->length));
		}

		tlv = GO_NXT_TLV(tlv, rlen);
	}

	if (rlen) {
		printf
		    ("\n\t\tMessy Redirect TLV header, type (0x%x)\n\t\texcess of %d Bytes ",
		     EXTRACT_16BITS(&tlv->type), rlen - EXTRACT_16BITS(&tlv->length));
		return -1;
	}

	return 0;

trunc:
	fputs("[|forces]", stdout);
	return -1;
}

#define OP_OFF 8
#define OP_MIN 12

int
lfbselect_print(register const u_char * pptr, register u_int len,
		u_int16_t op_msk, int indent)
{
	const struct forces_lfbsh *lfbs;
	const struct forces_tlv *otlv;
	char *ib = indent_pr(indent, 0);
	u_int dlen;
	u_int rlen;
	int invtlv;

	/*
	 * forces_type_print() has ensured that len (the TLV length)
	 * >= TLV_HDRL.
	 */
	dlen = len - TLV_HDRL;
	if (dlen <= OP_MIN) {	/* id, instance, oper tlv header .. */
		printf("\n\t\ttruncated lfb selector: %d bytes missing! ",
		       OP_MIN - dlen);
		return -1;
	}

	/*
	 * At this point, we know that dlen > OP_MIN; OP_OFF < OP_MIN, so
	 * we also know that it's > OP_OFF.
	 */
	rlen = dlen - OP_OFF;

	lfbs = (const struct forces_lfbsh *)pptr;
	TCHECK(*lfbs);
	if (vflag >= 3) {
		printf("\n%s%s(Classid %x) instance %x\n",
		       ib, tok2str(ForCES_LFBs, NULL, EXTRACT_32BITS(&lfbs->class)),
		       EXTRACT_32BITS(&lfbs->class),
		       EXTRACT_32BITS(&lfbs->instance));
	}

	otlv = (struct forces_tlv *)(lfbs + 1);

	indent += 1;
	while (rlen != 0) {
		TCHECK(*otlv);
		invtlv = tlv_valid(otlv, rlen);
		if (invtlv)
			break;

		/*
		 * At this point, tlv_valid() has ensured that the TLV
		 * length is large enough but not too large (it doesn't
		 * go past the end of the containing TLV).
		 */
		if (op_valid(EXTRACT_16BITS(&otlv->type), op_msk)) {
			otlv_print(otlv, 0, indent);
		} else {
			if (vflag < 3)
				printf("\n");
			printf
			    ("\t\tINValid oper-TLV type 0x%x length %d for this ForCES message\n",
			     EXTRACT_16BITS(&otlv->type), EXTRACT_16BITS(&otlv->length));
			invoptlv_print((u_char *)otlv, rlen, 0, indent);
		}
		otlv = GO_NXT_TLV(otlv, rlen);
	}

	if (rlen) {
		printf
		    ("\n\t\tMessy oper TLV header, type (0x%x)\n\t\texcess of %d Bytes ",
		     EXTRACT_16BITS(&otlv->type), rlen - EXTRACT_16BITS(&otlv->length));
		return -1;
	}

	return 0;

trunc:
	fputs("[|forces]", stdout);
	return -1;
}

int
forces_type_print(register const u_char * pptr, const struct forcesh *fhdr _U_,
		  register u_int mlen, const struct tom_h *tops)
{
	const struct forces_tlv *tltlv;
	u_int rlen;
	int invtlv;
	int rc = 0;
	int ttlv = 0;

	/*
	 * forces_print() has already checked that mlen >= ForCES_HDRL
	 * by calling ForCES_HLN_VALID().
	 */
	rlen = mlen - ForCES_HDRL;

	if (rlen > TLV_HLN) {
		if (tops->flags & ZERO_TTLV) {
			printf("<0x%x>Illegal Top level TLV!\n", tops->flags);
			return -1;
		}
	} else {
		if (tops->flags & ZERO_MORE_TTLV)
			return 0;
		if (tops->flags & ONE_MORE_TTLV) {
			printf("\tTop level TLV Data missing!\n");
			return -1;
		}
	}

	if (tops->flags & ZERO_TTLV) {
		return 0;
	}

	ttlv = tops->flags >> 4;
	tltlv = GET_TOP_TLV(pptr);

	/*XXX: 15 top level tlvs will probably be fine
	   You are nuts if you send more ;-> */
	while (rlen != 0) {
		TCHECK(*tltlv);
		invtlv = tlv_valid(tltlv, rlen);
		if (invtlv)
			break;

		/*
		 * At this point, tlv_valid() has ensured that the TLV
		 * length is large enough but not too large (it doesn't
		 * go past the end of the packet).
		 */
		if (!ttlv_valid(EXTRACT_16BITS(&tltlv->type))) {
			printf("\n\tInvalid ForCES Top TLV type=0x%x",
			       EXTRACT_16BITS(&tltlv->type));
			return -1;
		}

		if (vflag >= 3)
			printf("\t%s, length %d (data length %d Bytes)",
			       tok2str(ForCES_TLV, NULL, EXTRACT_16BITS(&tltlv->type)),
			       EXTRACT_16BITS(&tltlv->length),
			       EXTRACT_16BITS(&tltlv->length) - TLV_HDRL);

		rc = tops->print((u_char *) TLV_DATA(tltlv),
				 EXTRACT_16BITS(&tltlv->length), tops->op_msk, 9);
		if (rc < 0) {
			return -1;
		}
		tltlv = GO_NXT_TLV(tltlv, rlen);
		ttlv--;
		if (ttlv <= 0)
			break;
	}
	/*
	 * XXX - if ttlv != 0, does that mean that the packet was too
	 * short, and didn't have *enough* TLVs in it?
	 */
	if (rlen) {
		printf("\tMess TopTLV header: min %u, total %d advertised %d ",
		       TLV_HDRL, rlen, EXTRACT_16BITS(&tltlv->length));
		return -1;
	}

	return 0;

trunc:
	fputs("[|forces]", stdout);
	return -1;
}

void forces_print(register const u_char * pptr, register u_int len)
{
	const struct forcesh *fhdr;
	u_int mlen;
	u_int32_t flg_raw;
	const struct tom_h *tops;
	int rc = 0;

	fhdr = (const struct forcesh *)pptr;
	TCHECK(*fhdr);
	if (!tom_valid(fhdr->fm_tom)) {
		printf("Invalid ForCES message type %d\n", fhdr->fm_tom);
		goto error;
	}

	mlen = ForCES_BLN(fhdr);

	tops = get_forces_tom(fhdr->fm_tom);
	if (tops->v == TOM_RSVD) {
		printf("\n\tUnknown ForCES message type=0x%x", fhdr->fm_tom);
		goto error;
	}

	printf("\n\tForCES %s ", tops->s);
	if (!ForCES_HLN_VALID(mlen, len)) {
		printf
		    ("Illegal ForCES pkt len - min %u, total recvd %d, advertised %d ",
		     ForCES_HDRL, len, ForCES_BLN(fhdr));
		goto error;
	}

	TCHECK2(*(pptr + 20), 4);
	flg_raw = EXTRACT_32BITS(pptr + 20);
	if (vflag >= 1) {
		printf("\n\tForCES Version %d len %uB flags 0x%08x ",
		       ForCES_V(fhdr), mlen, flg_raw);
		printf("\n\tSrcID 0x%x(%s) DstID 0x%x(%s) Correlator 0x%" PRIx64,
		       ForCES_SID(fhdr), ForCES_node(ForCES_SID(fhdr)),
		       ForCES_DID(fhdr), ForCES_node(ForCES_DID(fhdr)),
		       EXTRACT_64BITS(fhdr->fm_cor));

	}
	if (vflag >= 2) {
		printf
		    ("\n\tForCES flags:\n\t  %s(0x%x), prio=%d, %s(0x%x),\n\t  %s(0x%x), %s(0x%x)\n",
		     ForCES_ACKp(ForCES_ACK(fhdr)), ForCES_ACK(fhdr),
		     ForCES_PRI(fhdr),
		     ForCES_EMp(ForCES_EM(fhdr)), ForCES_EM(fhdr),
		     ForCES_ATp(ForCES_AT(fhdr)), ForCES_AT(fhdr),
		     ForCES_TPp(ForCES_TP(fhdr)), ForCES_TP(fhdr));
		printf
		    ("\t  Extra flags: rsv(b5-7) 0x%x rsv(b13-31) 0x%x\n",
		     ForCES_RS1(fhdr), ForCES_RS2(fhdr));
	}
	rc = forces_type_print(pptr, fhdr, mlen, tops);
	if (rc < 0) {
error:
		hex_print_with_offset("\n\t[", pptr, len, 0);
		printf("\n\t]");
		return;
	}

	if (vflag >= 4) {
		printf("\n\t  Raw ForCES message\n\t [");
		hex_print_with_offset("\n\t ", pptr, len, 0);
		printf("\n\t ]");
	}
	printf("\n");
	return;

trunc:
	fputs("[|forces]", stdout);
}
