/*
 * Copyright (c) 1990, 1991, 1993, 1994, 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by John Robert LoVerso.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * This implementation has been influenced by the CMU SNMP release,
 * by Steve Waldbusser.  However, this shares no code with that system.
 * Additional ASN.1 insight gained from Marshall T. Rose's _The_Open_Book_.
 * Earlier forms of this implementation were derived and/or inspired by an
 * awk script originally written by C. Philip Wood of LANL (but later
 * heavily modified by John Robert LoVerso).  The copyright notice for
 * that work is preserved below, even though it may not rightly apply
 * to this file.
 *
 * This started out as a very simple program, but the incremental decoding
 * (into the BE structure) complicated things.
 *
 #			Los Alamos National Laboratory
 #
 #	Copyright, 1990.  The Regents of the University of California.
 #	This software was produced under a U.S. Government contract
 #	(W-7405-ENG-36) by Los Alamos National Laboratory, which is
 #	operated by the	University of California for the U.S. Department
 #	of Energy.  The U.S. Government is licensed to use, reproduce,
 #	and distribute this software.  Permission is granted to the
 #	public to copy and use this software without charge, provided
 #	that this Notice and any statement of authorship are reproduced
 #	on all copies.  Neither the Government nor the University makes
 #	any warranty, express or implied, or assumes any liability or
 #	responsibility for the use of this software.
 #	@(#)snmp.awk.x	1.1 (LANL) 1/15/90
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: print-snmp.c,v 1.31 96/12/10 23:22:55 leres Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/time.h>

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"

/*
 * Universal ASN.1 types
 * (we only care about the tag values for those allowed in the Internet SMI)
 */
char *Universal[] = {
	"U-0",
	"Boolean",
	"Integer",
#define INTEGER 2
	"Bitstring",
	"String",
#define STRING 4
	"Null",
#define ASN_NULL 5
	"ObjID",
#define OBJECTID 6
	"ObjectDes",
	"U-8","U-9","U-10","U-11",	/* 8-11 */
	"U-12","U-13","U-14","U-15",	/* 12-15 */
	"Sequence",
#define SEQUENCE 16
	"Set"
};

/*
 * Application-wide ASN.1 types from the Internet SMI and their tags
 */
char *Application[] = {
	"IpAddress",
#define IPADDR 0
	"Counter",
#define COUNTER 1
	"Gauge",
#define GAUGE 2
	"TimeTicks",
#define TIMETICKS 3
	"Opaque"
};

/*
 * Context-specific ASN.1 types for the SNMP PDUs and their tags
 */
char *Context[] = {
	"GetRequest",
#define GETREQ 0
	"GetNextRequest",
#define GETNEXTREQ 1
	"GetResponse",
#define GETRESP 2
	"SetRequest",
#define SETREQ 3
	"Trap"
#define TRAP 4
};

/*
 * Private ASN.1 types
 * The Internet SMI does not specify any
 */
char *Private[] = {
	"P-0"
};

/*
 * error-status values for any SNMP PDU
 */
char *ErrorStatus[] = {
	"noError",
	"tooBig",
	"noSuchName",
	"badValue",
	"readOnly",
	"genErr"
};
#define DECODE_ErrorStatus(e) \
	( e >= 0 && e <= sizeof(ErrorStatus)/sizeof(ErrorStatus[0]) \
	? ErrorStatus[e] : (sprintf(errbuf, "err=%u", e), errbuf))

/*
 * generic-trap values in the SNMP Trap-PDU
 */
char *GenericTrap[] = {
	"coldStart",
	"warmStart",
	"linkDown",
	"linkUp",
	"authenticationFailure",
	"egpNeighborLoss",
	"enterpriseSpecific"
#define GT_ENTERPRISE 7
};
#define DECODE_GenericTrap(t) \
	( t >= 0 && t <= sizeof(GenericTrap)/sizeof(GenericTrap[0]) \
	? GenericTrap[t] : (sprintf(buf, "gt=%d", t), buf))

/*
 * ASN.1 type class table
 * Ties together the preceding Universal, Application, Context, and Private
 * type definitions.
 */
