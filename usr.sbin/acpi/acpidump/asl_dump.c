/*-
 * Copyright (c) 1999 Doug Rabson
 * Copyright (c) 2000 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: asl_dump.c,v 1.6 2000/08/09 14:47:52 iwasaki Exp $
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/acpi.h>

#include <assert.h>
#include <err.h>
#include <stdio.h>

#include "acpidump.h"

static u_int32_t
asl_dump_pkglength(u_int8_t **dpp)
{
	u_int8_t	*dp;
	u_int32_t	pkglength;

	dp = *dpp;
	pkglength = *dp++;
	switch (pkglength >> 6) {
	case 0:
		break;
	case 1:
		pkglength = (pkglength & 0xf) + (dp[0] << 4);
		dp += 1;
		break;
	case 2:
		pkglength = (pkglength & 0xf) + (dp[0] << 4) + (dp[1] << 12);
		dp += 2;
		break;
	case 3:
		pkglength = (pkglength & 0xf)
			+ (dp[0] << 4) + (dp[1] << 12) + (dp[2] << 20);
		dp += 3;
		break;
	}

	*dpp = dp;
	return (pkglength);
}

static void
print_nameseg(u_int8_t *dp)
{

	if (dp[3] != '_')
		printf("%c%c%c%c", dp[0], dp[1], dp[2], dp[3]);
	else if (dp[2] != '_')
		printf("%c%c%c_", dp[0], dp[1], dp[2]);
	else if (dp[1] != '_')
		printf("%c%c__", dp[0], dp[1]);
	else if (dp[0] != '_')
		printf("%c___", dp[0]);
}

static u_int8_t
asl_dump_bytedata(u_int8_t **dpp)
{
	u_int8_t	*dp;
	u_int8_t	data;

	dp = *dpp;
	data = dp[0];
	*dpp = dp + 1;
	return (data);
}

static u_int16_t
asl_dump_worddata(u_int8_t **dpp)
{
	u_int8_t	*dp;
	u_int16_t	data;

	dp = *dpp;
	data = dp[0] + (dp[1] << 8);
	*dpp = dp + 2;
	return (data);
}

static u_int32_t
asl_dump_dworddata(u_int8_t **dpp)
{
	u_int8_t	*dp;
	u_int32_t	data;

	dp = *dpp;
	data = dp[0] + (dp[1] << 8) + (dp[2] << 16) + (dp[3] << 24);
	*dpp = dp + 4;
	return (data);
}

static u_int8_t *
asl_dump_namestring(u_int8_t **dpp)
{
	u_int8_t	*dp;
	u_int8_t	*name;

	dp = *dpp;
	name = dp;
	if (dp[0] == '\\')
		dp++;
	else if (dp[0] == '^')
		while (dp[0] == '^')
			dp++;
	if (dp[0] == 0x00)	/* NullName */
		dp++;
	else if (dp[0] == 0x2e)	/* DualNamePrefix */
		dp += 1 + 4 + 4;/* NameSeg, NameSeg */
	else if (dp[0] == 0x2f) {	/* MultiNamePrefix */
		int             segcount = dp[1];
		dp += 1 + 1 + segcount * 4;	/* segcount * NameSeg */
	} else
		dp += 4;	/* NameSeg */

	*dpp = dp;
	return (name);
}

static void
print_namestring(u_int8_t *dp)
{

	if (dp[0] == '\\') {
		putchar(dp[0]);
		dp++;
	} else if (dp[0] == '^') {
		while (dp[0] == '^') {
			putchar(dp[0]);
			dp++;
		}
	}
	if (dp[0] == 0x00) {	/* NullName */
		/* printf("<null>"); */
		dp++;
	} else if (dp[0] == 0x2e) {	/* DualNamePrefix */
		print_nameseg(dp + 1);
		putchar('.');
		print_nameseg(dp + 5);
	} else if (dp[0] == 0x2f) {	/* MultiNamePrefix */
		int             segcount = dp[1];
		int             i;
		for (i = 0, dp += 2; i < segcount; i++, dp += 4) {
			if (i > 0)
				putchar('.');
			print_nameseg(dp);
		}
	} else			/* NameSeg */
		print_nameseg(dp);
}

