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

#include <assert.h>
#include <err.h>
#include <stdio.h>

#include "acpidump.h"

#include "aml/aml_env.h"

struct aml_environ	asl_env;
int			rflag;

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

#define ASL_ENTER_SCOPE(dp_orig, old_name) do {				\
	u_int8_t	*dp_copy;					\
	u_int8_t	*name;						\
	old_name = asl_env.curname;					\
	dp_copy = dp_orig;						\
	name = asl_dump_namestring(&dp_copy);				\
	asl_env.curname = aml_search_name(&asl_env, name);		\
} while(0)

#define ASL_LEAVE_SCOPE(old_name) do {					\
	asl_env.curname = old_name;					\
} while(0)

static void
asl_dump_defscope(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int32_t	pkglength;
	struct	aml_name *oname;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);

	printf("Scope(");
	ASL_ENTER_SCOPE(dp, oname);
	asl_dump_termobj(&dp, indent);
	printf(") {\n");
	end = start + pkglength;
	asl_dump_objectlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);
	ASL_LEAVE_SCOPE(oname);
	*dpp = dp;
}

static void
asl_dump_resourcebuffer(u_int8_t *dp, u_int8_t *end, int indent)
{
	u_int8_t	*p;
	int		print, len, name, indep, i, ofs;

	print = 0;
	indep = 0;
restart:
	if (print) {
		printf("\n");
		print_indent(indent);
		printf("/* ResourceTemplate() {\n");
	}
	for (p = dp; p < end; ) {
		ofs = p - dp;
		if (*p & 0x80) {	/* large resource */
			if ((end - p) < 3) {
				return;
			}
			name = *p;
			len = ((int)*(p + 2) << 8) + *(p + 1);
			p += 3;
		} else {		/* small resource */
			name = (*p >> 3) & 0x0f;
			len = *p & 0x7;
			p++;
		}
		if (name == 0xf) {	/* end tag */
			if (print == 0) {
				print = 1;
				goto restart;
			} else {
				print_indent(indent);
				printf("} */\n");
				print_indent(indent);
				break;
			}
		}
		
		if (print) {
			print_indent(indent);
			switch (name) {
			case 0x06:
				if (indep) {
					printf("    }\n");
					print_indent(indent);
				}
				printf("    StartDependentFn(");
				if (len == 1) 
					printf("%d, %d", 
					       *p & 0x3,
					       (*p >> 2) & 0x3);
				printf(") {\n");
				indep = 1;
				continue;
			case 0x07:
				if (indep)
					printf("    }\n");
				print_indent(indent);
				printf("    EndDependentFn() {}\n");
				indep = 0;
				continue;
			}

			printf("%s 0x%-04.4x  ", indep ? "    " : "", ofs);
			switch (name) {
			case 0x04:	/* IRQ() { } */
			{
				int i, first;

				printf("IRQ(");
				if (len == 3) {
					printf("%s, Active%s, %s",
					       *(p + 2) & 0x01 ? "Edge" : "Level",
					       *(p + 2) & 0x08 ? "Low" : "High",
					       *(p + 2) & 0x10 ? "Shared" :
					       "Exclusive");
				}
				printf(")");
				first = 1;
				for (i = 0; i < 16; i++) {
					if (*(p + (i / 8)) & (1 << (i % 8))) {
						if (first) {
							printf(" {");
							first = 0;
						} else {
							printf(", ");
						}
						printf("%d", i);
					}
				}
				if (!first)
					printf("}");
				printf("\n");
				break;
			}

			case 0x05:	/* DMA() { } */
			{
				int i, first;

				printf("DMA(%s, %sBusMaster, Transfer%s)",
				       (*(p + 1) & 0x60) == 0 ? "Compatibility" :
				       (*(p + 1) & 0x60) == 1 ? "TypeA" :
				       (*(p + 1) & 0x60) == 2 ? "TypeB" : "TypeF",
				       *(p + 1) & 0x04 ? "" : "Not",
				       (*(p + 1) & 0x03) == 0 ? "8" :
				       (*(p + 1) & 0x03) == 1 ? "8_16" : "16");
				first = 1;
				for (i = 0; i < 8; i++) {
					if (*p & (1 << i)) {
						if (first) {
							printf(" {");
							first = 0;
						} else {
							printf(", ");
						}
						printf("%d", i);
					}
				}
				if (!first)
					printf("}");
				printf("\n");
				break;
			}
			case 0x08:	/* IO() */
				printf("IO(Decode%s, 0x%x, 0x%x, 0x%x, 0x%x)\n",
				       *p & 0x01 ? "16" : "10",
				       (int)*(u_int16_t *)(p + 1),
				       (int)*(u_int16_t *)(p + 3),
				       *(p + 5),
				       *(p + 6));
				break;

			case 0x09:	/* FixedIO() */
				printf("FixedIO(0x%x, 0x%x)\n",
				       *p + ((int)*(p + 1) << 8),
				       *(p + 2));
				break;

			case 0x0e:	/* VendorShort() { }*/
			case 0x84:	/* VendorLong() { } */
			{
				int i, first;

				printf("Vendor%s()", name == 0x0e ? "Short" : "Long");
				first = 0;
				for (i = 0; i < len; i++) {
					if (first) {
						printf(" {");
						first = 0;
					} else {
						printf(", ");
					}
					printf("0x%02x", *(p + i));
				}
				if (!first)
					printf("}");
				printf("\n");
				break;
			}
			case 0x81:	/* Memory24() */
				printf("Memory24(Read%s, 0x%06x, 0x%06x, 0x%x, 0x%x)\n",
				       *p & 0x01 ? "Write" : "Only",
				       (u_int32_t)*(u_int16_t *)(p + 1) << 8,
				       (u_int32_t)*(u_int16_t *)(p + 3) << 8,
				       (int)*(u_int16_t *)(p + 5),
				       (int)*(u_int16_t *)(p + 7));
				break;

			case 0x82:	/* Register() */
				printf("Register(%s, %d, %d, 0x%016llx)\n",
				       *p == 0x00 ? "SystemMemory" :
				       *p == 0x01 ? "SystemIO" :
				       *p == 0x02 ? "PCIConfigSpace" :
				       *p == 0x03 ? "EmbeddedController" :
				       *p == 0x04 ? "SMBus" :
				       *p == 0x7f ? "FunctionalFixedHardware" : "Unknown",
				       *(p + 1),
				       *(p + 2),
				       *(u_int64_t *)(p + 3));
				break;
				      
			case 0x85:	/* Memory32() */
				printf("Memory32(Read%s, 0x%08x, 0x%08x, 0x%x, 0x%x)\n",
				       *p & 0x01 ? "Write" : "Only",
				       *(u_int32_t *)(p + 1),
				       *(u_int32_t *)(p + 5),
				       *(u_int32_t *)(p + 9),
				       *(u_int32_t *)(p + 13));
				break;

			case 0x86:	/* Memory32Fixed() */
				printf("Memory32Fixed(Read%s, 0x%08x, 0x%x)\n",
				       *p & 0x01 ? "Write" : "Only",
				       *(u_int32_t *)(p + 1),
				       *(u_int32_t *)(p + 5));
				break;

			case 0x87:	/* DWordMemory() / DWordIO() */
			case 0x88:	/* WordMemory() / WordIO() */
			case 0x8a:	/* QWordMemory() / QWordIO() */
			{
				u_int64_t granularity, minimum, maximum, translation, length;
				char *size, *source;
				int index, slen;

				switch (name) {
				case 0x87:
					size = "D";
					granularity = *(u_int32_t *)(p + 3);
					minimum     = *(u_int32_t *)(p + 7);
					maximum     = *(u_int32_t *)(p + 11);
					translation = *(u_int32_t *)(p + 15);
					length      = *(u_int32_t *)(p + 19);
					index       = *(p + 23);
					source      = p + 24;
					slen        = len - 24;
					break;
				case 0x88:
					size = "";
					granularity = *(u_int16_t *)(p + 3);
					minimum     = *(u_int16_t *)(p + 5);
					maximum     = *(u_int16_t *)(p + 7);
					translation = *(u_int16_t *)(p + 9);
					length      = *(u_int16_t *)(p + 11);
					index       = *(p + 13);
					source      = p + 14;
					slen        = len - 14;
					break;
				case 0x8a:
					size = "Q";
					granularity = *(u_int64_t *)(p + 3);
					minimum     = *(u_int64_t *)(p + 11);
					maximum     = *(u_int64_t *)(p + 19);
					translation = *(u_int64_t *)(p + 27);
					length      = *(u_int64_t *)(p + 35);
					index       = *(p + 43);
					source      = p + 44;
					slen        = len - 44;
					break;
				}
				switch(*p) {
				case 0:
					printf("%sWordMemory("
					       "Resource%s, "
					       "%sDecode, "
					       "Min%sFixed, "
					       "Max%sFixed, "
					       "%s, "
					       "Read%s, "
					       "0x%llx, 0x%llx, 0x%llx, 0x%llx, 0x%llx, "
					       "%d, '%.*s', "
					       "AddressRange%s, "
					       "Type%s)\n",
					       size, 
					       *(p + 1) & 0x01 ? "Consumer" : "Producer",
					       *(p + 1) & 0x02 ? "Sub" : "Pos",
					       *(p + 1) & 0x04 ? "" : "Not",
					       *(p + 1) & 0x08 ? "" : "Not",
					       (*(p + 2) >> 1) == 0 ? "NonCacheable" :
					       (*(p + 2) >> 1) == 1 ? "Cacheable" :
					       (*(p + 2) >> 1) == 2 ? "WriteCombining" : 
					       "Prefetchable",
					       *(p + 2) & 0x01 ? "Write" : "Only",
					       granularity, minimum, maximum, translation, length,
					       index, slen, source,
					       ((*(p + 2) >> 3) & 0x03) == 0 ? "Memory" :
					       ((*(p + 2) >> 3) & 0x03) == 1 ? "Reserved" :
					       ((*(p + 2) >> 3) & 0x03) == 2 ? "ACPI" : "NVS",
					       *(p + 2) & 0x20 ? "Translation" : "Static");
					break;
				case 1:
					printf("%sWordIO("
					       "Resource%s, "
					       "Min%sFixed, "
					       "Max%sFixed, "
					       "%sDecode, "
					       "%s, "
					       "0x%llx, 0x%llx, 0x%llx, 0x%llx, 0x%llx, "
					       "%d, '%.*s', "
					       "Type%s, "
					       "%sTranslation)\n",
					       size, 
					       *(p + 1) & 0x01 ? "Consumer" : "Producer",
					       *(p + 1) & 0x04 ? "" : "Not",
					       *(p + 1) & 0x08 ? "" : "Not",
					       *(p + 1) & 0x02 ? "Sub" : "Pos",
					       (*(p + 2) & 0x03) == 0 ? "EntireRange" :
					       (*(p + 2) & 0x03) == 1 ? "NonISAOnlyRanges" :
					       (*(p + 2) & 0x03) == 2 ? "ISAOnlyRanges" : "EntireRange",
					       granularity, minimum, maximum, translation, length,
					       index, slen, source,
					       *(p + 2) & 0x10 ? "Translation" : "Static",
					       *(p + 2) & 0x20 ? "Sparse" : "Dense");
					break;
				case 2:
					printf("%sWordBus("
					       "Resource%s, "
					       "%sDecode, "
					       "Min%sFixed, "
					       "Max%sFixed, "
					       "0x%llx, 0x%llx, 0x%llx, 0x%llx, 0x%llx, "
						"%d, '%.*s')\n",
					       size, 
					       *(p + 1) & 0x01 ? "Consumer" : "Producer",
					       *(p + 1) & 0x02 ? "Sub" : "Pos",
					       *(p + 1) & 0x04 ? "" : "Not",
					       *(p + 1) & 0x08 ? "" : "Not",
					       granularity, minimum, maximum, translation, length,
					       index, slen, source);
					break;
				default:
					printf("%sWordUnknown()\n", size);
				}
				break;
			}
			case 0x89:	/* Interrupt() { } */
			{
				int i, first, pad, sl;
				char *rp;

				pad = *(p + 1) * 4;
				rp = p + 1 + pad;
				sl = len - pad - 3;
				printf("Interrupt(Resource%s, %s, Active%s, %s, %d, %.*s)",
				       *p & 0x01 ? "Producer" : "Consumer",
				       *p & 0x02 ? "Edge" : "Level",
				       *p & 0x04 ? "Low" : "High",
				       *p & 0x08 ? "Shared" : "Exclusive",
				       (int)*(p + 1 + pad),
				       sl,
				       rp);
				first = 1;
				for (i = 0; i < *(p + 1); i++) {
					if (first) {
						printf(" {");
						first = 0;
					} else {
						printf(", ");
					}
					printf("%u", *(u_int32_t *)(p + 2 + (i * 4)));
				}
				if (!first)
					printf("}");
				printf("\n");
				break;
			}
			default:
				printf("Unknown(0x%x, %d)\n", name, len);
				break;
			}
		}
		p += len;
	}
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
	start = dp;
	printf(") {");
	while (dp < end) {
		printf("0x%x", *dp++);
		if (dp < end)
			printf(", ");
	}
	if (rflag)
		asl_dump_resourcebuffer(start, end, indent);
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

int	scope_within_method = 0;

static void
asl_dump_defmethod(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int8_t	flags;
	u_int32_t	pkglength;
	struct	aml_name *oname;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);

	printf("Method(");
	ASL_ENTER_SCOPE(dp, oname);
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
	scope_within_method = 1;
	asl_dump_objectlist(&dp, end, indent + 1);
	scope_within_method = 0;
	print_indent(indent);
	printf("}");

	assert(dp == end);
	ASL_LEAVE_SCOPE(oname);
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
	case '/':
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
	struct	aml_name *oname;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	end = start + pkglength;

	printf("Device(");
	ASL_ENTER_SCOPE(dp, oname);
	asl_dump_termobj(&dp, indent);
	printf(") {\n");
	asl_dump_objectlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);

	ASL_LEAVE_SCOPE(oname);
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
	struct	aml_name *oname;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	end = start + pkglength;

	printf("Processor(");
	ASL_ENTER_SCOPE(dp, oname);
	asl_dump_termobj(&dp, indent);
	procid = asl_dump_bytedata(&dp);
	pblkaddr = asl_dump_dworddata(&dp);
	pblklen = asl_dump_bytedata(&dp);
	printf(", %d, 0x%x, 0x%x) {\n", procid, pblkaddr, pblklen);
	asl_dump_objectlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);

	ASL_LEAVE_SCOPE(oname);
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
	struct	aml_name *oname;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	end = start + pkglength;

	printf("PowerResource(");
	ASL_ENTER_SCOPE(dp, oname);
	asl_dump_termobj(&dp, indent);
	systemlevel = asl_dump_bytedata(&dp);
	resourceorder = asl_dump_worddata(&dp);
	printf(", %d, %d) {\n", systemlevel, resourceorder);
	asl_dump_objectlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);

	ASL_LEAVE_SCOPE(oname);
	*dpp = dp;
}