#define defineCLASS(x) { "x", x, sizeof(x)/sizeof(x[0]) } /* not ANSI-C */
struct {
	char	*name;
	char	**Id;
	    int	numIDs;
    } Class[] = {
	defineCLASS(Universal),
#define	UNIVERSAL	0
	defineCLASS(Application),
#define	APPLICATION	1
	defineCLASS(Context),
#define	CONTEXT		2
	defineCLASS(Private),
#define	PRIVATE		3
};

/*
 * defined forms for ASN.1 types
 */
char *Form[] = {
	"Primitive",
#define PRIMITIVE	0
	"Constructed",
#define CONSTRUCTED	1
};

/*
 * A structure for the OID tree for the compiled-in MIB.
 * This is stored as a general-order tree.
 */
struct obj {
	char	*desc;			/* name of object */
	u_char	oid;			/* sub-id following parent */
	u_char	type;			/* object type (unused) */
	struct obj *child, *next;	/* child and next sibling pointers */
} *objp = NULL;

/*
 * Include the compiled in SNMP MIB.  "mib.h" is produced by feeding
 * RFC-1156 format files into "makemib".  "mib.h" MUST define at least
 * a value for `mibroot'.
 *
 * In particular, this is gross, as this is including initialized structures,
 * and by right shouldn't be an "include" file.
 */
#include "mib.h"

/*
 * This defines a list of OIDs which will be abbreviated on output.
 * Currently, this includes the prefixes for the Internet MIB, the
 * private enterprises tree, and the experimental tree.
 */
struct obj_abrev {
	char *prefix;			/* prefix for this abrev */
	struct obj *node;		/* pointer into object table */
	char *oid;			/* ASN.1 encoded OID */
} obj_abrev_list[] = {
#ifndef NO_ABREV_MIB
	/* .iso.org.dod.internet.mgmt.mib */
	{ "",	&_mib_obj,		"\53\6\1\2\1" },
#endif
#ifndef NO_ABREV_ENTER
	/* .iso.org.dod.internet.private.enterprises */
	{ "E:",	&_enterprises_obj,	"\53\6\1\4\1" },
#endif
#ifndef NO_ABREV_EXPERI
	/* .iso.org.dod.internet.experimental */
	{ "X:",	&_experimental_obj,	"\53\6\1\3" },
#endif
	{ 0,0,0 }
};

/*
 * This is used in the OID print routine to walk down the object tree
 * rooted at `mibroot'.
 */
#define OBJ_PRINT(o, suppressdot) \
{ \
	if (objp) { \
		do { \
			if ((o) == objp->oid) \
				break; \
		} while ((objp = objp->next) != NULL); \
	} \
	if (objp) { \
		printf(suppressdot?"%s":".%s", objp->desc); \
		objp = objp->child; \
	} else \
		printf(suppressdot?"%u":".%u", (o)); \
}

/*
 * This is the definition for the Any-Data-Type storage used purely for
 * temporary internal representation while decoding an ASN.1 data stream.
 */
struct be {
	u_int32_t asnlen;
	union {
		caddr_t raw;
		int32_t integer;
		u_int32_t uns;
		const u_char *str;
	} data;
	u_short id;
	u_char form, class;		/* tag info */
	u_char type;
#define BE_ANY		255
#define BE_NONE		0
#define BE_NULL		1
#define BE_OCTET	2
#define BE_OID		3
#define BE_INT		4
#define BE_UNS		5
#define BE_STR		6
#define BE_SEQ		7
#define BE_INETADDR	8
#define BE_PDU		9
};

/*
 * Defaults for SNMP PDU components
 */
#define DEF_COMMUNITY "public"
#define DEF_VERSION 0

/*
 * constants for ASN.1 decoding
 */
#define OIDMUX 40
#define ASNLEN_INETADDR 4
#define ASN_SHIFT7 7
#define ASN_SHIFT8 8
#define ASN_BIT8 0x80
#define ASN_LONGLEN 0x80

#define ASN_ID_BITS 0x1f
#define ASN_FORM_BITS 0x20
#define ASN_FORM_SHIFT 5
#define ASN_CLASS_BITS 0xc0
#define ASN_CLASS_SHIFT 6