static void
print_indent(int indent)
{
	int	i;

	for (i = 0; i < indent; i++)
		printf("    ");
}

static void
asl_dump_defscope(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int32_t	pkglength;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);

	printf("Scope(");
	asl_dump_termobj(&dp, indent);
	printf(") {\n");
	end = start + pkglength;
	asl_dump_objectlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);

	*dpp = dp;
}

static void
asl_dump_defbuffer(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int32_t	pkglength;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	end = start + pkglength;
	printf("Buffer(");
	asl_dump_termobj(&dp, indent);
	printf(") {");
	while (dp < end) {
		printf("0x%x", *dp++);
		if (dp < end)
			printf(", ");
	}
	printf(" }");

	*dpp = dp;
}

static void
asl_dump_defpackage(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int8_t	numelements;
	u_int32_t	pkglength;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	numelements = asl_dump_bytedata(&dp);
	end = start + pkglength;
	printf("Package(0x%x) {\n", numelements);
	while (dp < end) {
		print_indent(indent + 1);
		asl_dump_termobj(&dp, indent + 1);
		printf(",\n");
	}

	print_indent(indent);
	printf("}");

	dp = end;

	*dpp = dp;
}

static void
asl_dump_defmethod(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int8_t	flags;
	u_int32_t	pkglength;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);

	printf("Method(");
	asl_dump_termobj(&dp, indent);
	flags = *dp++;
	if (flags) {
		printf(", %d", flags & 7);
		if (flags & 8) {
			printf(", Serialized");
		}
	}
	printf(") {\n");
	end = start + pkglength;
	asl_dump_objectlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);

	*dpp = dp;
}


static void
asl_dump_defopregion(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	const	char *regions[] = {
		"SystemMemory",
		"SystemIO",
		"PCI_Config",
		"EmbeddedControl",
		"SMBus",
	};

	dp = *dpp;
	printf("OperationRegion(");
	asl_dump_termobj(&dp, indent);	/* Name */
	printf(", %s, ", regions[*dp++]);	/* Space */
	asl_dump_termobj(&dp, indent);	/* Offset */
	printf(", ");
	asl_dump_termobj(&dp, indent);	/* Length */
	printf(")");

	*dpp = dp;
}

static const char *accessnames[] = {
	"AnyAcc",
	"ByteAcc",
	"WordAcc",
	"DWordAcc",
	"BlockAcc",
	"SMBSendRecvAcc",
	"SMBQuickAcc"
};

static int
asl_dump_field(u_int8_t **dpp, u_int32_t offset)
{
	u_int8_t	*dp;
	u_int8_t	*name;
	u_int8_t	access, attribute;
	u_int32_t	width;

	dp = *dpp;
	switch (*dp) {
	case '\\':
	case '^':
	case 'A' ... 'Z':
	case '_':
	case '.':
		name = asl_dump_namestring(&dp);
		width = asl_dump_pkglength(&dp);
		offset += width;
		print_namestring(name);
		printf(",\t%d", width);
		break;
	case 0x00:
		dp++;
		width = asl_dump_pkglength(&dp);
		offset += width;
		if ((offset % 8) == 0) {
			printf("Offset(0x%x)", offset / 8);
		} else {
			printf(",\t%d", width);
		}
		break;
	case 0x01:
		access = dp[1];
		attribute = dp[2];
		dp += 3;
		printf("AccessAs(%s, %d)", accessnames[access], attribute);
		break;
	}

	*dpp = dp;
	return (offset);
}

static void
asl_dump_fieldlist(u_int8_t **dpp, u_int8_t *end, int indent)
{
	u_int8_t	*dp;
	u_int32_t	offset;

	dp = *dpp;
	offset = 0;
	while (dp < end) {
		print_indent(indent);
		offset = asl_dump_field(&dp, offset);
		if (dp < end)
			printf(",\n");
		else
			printf("\n");
	}

	*dpp = dp;
}