static void
asl_dump_defthermalzone(u_int8_t **dpp, int indent)
{
	u_int8_t	*dp;
	u_int8_t	*start;
	u_int8_t	*end;
	u_int32_t	pkglength;
	struct	aml_name *oname;

	dp = *dpp;
	start = dp;
	pkglength = asl_dump_pkglength(&dp);
	end = start + pkglength;

	printf("ThermalZone(");
	ASL_ENTER_SCOPE(dp, oname);
	asl_dump_termobj(&dp, indent);
	printf(") {\n");
	asl_dump_objectlist(&dp, end, indent + 1);
	print_indent(indent);
	printf("}");

	assert(dp == end);

	ASL_LEAVE_SCOPE(oname);
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
	u_int8_t	*name;
	u_int8_t	opcode;
	struct	aml_name *method;
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
	case '/':
		dp--;
		print_namestring((name = asl_dump_namestring(&dp)));
		if (scope_within_method == 1) {
			method = aml_search_name(&asl_env, name);
			if (method != NULL && method->property != NULL &&
			    method->property->type == aml_t_method) {
				int	i, argnum;

				argnum = method->property->meth.argnum & 7;
				printf("(");
				for (i = 0; i < argnum; i++) {
					asl_dump_termobj(&dp, indent);
					if (i < (argnum-1)) {
						printf(", ");
					}
				}
				printf(")");
			}
		}
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
	case 0x73:		/* ConcatenateOp */
		printf("Concatenate(");
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
		printf("SizeOf(");
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