#define ASN_ID_EXT 0x1f		/* extension ID in tag field */

/*
 * truncated==1 means the packet was complete, but we don't have all of
 * it to decode.
 */
static int truncated;
#define ifNotTruncated if (truncated) fputs("[|snmp]", stdout); else

/*
 * This decodes the next ASN.1 object in the stream pointed to by "p"
 * (and of real-length "len") and stores the intermediate data in the
 * provided BE object.
 *
 * This returns -l if it fails (i.e., the ASN.1 stream is not valid).
 * O/w, this returns the number of bytes parsed from "p".
 */
static int
asn1_parse(register const u_char *p, u_int len, struct be *elem)
{
	u_char form, class, id;
	int i, hdr;

	elem->asnlen = 0;
	elem->type = BE_ANY;
	if (len < 1) {
		ifNotTruncated puts("[nothing to parse], stdout");
		return -1;
	}

	/*
	 * it would be nice to use a bit field, but you can't depend on them.
	 *  +---+---+---+---+---+---+---+---+
	 *  + class |frm|        id         |
	 *  +---+---+---+---+---+---+---+---+
	 *    7   6   5   4   3   2   1   0
	 */
	id = *p & ASN_ID_BITS;		/* lower 5 bits, range 00-1f */
#ifdef notdef
	form = (*p & 0xe0) >> 5;	/* move upper 3 bits to lower 3 */
	class = form >> 1;		/* bits 7&6 -> bits 1&0, range 0-3 */
	form &= 0x1;			/* bit 5 -> bit 0, range 0-1 */
#else
	form = (u_char)(*p & ASN_FORM_BITS) >> ASN_FORM_SHIFT;
	class = (u_char)(*p & ASN_CLASS_BITS) >> ASN_CLASS_SHIFT;
#endif
	elem->form = form;
	elem->class = class;
	elem->id = id;
	if (vflag)
		printf("|%.2x", *p);
	p++; len--; hdr = 1;
	/* extended tag field */
	if (id == ASN_ID_EXT) {
		for (id = 0; *p & ASN_BIT8 && len > 0; len--, hdr++, p++) {
			if (vflag)
				printf("|%.2x", *p);
			id = (id << 7) | (*p & ~ASN_BIT8);
		}
		if (len == 0 && *p & ASN_BIT8) {
			ifNotTruncated fputs("[Xtagfield?]", stdout);
			return -1;
		}
		elem->id = id = (id << 7) | *p;
		--len;
		++hdr;
		++p;
	}
	if (len < 1) {
		ifNotTruncated fputs("[no asnlen]", stdout);
		return -1;
	}
	elem->asnlen = *p;
	if (vflag)
		printf("|%.2x", *p);
	p++; len--; hdr++;
	if (elem->asnlen & ASN_BIT8) {
		int noct = elem->asnlen % ASN_BIT8;
		elem->asnlen = 0;
		if (len < noct) {
			ifNotTruncated printf("[asnlen? %d<%d]", len, noct);
			return -1;
		}
		for (; noct-- > 0; len--, hdr++) {
			if (vflag)
				printf("|%.2x", *p);
			elem->asnlen = (elem->asnlen << ASN_SHIFT8) | *p++;
		}
	}
	if (len < elem->asnlen) {
		if (!truncated) {
			printf("[len%d<asnlen%u]", len, elem->asnlen);
			return -1;
		}
		/* maybe should check at least 4? */
		elem->asnlen = len;
	}
	if (form >= sizeof(Form)/sizeof(Form[0])) {
		ifNotTruncated printf("[form?%d]", form);
		return -1;
	}
	if (class >= sizeof(Class)/sizeof(Class[0])) {
		ifNotTruncated printf("[class?%c/%d]", *Form[form], class);
		return -1;
	}
	if ((int)id >= Class[class].numIDs) {
		ifNotTruncated printf("[id?%c/%s/%d]", *Form[form],
			Class[class].name, id);
		return -1;
	}

	switch (form) {
	case PRIMITIVE:
		switch (class) {
		case UNIVERSAL:
			switch (id) {
			case STRING:
				elem->type = BE_STR;
				elem->data.str = p;
				break;

			case INTEGER: {
				register int32_t data;
				elem->type = BE_INT;
				data = 0;

				if (*p & ASN_BIT8)	/* negative */
					data = -1;
				for (i = elem->asnlen; i-- > 0; p++)
					data = (data << ASN_SHIFT8) | *p;
				elem->data.integer = data;
				break;
			}

			case OBJECTID:
				elem->type = BE_OID;
				elem->data.raw = (caddr_t)p;
				break;

			case ASN_NULL:
				elem->type = BE_NULL;
				elem->data.raw = NULL;
				break;

			default:
				elem->type = BE_OCTET;
				elem->data.raw = (caddr_t)p;
				printf("[P/U/%s]",
					Class[class].Id[id]);
				break;
			}
			break;

		case APPLICATION:
			switch (id) {
			case IPADDR:
				elem->type = BE_INETADDR;
				elem->data.raw = (caddr_t)p;
				break;

			case COUNTER:
			case GAUGE:
			case TIMETICKS: {
				register u_int32_t data;
				elem->type = BE_UNS;
				data = 0;
				for (i = elem->asnlen; i-- > 0; p++)
					data = (data << 8) + *p;
				elem->data.uns = data;
				break;
			}

			default:
				elem->type = BE_OCTET;
				elem->data.raw = (caddr_t)p;
				printf("[P/A/%s]",
					Class[class].Id[id]);
				break;
			}
			break;

		default:
			elem->type = BE_OCTET;
			elem->data.raw = (caddr_t)p;
			printf("[P/%s/%s]",
				Class[class].name, Class[class].Id[id]);
			break;
		}
		break;

	case CONSTRUCTED:
		switch (class) {
		case UNIVERSAL:
			switch (id) {
			case SEQUENCE:
				elem->type = BE_SEQ;
				elem->data.raw = (caddr_t)p;
				break;

			default:
				elem->type = BE_OCTET;
				elem->data.raw = (caddr_t)p;
				printf("C/U/%s", Class[class].Id[id]);
				break;
			}
			break;

		case CONTEXT:
			elem->type = BE_PDU;
			elem->data.raw = (caddr_t)p;
			break;

		default:
			elem->type = BE_OCTET;
			elem->data.raw = (caddr_t)p;
			printf("C/%s/%s",
				Class[class].name, Class[class].Id[id]);
			break;
		}
		break;
	}
	p += elem->asnlen;
	len -= elem->asnlen;
	return elem->asnlen + hdr;
}