static void
asl_dump_deffield(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int8_t	flags;
	u_int32_t	pkglength;
	static	const char *lockrules[] = {"NoLock", "Lock"};
	static	const char *updaterules[] = {"Preserve", "WriteAsOnes",
					     "WriteAsZeros", "*Error*"};

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	end = start + pkglength;

	printf("Field(");
	asl_dump_termobj(&dp, indent);	/* Name */
	flags = asl_dump_bytedata(&dp);
	printf(", %s, %s, %s) {\n",
	       accessnames[flags & 0xf],
	       lockrules[(flags >> 4) & 1],
	       updaterules[(flags >> 5) & 3]);
	asl_dump_fieldlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);

	*dpp = dp;
}

static void
asl_dump_defindexfield(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int8_t	flags;
	u_int32_t	pkglength;
	static	const char *lockrules[] = {"NoLock", "Lock"};
	static	const char *updaterules[] = {"Preserve", "WriteAsOnes",
					     "WriteAsZeros", "*Error*"};

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	end = start + pkglength;

	printf("IndexField(");
	asl_dump_termobj(&dp, indent);	/* Name1 */
	printf(", ");
	asl_dump_termobj(&dp, indent);	/* Name2 */
	flags = asl_dump_bytedata(&dp);
	printf(", %s, %s, %s) {\n",
	       accessnames[flags & 0xf],
	       lockrules[(flags >> 4) & 1],
	       updaterules[(flags >> 5) & 3]);
	asl_dump_fieldlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);

	*dpp = dp;
}

static void
asl_dump_defbankfield(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int8_t	flags;
	u_int32_t	pkglength;
	static	const char *lockrules[] = {"NoLock", "Lock"};
	static	const char *updaterules[] = {"Preserve", "WriteAsOnes",
					     "WriteAsZeros", "*Error*"};

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	end = start + pkglength;

	printf("Field(");
	asl_dump_termobj(&dp, indent);	/* Name1 */
	printf(", ");
	asl_dump_termobj(&dp, indent);	/* Name2 */
	printf(", ");
	asl_dump_termobj(&dp, indent);	/* BankValue */
	flags = asl_dump_bytedata(&dp);
	printf(", %s, %s, %s) {\n",
	       accessnames[flags & 0xf],
	       lockrules[(flags >> 4) & 1],
	       updaterules[(flags >> 5) & 3]);
	asl_dump_fieldlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);

	*dpp = dp;
}

static void
asl_dump_defdevice(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int32_t	pkglength;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	end = start + pkglength;

	printf("Device(");
	asl_dump_termobj(&dp, indent);
	printf(") {\n");
	asl_dump_objectlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);

	*dpp = dp;
}

static void
asl_dump_defprocessor(u_int8_t **dpp, int indent)
{
	u_int8_t       *dp;
	u_int8_t       *start;
	u_int8_t       *end;
	u_int8_t        procid;
	u_int8_t        pblklen;
	u_int32_t       pkglength;
	u_int32_t       pblkaddr;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	end = start + pkglength;

	printf("Processor(");
	asl_dump_termobj(&dp, indent);
	procid = asl_dump_bytedata(&dp);
	pblkaddr = asl_dump_dworddata(&dp);
	pblklen = asl_dump_bytedata(&dp);
	printf(", %d, 0x%x, 0x%x) {\n", procid, pblkaddr, pblklen);
	asl_dump_objectlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);

	*dpp = dp;
}

static void
asl_dump_defpowerres(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int8_t	systemlevel;
	u_int16_t	resourceorder;
	u_int32_t	pkglength;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	end = start + pkglength;

	printf("PowerResource(");
	asl_dump_termobj(&dp, indent);
	systemlevel = asl_dump_bytedata(&dp);
	resourceorder = asl_dump_worddata(&dp);
	printf(", %d, %d) {\n", systemlevel, resourceorder);
	asl_dump_objectlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);

	*dpp = dp;
}

static void
asl_dump_defthermalzone(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int32_t	pkglength;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	end = start + pkglength;

	printf("ThermalZone(");
	asl_dump_termobj(&dp, indent);
	printf(") {\n");
	asl_dump_objectlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);

	*dpp = dp;
}

static void
asl_dump_defif(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int32_t	pkglength;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	end = start + pkglength;

	printf("If(");
	asl_dump_termobj(&dp, indent);
	printf(") {\n");
	asl_dump_objectlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);

	*dpp = dp;
}