/*
 * Display the ASN.1 object represented by the BE object.
 * This used to be an integral part of asn1_parse() before the intermediate
 * BE form was added.
 */
static void
asn1_print(struct be *elem)
{
	u_char *p = (u_char *)elem->data.raw;
	u_int32_t asnlen = elem->asnlen;
	int i;

	switch (elem->type) {

	case BE_OCTET:
		for (i = asnlen; i-- > 0; p++);
			printf("_%.2x", *p);
		break;

	case BE_NULL:
		break;

	case BE_OID: {
	int o = 0, first = -1, i = asnlen;

		if (!nflag && asnlen > 2) {
			struct obj_abrev *a = &obj_abrev_list[0];
			for (; a->node; a++) {
				if (!memcmp(a->oid, (char *)p,
				    strlen(a->oid))) {
					objp = a->node->child;
					i -= strlen(a->oid);
					p += strlen(a->oid);
					fputs(a->prefix, stdout);
					first = 1;
					break;
				}
			}
		}
		for (; i-- > 0; p++) {
			o = (o << ASN_SHIFT7) + (*p & ~ASN_BIT8);
			if (*p & ASN_LONGLEN)
				continue;

			/*
			 * first subitem encodes two items with 1st*OIDMUX+2nd
			 */
			if (first < 0) {
				if (!nflag)
					objp = mibroot;
				first = 0;
				OBJ_PRINT(o/OIDMUX, first);
				o %= OIDMUX;
			}
			OBJ_PRINT(o, first);
			if (--first < 0)
				first = 0;
			o = 0;
		}
		break;
	}

	case BE_INT:
		printf("%d", elem->data.integer);
		break;

	case BE_UNS:
		printf("%d", elem->data.uns);
		break;

	case BE_STR: {
		register int printable = 1, first = 1;
		const u_char *p = elem->data.str;
		for (i = asnlen; printable && i-- > 0; p++)
			printable = isprint(*p) || isspace(*p);
		p = elem->data.str;
		if (printable) {
			putchar('"');
			(void)fn_print(p, p + asnlen);
			putchar('"');
		} else
			for (i = asnlen; i-- > 0; p++) {
				printf(first ? "%.2x" : "_%.2x", *p);
				first = 0;
			}
		break;
	}

	case BE_SEQ:
		printf("Seq(%u)", elem->asnlen);
		break;

	case BE_INETADDR: {
		char sep;
		if (asnlen != ASNLEN_INETADDR)
			printf("[inetaddr len!=%d]", ASNLEN_INETADDR);
		sep='[';
		for (i = asnlen; i-- > 0; p++) {
			printf("%c%u", sep, *p);
			sep='.';
		}
		putchar(']');
		break;
	}

	case BE_PDU:
		printf("%s(%u)",
			Class[CONTEXT].Id[elem->id], elem->asnlen);
		break;

	case BE_ANY:
		fputs("[BE_ANY!?]", stdout);
		break;

	default:
		fputs("[be!?]", stdout);
		break;
	}
}

#ifdef notdef
/*
 * This is a brute force ASN.1 printer: recurses to dump an entire structure.
 * This will work for any ASN.1 stream, not just an SNMP PDU.
 *
 * By adding newlines and spaces at the correct places, this would print in
 * Rose-Normal-Form.
 *
 * This is not currently used.
 */
static void
asn1_decode(u_char *p, u_int length)
{
	struct be elem;
	int i = 0;

	while (i >= 0 && length > 0) {
		i = asn1_parse(p, length, &elem);
		if (i >= 0) {
			fputs(" ", stdout);
			asn1_print(&elem);
			if (elem.type == BE_SEQ || elem.type == BE_PDU) {
				fputs(" {", stdout);
				asn1_decode(elem.data.raw, elem.asnlen);
				fputs(" }", stdout);
			}
			length -= i;
			p += i;
		}
	}
}
#endif

/*
 * General SNMP header
 *	SEQUENCE {
 *		version INTEGER {version-1(0)},
 *		community OCTET STRING,
 *		data ANY	-- PDUs
 *	}
 * PDUs for all but Trap: (see rfc1157 from page 15 on)
 *	SEQUENCE {
 *		request-id INTEGER,
 *		error-status INTEGER,
 *		error-index INTEGER,
 *		varbindlist SEQUENCE OF
 *			SEQUENCE {
 *				name ObjectName,
 *				value ObjectValue
 *			}
 *	}
 * PDU for Trap:
 *	SEQUENCE {
 *		enterprise OBJECT IDENTIFIER,
 *		agent-addr NetworkAddress,
 *		generic-trap INTEGER,
 *		specific-trap INTEGER,
 *		time-stamp TimeTicks,
 *		varbindlist SEQUENCE OF
 *			SEQUENCE {
 *				name ObjectName,
 *				value ObjectValue
 *			}
 *	}
 */

/*
 * Decode SNMP varBind
 */