static void
asl_dump_defelse(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int32_t       pkglength;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	end = start + pkglength;

	printf("Else {\n");
	asl_dump_objectlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);

	*dpp = dp;
}

static void
asl_dump_defwhile(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int32_t	pkglength;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	end = start + pkglength;

	printf("While(");
	asl_dump_termobj(&dp, indent);
	printf(") {\n");
	asl_dump_objectlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);

	*dpp = dp;
}

/*
 * Public interfaces
 */

void
asl_dump_termobj(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	opcode;
	const	char *matchstr[] = {
		"MTR", "MEQ", "MLE", "MLT", "MGE", "MGT",
	};

#define OPTARG() do {						\
	printf(", ");						\
	if (*dp == 0x00) {					\
	    dp++;						\
	} else { 						\
	    asl_dump_termobj(&dp, indent);			\
	}							\
} while (0)

	dp = *dpp;
	opcode = *dp++;
	switch (opcode) {
	case '\\':
	case '^':
	case 'A' ... 'Z':
	case '_':
	case '.':
		dp--;
		print_namestring(asl_dump_namestring(&dp));
		break;
	case 0x0a:		/* BytePrefix */
		printf("0x%x", asl_dump_bytedata(&dp));
		break;
	case 0x0b:		/* WordPrefix */
		printf("0x%04x", asl_dump_worddata(&dp));
		break;
	case 0x0c:		/* DWordPrefix */
		printf("0x%08x", asl_dump_dworddata(&dp));
		break;
	case 0x0d:		/* StringPrefix */
		printf("\"%s\"", (const char *) dp);
		while (*dp)
			dp++;
		dp++;		/* NUL terminate */
		break;
	case 0x00:		/* ZeroOp */
		printf("Zero");
		break;
	case 0x01:		/* OneOp */
		printf("One");
		break;
	case 0xff:		/* OnesOp */
		printf("Ones");
		break;
	case 0x06:		/* AliasOp */
		printf("Alias(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		printf(")");
		break;
	case 0x08:		/* NameOp */
		printf("Name(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		printf(")");
		break;
	case 0x10:		/* ScopeOp */
		asl_dump_defscope(&dp, indent);
		break;
	case 0x11:		/* BufferOp */
		asl_dump_defbuffer(&dp, indent);
		break;
	case 0x12:		/* PackageOp */
		asl_dump_defpackage(&dp, indent);
		break;
	case 0x14:		/* MethodOp */
		asl_dump_defmethod(&dp, indent);
		break;
	case 0x5b:		/* ExtOpPrefix */
		opcode = *dp++;
		switch (opcode) {
		case 0x01:	/* MutexOp */
			printf("Mutex(");
			asl_dump_termobj(&dp, indent);
			printf(", %d)", *dp++);
			break;
		case 0x02:	/* EventOp */
			printf("Event(");
			asl_dump_termobj(&dp, indent);
			printf(")");
			break;
		case 0x12:	/* CondRefOfOp */
			printf("CondRefOf(");
			asl_dump_termobj(&dp, indent);
			printf(", ");
			asl_dump_termobj(&dp, indent);
			printf(")");
			break;
		case 0x13:	/* CreateFieldOp */
			printf("CreateField(");
			asl_dump_termobj(&dp, indent);
			printf(", ");
			asl_dump_termobj(&dp, indent);
			printf(", ");
			asl_dump_termobj(&dp, indent);
			printf(", ");
			asl_dump_termobj(&dp, indent);
			printf(")");
			break;
		case 0x20:	/* LoadOp */
			printf("Load(");
			asl_dump_termobj(&dp, indent);
			printf(", ");
			asl_dump_termobj(&dp, indent);
			printf(")");
			break;
		case 0x21:	/* StallOp */
			printf("Stall(");
			asl_dump_termobj(&dp, indent);
			printf(")");
			break;
		case 0x22:	/* SleepOp */
			printf("Sleep(");
			asl_dump_termobj(&dp, indent);
			printf(")");
			break;
		case 0x23:	/* AcquireOp */
			printf("Acquire(");
			asl_dump_termobj(&dp, indent);
			printf(", 0x%x)", asl_dump_worddata(&dp));
			break;
		case 0x24:	/* SignalOp */
			printf("Signal(");
			asl_dump_termobj(&dp, indent);
			printf(")");
			break;
		case 0x25:	/* WaitOp */
			printf("Wait(");
			asl_dump_termobj(&dp, indent);
			printf(", ");
			asl_dump_termobj(&dp, indent);
			printf(")");
			break;
		case 0x26:	/* ResetOp */
			printf("Reset(");
			asl_dump_termobj(&dp, indent);
			printf(")");
			break;
		case 0x27:	/* ReleaseOp */
			printf("Release(");
			asl_dump_termobj(&dp, indent);
			printf(")");
			break;
		case 0x28:	/* FromBCDOp */
			printf("FromBCD(");
			asl_dump_termobj(&dp, indent);
			printf(", ");
			asl_dump_termobj(&dp, indent);
			printf(")");
			break;
		case 0x29:	/* ToBCDOp */
			printf("ToBCD(");
			asl_dump_termobj(&dp, indent);
			OPTARG();
			printf(")");
			break;
		case 0x2a:	/* UnloadOp */
			printf("Unload(");
			asl_dump_termobj(&dp, indent);
			printf(")");
			break;
		case 0x30:
			printf("Revision");
			break;
		case 0x31:
			printf("Debug");
			break;
		case 0x32:	/* FatalOp */
			printf("Fatal(");
			printf("0x%x, ", asl_dump_bytedata(&dp));
			printf("0x%x, ", asl_dump_dworddata(&dp));
			asl_dump_termobj(&dp, indent);
			printf(")");
			break;
		case 0x80:	/* OpRegionOp */
			asl_dump_defopregion(&dp, indent);
			break;
		case 0x81:	/* FieldOp */
			asl_dump_deffield(&dp, indent);
			break;
		case 0x82:	/* DeviceOp */
			asl_dump_defdevice(&dp, indent);
			break;
		case 0x83:	/* ProcessorOp */
			asl_dump_defprocessor(&dp, indent);
			break;
		case 0x84:	/* PowerResOp */
			asl_dump_defpowerres(&dp, indent);
			break;
		case 0x85:	/* ThermalZoneOp */
			asl_dump_defthermalzone(&dp, indent);
			break;
		case 0x86:	/* IndexFieldOp */
			asl_dump_defindexfield(&dp, indent);
			break;
		case 0x87:	/* BankFieldOp */
			asl_dump_defbankfield(&dp, indent);
			break;
		default:
			errx(1, "strange opcode 0x5b, 0x%x\n", opcode);
		}
		break;
	case 0x68 ... 0x6e:	/* ArgN */
		printf("Arg%d", opcode - 0x68);
		break;
	case 0x60 ... 0x67:
		printf("Local%d", opcode - 0x60);
		break;
	case 0x70:		/* StoreOp */
		printf("Store(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		printf(")");
		break;
	case 0x71:		/* RefOfOp */
		printf("RefOf(");
		asl_dump_termobj(&dp, indent);
		printf(")");
		break;
	case 0x72:		/* AddOp */
		printf("Add(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		OPTARG();
		printf(")");
		break;
	case 0x73:		/* ConcatOp */
		printf("Concat(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		printf(")");
		break;
	case 0x74:		/* SubtractOp */
		printf("Subtract(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		OPTARG();
		printf(")");
		break;
	case 0x75:		/* IncrementOp */
		printf("Increment(");
		asl_dump_termobj(&dp, indent);
		printf(")");
		break;
	case 0x76:		/* DecrementOp */
		printf("Decrement(");
		asl_dump_termobj(&dp, indent);
		printf(")");
		break;
	case 0x77:		/* MultiplyOp */
		printf("Multiply(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		OPTARG();
		printf(")");
		break;
	case 0x78:		/* DivideOp */
		printf("Divide(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		OPTARG();
		OPTARG();
		printf(")");
		break;
	case 0x79:		/* ShiftLeftOp */
		printf("ShiftLeft(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		OPTARG();
		printf(")");
		break;
	case 0x7a:		/* ShiftRightOp */
		printf("ShiftRight(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		OPTARG();
		printf(")");
		break;
	case 0x7b:		/* AndOp */
		printf("And(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		OPTARG();
		printf(")");
		break;
	case 0x7c:		/* NAndOp */
		printf("NAnd(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		OPTARG();
		printf(")");
		break;
	case 0x7d:		/* OrOp */
		printf("Or(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		OPTARG();
		printf(")");
		break;
	case 0x7e:		/* NOrOp */
		printf("NOr(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		OPTARG();
		printf(")");
		break;
	case 0x7f:		/* XOrOp */
		printf("XOr(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		OPTARG();
		printf(")");
		break;
	case 0x80:		/* NotOp */
		printf("Not(");
		asl_dump_termobj(&dp, indent);
		OPTARG();
		printf(")");
		break;
	case 0x81:		/* FindSetLeftBitOp */
		printf("FindSetLeftBit(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		printf(")");
		break;
	case 0x82:		/* FindSetRightBitOp */
		printf("FindSetRightBit(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		printf(")");
		break;
	case 0x83:		/* DerefOp */
		printf("DerefOf(");
		asl_dump_termobj(&dp, indent);
		printf(")");
		break;
	case 0x86:		/* NotifyOp */
		printf("Notify(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		printf(")");
		break;
	case 0x87:		/* SizeOfOp */
		printf("Sizeof(");
		asl_dump_termobj(&dp, indent);
		printf(")");
		break;
	case 0x88:		/* IndexOp */
		printf("Index(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		OPTARG();
		printf(")");
		break;
	case 0x89:		/* MatchOp */
		printf("Match(");
		asl_dump_termobj(&dp, indent);
		printf(", %s, ", matchstr[*dp++]);
		asl_dump_termobj(&dp, indent);
		printf(", %s, ", matchstr[*dp++]);
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		printf(")");
		break;
	case 0x8a:		/* CreateDWordFieldOp */
		printf("CreateDWordField(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		printf(")");
		break;
	case 0x8b:		/* CreateWordFieldOp */
		printf("CreateWordField(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		printf(")");
		break;
	case 0x8c:		/* CreateByteFieldOp */
		printf("CreateByteField(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		printf(")");
		break;
	case 0x8d:		/* CreateBitFieldOp */
		printf("CreateBitField(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		printf(")");
		break;
	case 0x8e:		/* ObjectTypeOp */
		printf("ObjectType(");
		asl_dump_termobj(&dp, indent);
		printf(")");
		break;
	case 0x90:
		printf("LAnd(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		printf(")");
		break;
	case 0x91:
		printf("LOr(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		printf(")");
		break;
	case 0x92:
		printf("LNot(");
		asl_dump_termobj(&dp, indent);
		printf(")");
		break;
	case 0x93:
		printf("LEqual(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		printf(")");
		break;
	case 0x94:
		printf("LGreater(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		printf(")");
		break;
	case 0x95:
		printf("LLess(");
		asl_dump_termobj(&dp, indent);
		printf(", ");
		asl_dump_termobj(&dp, indent);
		printf(")");
		break;
	case 0xa0:		/* IfOp */
		asl_dump_defif(&dp, indent);
		break;
	case 0xa1:		/* ElseOp */
		asl_dump_defelse(&dp, indent);
		break;
	case 0xa2:		/* WhileOp */
		asl_dump_defwhile(&dp, indent);
		break;
	case 0xa3:		/* NoopOp */
		printf("Noop");
		break;
	case 0xa5:		/* BreakOp */
		printf("Break");
		break;
	case 0xa4:		/* ReturnOp */
		printf("Return(");
		asl_dump_termobj(&dp, indent);
		printf(")");
		break;
	case 0xcc:		/* BreakPointOp */
		printf("BreakPoint");
		break;
	default:
		errx(1, "strange opcode 0x%x\n", opcode);
	}

	*dpp = dp;
}

void
asl_dump_objectlist(u_int8_t **dpp, u_int8_t *end, int indent)
{
	u_int8_t	*dp;

	dp = *dpp;
	while (dp < end) {
		print_indent(indent);
		asl_dump_termobj(&dp, indent);
		printf("\n");
	}

	*dpp = dp;
}