static void
varbind_print(u_char pduid, const u_char *np, u_int length, int error)
{
	struct be elem;
	int count = 0, ind;

	/* Sequence of varBind */
	if ((count = asn1_parse(np, length, &elem)) < 0)
		return;
	if (elem.type != BE_SEQ) {
		fputs("[!SEQ of varbind]", stdout);
		asn1_print(&elem);
		return;
	}
	if (count < length)
		printf("[%d extra after SEQ of varbind]", length - count);
	/* descend */
	length = elem.asnlen;
	np = (u_char *)elem.data.raw;

	for (ind = 1; length > 0; ind++) {
		const u_char *vbend;
		u_int vblength;

		if (!error || ind == error)
			fputs(" ", stdout);

		/* Sequence */
		if ((count = asn1_parse(np, length, &elem)) < 0)
			return;
		if (elem.type != BE_SEQ) {
			fputs("[!varbind]", stdout);
			asn1_print(&elem);
			return;
		}
		vbend = np + count;
		vblength = length - count;
		/* descend */
		length = elem.asnlen;
		np = (u_char *)elem.data.raw;

		/* objName (OID) */
		if ((count = asn1_parse(np, length, &elem)) < 0)
			return;
		if (elem.type != BE_OID) {
			fputs("[objName!=OID]", stdout);
			asn1_print(&elem);
			return;
		}
		if (!error || ind == error)
			asn1_print(&elem);
		length -= count;
		np += count;

		if (pduid != GETREQ && pduid != GETNEXTREQ && !error)
				fputs("=", stdout);

		/* objVal (ANY) */
		if ((count = asn1_parse(np, length, &elem)) < 0)
			return;
		if (pduid == GETREQ || pduid == GETNEXTREQ) {
			if (elem.type != BE_NULL) {
				fputs("[objVal!=NULL]", stdout);
				asn1_print(&elem);
			}
		} else
			if (error && ind == error && elem.type != BE_NULL)
				fputs("[err objVal!=NULL]", stdout);
			if (!error || ind == error)
				asn1_print(&elem);

		length = vblength;
		np = vbend;
	}
}

/*
 * Decode SNMP PDUs: GetRequest, GetNextRequest, GetResponse, and SetRequest
 */
static void
snmppdu_print(u_char pduid, const u_char *np, u_int length)
{
	struct be elem;
	int count = 0, error;

	/* reqId (Integer) */
	if ((count = asn1_parse(np, length, &elem)) < 0)
		return;
	if (elem.type != BE_INT) {
		fputs("[reqId!=INT]", stdout);
		asn1_print(&elem);
		return;
	}
	/* ignore the reqId */
	length -= count;
	np += count;

	/* errorStatus (Integer) */
	if ((count = asn1_parse(np, length, &elem)) < 0)
		return;
	if (elem.type != BE_INT) {
		fputs("[errorStatus!=INT]", stdout);
		asn1_print(&elem);
		return;
	}
	error = 0;
	if ((pduid == GETREQ || pduid == GETNEXTREQ)
	    && elem.data.integer != 0) {
		char errbuf[10];
		printf("[errorStatus(%s)!=0]",
			DECODE_ErrorStatus(elem.data.integer));
	} else if (elem.data.integer != 0) {
		char errbuf[10];
		printf(" %s", DECODE_ErrorStatus(elem.data.integer));
		error = elem.data.integer;
	}
	length -= count;
	np += count;

	/* errorIndex (Integer) */
	if ((count = asn1_parse(np, length, &elem)) < 0)
		return;
	if (elem.type != BE_INT) {
		fputs("[errorIndex!=INT]", stdout);
		asn1_print(&elem);
		return;
	}
	if ((pduid == GETREQ || pduid == GETNEXTREQ)
	    && elem.data.integer != 0)
		printf("[errorIndex(%d)!=0]", elem.data.integer);
	else if (elem.data.integer != 0) {
		if (!error)
			printf("[errorIndex(%d) w/o errorStatus]",
				elem.data.integer);
		else {
			printf("@%d", elem.data.integer);
			error = elem.data.integer;
		}
	} else if (error) {
		fputs("[errorIndex==0]", stdout);
		error = 0;
	}
	length -= count;
	np += count;

	varbind_print(pduid, np, length, error);
	return;
}

/*
 * Decode SNMP Trap PDU
 */
static void
trap_print(const u_char *np, u_int length)
{
	struct be elem;
	int count = 0, generic;

	putchar(' ');

	/* enterprise (oid) */
	if ((count = asn1_parse(np, length, &elem)) < 0)
		return;
	if (elem.type != BE_OID) {
		fputs("[enterprise!=OID]", stdout);
		asn1_print(&elem);
		return;
	}
	asn1_print(&elem);
	length -= count;
	np += count;

	putchar(' ');

	/* agent-addr (inetaddr) */
	if ((count = asn1_parse(np, length, &elem)) < 0)
		return;
	if (elem.type != BE_INETADDR) {
		fputs("[agent-addr!=INETADDR]", stdout);
		asn1_print(&elem);
		return;
	}
	asn1_print(&elem);
	length -= count;
	np += count;

	/* generic-trap (Integer) */
	if ((count = asn1_parse(np, length, &elem)) < 0)
		return;
	if (elem.type != BE_INT) {
		fputs("[generic-trap!=INT]", stdout);
		asn1_print(&elem);
		return;
	}
	generic = elem.data.integer;
	{
		char buf[10];
		printf(" %s", DECODE_GenericTrap(generic));
	}
	length -= count;
	np += count;

	/* specific-trap (Integer) */
	if ((count = asn1_parse(np, length, &elem)) < 0)
		return;
	if (elem.type != BE_INT) {
		fputs("[specific-trap!=INT]", stdout);
		asn1_print(&elem);
		return;
	}
	if (generic != GT_ENTERPRISE) {
		if (elem.data.integer != 0)
			printf("[specific-trap(%d)!=0]", elem.data.integer);
	} else
		printf(" s=%d", elem.data.integer);
	length -= count;
	np += count;

	putchar(' ');

	/* time-stamp (TimeTicks) */
	if ((count = asn1_parse(np, length, &elem)) < 0)
		return;
	if (elem.type != BE_UNS) {			/* XXX */
		fputs("[time-stamp!=TIMETICKS]", stdout);
		asn1_print(&elem);
		return;
	}
	asn1_print(&elem);
	length -= count;
	np += count;

	varbind_print (TRAP, np, length, 0);
	return;
}

/*
 * Decode SNMP header and pass on to PDU printing routines
 */
void
snmp_print(const u_char *np, u_int length)
{
	struct be elem, pdu;
	int count = 0;

	truncated = 0;

	/* truncated packet? */
	if (np + length > snapend) {
		truncated = 1;
		length = snapend - np;
	}

	putchar(' ');

	/* initial Sequence */
	if ((count = asn1_parse(np, length, &elem)) < 0)
		return;
	if (elem.type != BE_SEQ) {
		fputs("[!init SEQ]", stdout);
		asn1_print(&elem);
		return;
	}
	if (count < length)
		printf("[%d extra after iSEQ]", length - count);
	/* descend */
	length = elem.asnlen;
	np = (u_char *)elem.data.raw;
	/* Version (Integer) */
	if ((count = asn1_parse(np, length, &elem)) < 0)
		return;
	if (elem.type != BE_INT) {
		fputs("[version!=INT]", stdout);
		asn1_print(&elem);
		return;
	}
	/* only handle version==0 */
	if (elem.data.integer != DEF_VERSION) {
		printf("[version(%d)!=0]", elem.data.integer);
		return;
	}
	length -= count;
	np += count;

	/* Community (String) */
	if ((count = asn1_parse(np, length, &elem)) < 0)
		return;
	if (elem.type != BE_STR) {
		fputs("[comm!=STR]", stdout);
		asn1_print(&elem);
		return;
	}
	/* default community */
	if (strncmp((char *)elem.data.str, DEF_COMMUNITY,
	    sizeof(DEF_COMMUNITY) - 1))
		/* ! "public" */
		printf("C=%.*s ", (int)elem.asnlen, elem.data.str);
	length -= count;
	np += count;

	/* PDU (Context) */
	if ((count = asn1_parse(np, length, &pdu)) < 0)
		return;
	if (pdu.type != BE_PDU) {
		fputs("[no PDU]", stdout);
		return;
	}
	if (count < length)
		printf("[%d extra after PDU]", length - count);
	asn1_print(&pdu);
	/* descend into PDU */
	length = pdu.asnlen;
	np = (u_char *)pdu.data.raw;

	switch (pdu.id) {
	case TRAP:
		trap_print(np, length);
		break;
	case GETREQ:
	case GETNEXTREQ:
	case GETRESP:
	case SETREQ:
		snmppdu_print(pdu.id, np, length);
		break;
	}
	return;
}
