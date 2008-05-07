/*
 * Copyright (c) 1997 - 2005 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "gen_locl.h"

RCSID("$Id: gen.c 22429 2008-01-13 10:25:50Z lha $");

FILE *headerfile, *codefile, *logfile;

#define STEM "asn1"

static const char *orig_filename;
static char *header;
static const char *headerbase = STEM;

/*
 * list of all IMPORTs
 */

struct import {
    const char *module;
    struct import *next;
};

static struct import *imports = NULL;

void
add_import (const char *module)
{
    struct import *tmp = emalloc (sizeof(*tmp));

    tmp->module = module;
    tmp->next   = imports;
    imports     = tmp;

    fprintf (headerfile, "#include <%s_asn1.h>\n", module);
}

const char *
get_filename (void)
{
    return orig_filename;
}

void
init_generate (const char *filename, const char *base)
{
    char *fn;

    orig_filename = filename;
    if (base != NULL) {
	headerbase = strdup(base);
	if (headerbase == NULL)
	    errx(1, "strdup");
    }
    asprintf(&header, "%s.h", headerbase);
    if (header == NULL)
	errx(1, "malloc");
    headerfile = fopen (header, "w");
    if (headerfile == NULL)
	err (1, "open %s", header);
    fprintf (headerfile,
	     "/* Generated from %s */\n"
	     "/* Do not edit */\n\n",
	     filename);
    fprintf (headerfile, 
	     "#ifndef __%s_h__\n"
	     "#define __%s_h__\n\n", headerbase, headerbase);
    fprintf (headerfile, 
	     "#include <stddef.h>\n"
	     "#include <time.h>\n\n");
    fprintf (headerfile,
	     "#ifndef __asn1_common_definitions__\n"
	     "#define __asn1_common_definitions__\n\n");
    fprintf (headerfile,
	     "typedef struct heim_integer {\n"
	     "  size_t length;\n"
	     "  void *data;\n"
	     "  int negative;\n"
	     "} heim_integer;\n\n");
    fprintf (headerfile,
	     "typedef struct heim_octet_string {\n"
	     "  size_t length;\n"
	     "  void *data;\n"
	     "} heim_octet_string;\n\n");
    fprintf (headerfile,
	     "typedef char *heim_general_string;\n\n"
	     );
    fprintf (headerfile,
	     "typedef char *heim_utf8_string;\n\n"
	     );
    fprintf (headerfile,
	     "typedef char *heim_printable_string;\n\n"
	     );
    fprintf (headerfile,
	     "typedef char *heim_ia5_string;\n\n"
	     );
    fprintf (headerfile,
	     "typedef struct heim_bmp_string {\n"
	     "  size_t length;\n"
	     "  uint16_t *data;\n"
	     "} heim_bmp_string;\n\n");
    fprintf (headerfile,
	     "typedef struct heim_universal_string {\n"
	     "  size_t length;\n"
	     "  uint32_t *data;\n"
	     "} heim_universal_string;\n\n");
    fprintf (headerfile,
	     "typedef char *heim_visible_string;\n\n"
	     );
    fprintf (headerfile,
	     "typedef struct heim_oid {\n"
	     "  size_t length;\n"
	     "  unsigned *components;\n"
	     "} heim_oid;\n\n");
    fprintf (headerfile,
	     "typedef struct heim_bit_string {\n"
	     "  size_t length;\n"
	     "  void *data;\n"
	     "} heim_bit_string;\n\n");
    fprintf (headerfile,
	     "typedef struct heim_octet_string heim_any;\n"
	     "typedef struct heim_octet_string heim_any_set;\n\n");
    fputs("#define ASN1_MALLOC_ENCODE(T, B, BL, S, L, R)                  \\\n"
	  "  do {                                                         \\\n"
	  "    (BL) = length_##T((S));                                    \\\n"
	  "    (B) = malloc((BL));                                        \\\n"
	  "    if((B) == NULL) {                                          \\\n"
	  "      (R) = ENOMEM;                                            \\\n"
	  "    } else {                                                   \\\n"
	  "      (R) = encode_##T(((unsigned char*)(B)) + (BL) - 1, (BL), \\\n"
	  "                       (S), (L));                              \\\n"
	  "      if((R) != 0) {                                           \\\n"
	  "        free((B));                                             \\\n"
	  "        (B) = NULL;                                            \\\n"
	  "      }                                                        \\\n"
	  "    }                                                          \\\n"
	  "  } while (0)\n\n",
	  headerfile);
    fprintf (headerfile, "struct units;\n\n");
    fprintf (headerfile, "#endif\n\n");
    asprintf(&fn, "%s_files", base);
    if (fn == NULL)
	errx(1, "malloc");
    logfile = fopen(fn, "w");
    if (logfile == NULL)
	err (1, "open %s", fn);
}

void
close_generate (void)
{
    fprintf (headerfile, "#endif /* __%s_h__ */\n", headerbase);

    fclose (headerfile);
    fprintf (logfile, "\n");
    fclose (logfile);
}

void
gen_assign_defval(const char *var, struct value *val)
{
    switch(val->type) {
    case stringvalue:
	fprintf(codefile, "if((%s = strdup(\"%s\")) == NULL)\nreturn ENOMEM;\n", var, val->u.stringvalue);
	break;
    case integervalue:
	fprintf(codefile, "%s = %d;\n", var, val->u.integervalue);
	break;
    case booleanvalue:
	if(val->u.booleanvalue)
	    fprintf(codefile, "%s = TRUE;\n", var);
	else
	    fprintf(codefile, "%s = FALSE;\n", var);
	break;
    default:
	abort();
    }
}

void
gen_compare_defval(const char *var, struct value *val)
{
    switch(val->type) {
    case stringvalue:
	fprintf(codefile, "if(strcmp(%s, \"%s\") != 0)\n", var, val->u.stringvalue);
	break;
    case integervalue:
	fprintf(codefile, "if(%s != %d)\n", var, val->u.integervalue);
	break;
    case booleanvalue:
	if(val->u.booleanvalue)
	    fprintf(codefile, "if(!%s)\n", var);
	else
	    fprintf(codefile, "if(%s)\n", var);
	break;
    default:
	abort();
    }
}

static void
generate_header_of_codefile(const char *name)
{
    char *filename;

    if (codefile != NULL)
	abort();

    asprintf (&filename, "%s_%s.x", STEM, name);
    if (filename == NULL)
	errx(1, "malloc");
    codefile = fopen (filename, "w");
    if (codefile == NULL)
	err (1, "fopen %s", filename);
    fprintf(logfile, "%s ", filename);
    free(filename);
    fprintf (codefile, 
	     "/* Generated from %s */\n"
	     "/* Do not edit */\n\n"
	     "#include <stdio.h>\n"
	     "#include <stdlib.h>\n"
	     "#include <time.h>\n"
	     "#include <string.h>\n"
	     "#include <errno.h>\n"
	     "#include <limits.h>\n"
	     "#include <krb5-types.h>\n",
	     orig_filename);

    fprintf (codefile,
	     "#include <%s.h>\n",
	     headerbase);
    fprintf (codefile,
	     "#include <asn1_err.h>\n"
	     "#include <der.h>\n"
	     "#include <parse_units.h>\n\n");

}

static void
close_codefile(void)
{
    if (codefile == NULL)
	abort();

    fclose(codefile);
    codefile = NULL;
}


void
generate_constant (const Symbol *s)
{
    switch(s->value->type) {
    case booleanvalue:
	break;
    case integervalue:
	fprintf (headerfile, "enum { %s = %d };\n\n",
		 s->gen_name, s->value->u.integervalue);
	break;
    case nullvalue:
	break;
    case stringvalue:
	break;
    case objectidentifiervalue: {
	struct objid *o, **list;
	int i, len;

	generate_header_of_codefile(s->gen_name);

	len = 0;
	for (o = s->value->u.objectidentifiervalue; o != NULL; o = o->next)
	    len++;
	list = emalloc(sizeof(*list) * len);

	i = 0;
	for (o = s->value->u.objectidentifiervalue; o != NULL; o = o->next)
	    list[i++] = o;

	fprintf (headerfile, "/* OBJECT IDENTIFIER %s ::= { ", s->name);
	for (i = len - 1 ; i >= 0; i--) {
	    o = list[i];
	    fprintf(headerfile, "%s(%d) ",
		    o->label ? o->label : "label-less", o->value);
	}

	fprintf (headerfile, "} */\n");
	fprintf (headerfile, "const heim_oid *oid_%s(void);\n\n",
		 s->gen_name);

	fprintf (codefile, "static unsigned oid_%s_variable_num[%d] =  {",
		 s->gen_name, len);
	for (i = len - 1 ; i >= 0; i--) {
	    fprintf(codefile, "%d%s ", list[i]->value, i > 0 ? "," : "");
	}
	fprintf(codefile, "};\n");

	fprintf (codefile, "static const heim_oid oid_%s_variable = "
		 "{ %d, oid_%s_variable_num };\n\n", 
		 s->gen_name, len, s->gen_name);

	fprintf (codefile, "const heim_oid *oid_%s(void)\n"
		 "{\n"
		 "return &oid_%s_variable;\n"
		 "}\n\n",
		 s->gen_name, s->gen_name);

	close_codefile();

	break;
    }
    default:
	abort();
    }
}

static void
space(int level)
{
    while(level-- > 0)
	fprintf(headerfile, "  ");
}

static const char *
last_member_p(struct member *m)
{
    struct member *n = ASN1_TAILQ_NEXT(m, members);
    if (n == NULL)
	return "";
    if (n->ellipsis && ASN1_TAILQ_NEXT(n, members) == NULL)
	return "";
    return ",";
}

static struct member *
have_ellipsis(Type *t)
{
    struct member *m;
    ASN1_TAILQ_FOREACH(m, t->members, members) {
	if (m->ellipsis)
	    return m;
    }
    return NULL;
}

static void
define_asn1 (int level, Type *t)
{
    switch (t->type) {
    case TType:
	fprintf (headerfile, "%s", t->symbol->name);
	break;
    case TInteger:
	if(t->members == NULL) {
            fprintf (headerfile, "INTEGER");
	    if (t->range)
		fprintf (headerfile, " (%d..%d)",
			 t->range->min, t->range->max);
        } else {
	    Member *m;
            fprintf (headerfile, "INTEGER {\n");
	    ASN1_TAILQ_FOREACH(m, t->members, members) {
                space (level + 1);
		fprintf(headerfile, "%s(%d)%s\n", m->gen_name, m->val, 
			last_member_p(m));
            }
	    space(level);
            fprintf (headerfile, "}");
        }
	break;
    case TBoolean:
	fprintf (headerfile, "BOOLEAN");
	break;
    case TOctetString:
	fprintf (headerfile, "OCTET STRING");
	break;
    case TEnumerated :
    case TBitString: {
	Member *m;

	space(level);
	if(t->type == TBitString)
	    fprintf (headerfile, "BIT STRING {\n");
	else
	    fprintf (headerfile, "ENUMERATED {\n");
	ASN1_TAILQ_FOREACH(m, t->members, members) {
	    space(level + 1);
	    fprintf (headerfile, "%s(%d)%s\n", m->name, m->val, 
		     last_member_p(m));
	}
	space(level);
	fprintf (headerfile, "}");
	break;
    }
    case TChoice:
    case TSet:
    case TSequence: {
	Member *m;
	int max_width = 0;

	if(t->type == TChoice)
	    fprintf(headerfile, "CHOICE {\n");
	else if(t->type == TSet)
	    fprintf(headerfile, "SET {\n");
	else
	    fprintf(headerfile, "SEQUENCE {\n");
	ASN1_TAILQ_FOREACH(m, t->members, members) {
	    if(strlen(m->name) > max_width)
		max_width = strlen(m->name);
	}
	max_width += 3;
	if(max_width < 16) max_width = 16;
	ASN1_TAILQ_FOREACH(m, t->members, members) {
	    int width = max_width;
	    space(level + 1);
	    if (m->ellipsis) {
		fprintf (headerfile, "...");
	    } else {
		width -= fprintf(headerfile, "%s", m->name);
		fprintf(headerfile, "%*s", width, "");
		define_asn1(level + 1, m->type);
		if(m->optional)
		    fprintf(headerfile, " OPTIONAL");
	    }
	    if(last_member_p(m))
		fprintf (headerfile, ",");
	    fprintf (headerfile, "\n");
	}
	space(level);
	fprintf (headerfile, "}");
	break;
    }
    case TSequenceOf:
	fprintf (headerfile, "SEQUENCE OF ");
	define_asn1 (0, t->subtype);
	break;
    case TSetOf:
	fprintf (headerfile, "SET OF ");
	define_asn1 (0, t->subtype);
	break;
    case TGeneralizedTime:
	fprintf (headerfile, "GeneralizedTime");
	break;
    case TGeneralString:
	fprintf (headerfile, "GeneralString");
	break;
    case TTag: {
	const char *classnames[] = { "UNIVERSAL ", "APPLICATION ", 
				     "" /* CONTEXT */, "PRIVATE " };
	if(t->tag.tagclass != ASN1_C_UNIV)
	    fprintf (headerfile, "[%s%d] ", 
		     classnames[t->tag.tagclass],
		     t->tag.tagvalue);
	if(t->tag.tagenv == TE_IMPLICIT)
	    fprintf (headerfile, "IMPLICIT ");
	define_asn1 (level, t->subtype);
	break;
    }
    case TUTCTime:
	fprintf (headerfile, "UTCTime");
	break;
    case TUTF8String:
	space(level);
	fprintf (headerfile, "UTF8String");
	break;
    case TPrintableString:
	space(level);
	fprintf (headerfile, "PrintableString");
	break;
    case TIA5String:
	space(level);
	fprintf (headerfile, "IA5String");
	break;
    case TBMPString:
	space(level);
	fprintf (headerfile, "BMPString");
	break;
    case TUniversalString:
	space(level);
	fprintf (headerfile, "UniversalString");
	break;
    case TVisibleString:
	space(level);
	fprintf (headerfile, "VisibleString");
	break;
    case TOID :
	space(level);
	fprintf(headerfile, "OBJECT IDENTIFIER");
	break;
    case TNull:
	space(level);
	fprintf (headerfile, "NULL");
	break;
    default:
	abort ();
    }
}

static void
define_type (int level, const char *name, Type *t, int typedefp, int preservep)
{
    switch (t->type) {
    case TType:
	space(level);
	fprintf (headerfile, "%s %s;\n", t->symbol->gen_name, name);
	break;
    case TInteger:
	space(level);
	if(t->members) {
            Member *m;
            fprintf (headerfile, "enum %s {\n", typedefp ? name : "");
	    ASN1_TAILQ_FOREACH(m, t->members, members) {
                space (level + 1);
                fprintf(headerfile, "%s = %d%s\n", m->gen_name, m->val, 
                        last_member_p(m));
            }
            fprintf (headerfile, "} %s;\n", name);
	} else if (t->range == NULL) {
	    fprintf (headerfile, "heim_integer %s;\n", name);
	} else if (t->range->min == INT_MIN && t->range->max == INT_MAX) {
	    fprintf (headerfile, "int %s;\n", name);
	} else if (t->range->min == 0 && t->range->max == UINT_MAX) {
	    fprintf (headerfile, "unsigned int %s;\n", name);
	} else if (t->range->min == 0 && t->range->max == INT_MAX) {
	    fprintf (headerfile, "unsigned int %s;\n", name);
	} else
	    errx(1, "%s: unsupported range %d -> %d", 
		 name, t->range->min, t->range->max);
	break;
    case TBoolean:
	space(level);
	fprintf (headerfile, "int %s;\n", name);
	break;
    case TOctetString:
	space(level);
	fprintf (headerfile, "heim_octet_string %s;\n", name);
	break;
    case TBitString: {
	Member *m;
	Type i;
	struct range range = { 0, INT_MAX };

	i.type = TInteger;
	i.range = &range;
	i.members = NULL;
	i.constraint = NULL;

	space(level);
	if(ASN1_TAILQ_EMPTY(t->members)) 
	    fprintf (headerfile, "heim_bit_string %s;\n", name);
	else {
	    fprintf (headerfile, "struct %s {\n", typedefp ? name : "");
	    ASN1_TAILQ_FOREACH(m, t->members, members) {
		char *n;
		
		asprintf (&n, "%s:1", m->gen_name);
		if (n == NULL)
		    errx(1, "malloc");
		define_type (level + 1, n, &i, FALSE, FALSE);
		free (n);
	    }
	    space(level);
	    fprintf (headerfile, "} %s;\n\n", name);
	}
	break;
    }
    case TEnumerated: {
	Member *m;

	space(level);
	fprintf (headerfile, "enum %s {\n", typedefp ? name : "");
	ASN1_TAILQ_FOREACH(m, t->members, members) {
	    space(level + 1);
	    if (m->ellipsis)
		fprintf (headerfile, "/* ... */\n");
	    else
		fprintf (headerfile, "%s = %d%s\n", m->gen_name, m->val,
			 last_member_p(m));
	}
	space(level);
	fprintf (headerfile, "} %s;\n\n", name);
	break;
    }
    case TSet:
    case TSequence: {
	Member *m;

	space(level);
	fprintf (headerfile, "struct %s {\n", typedefp ? name : "");
	if (t->type == TSequence && preservep) {
	    space(level + 1);
	    fprintf(headerfile, "heim_octet_string _save;\n");
	}
	ASN1_TAILQ_FOREACH(m, t->members, members) {
	    if (m->ellipsis) {
		;
	    } else if (m->optional) {
		char *n;

		asprintf (&n, "*%s", m->gen_name);
		if (n == NULL)
		    errx(1, "malloc");
		define_type (level + 1, n, m->type, FALSE, FALSE);
		free (n);
	    } else
		define_type (level + 1, m->gen_name, m->type, FALSE, FALSE);
	}
	space(level);
	fprintf (headerfile, "} %s;\n", name);
	break;
    }
    case TSetOf:
    case TSequenceOf: {
	Type i;
	struct range range = { 0, INT_MAX };

	i.type = TInteger;
	i.range = &range;
	i.members = NULL;
	i.constraint = NULL;

	space(level);
	fprintf (headerfile, "struct %s {\n", typedefp ? name : "");
	define_type (level + 1, "len", &i, FALSE, FALSE);
	define_type (level + 1, "*val", t->subtype, FALSE, FALSE);
	space(level);
	fprintf (headerfile, "} %s;\n", name);
	break;
    }
    case TGeneralizedTime:
	space(level);
	fprintf (headerfile, "time_t %s;\n", name);
	break;
    case TGeneralString:
	space(level);
	fprintf (headerfile, "heim_general_string %s;\n", name);
	break;
    case TTag:
	define_type (level, name, t->subtype, typedefp, preservep);
	break;
    case TChoice: {
	int first = 1;
	Member *m;

	space(level);
	fprintf (headerfile, "struct %s {\n", typedefp ? name : "");
	if (preservep) {
	    space(level + 1);
	    fprintf(headerfile, "heim_octet_string _save;\n");
	}
	space(level + 1);
	fprintf (headerfile, "enum {\n");
	m = have_ellipsis(t);
	if (m) {
	    space(level + 2);
	    fprintf (headerfile, "%s = 0,\n", m->label); 
	    first = 0;
	}
	ASN1_TAILQ_FOREACH(m, t->members, members) {
	    space(level + 2);
	    if (m->ellipsis)
		fprintf (headerfile, "/* ... */\n");
	    else
		fprintf (headerfile, "%s%s%s\n", m->label, 
			 first ? " = 1" : "", 
			 last_member_p(m));
	    first = 0;
	}
	space(level + 1);
	fprintf (headerfile, "} element;\n");
	space(level + 1);
	fprintf (headerfile, "union {\n");
	ASN1_TAILQ_FOREACH(m, t->members, members) {
	    if (m->ellipsis) {
		space(level + 2);
		fprintf(headerfile, "heim_octet_string asn1_ellipsis;\n");
	    } else if (m->optional) {
		char *n;

		asprintf (&n, "*%s", m->gen_name);
		if (n == NULL)
		    errx(1, "malloc");
		define_type (level + 2, n, m->type, FALSE, FALSE);
		free (n);
	    } else
		define_type (level + 2, m->gen_name, m->type, FALSE, FALSE);
	}
	space(level + 1);
	fprintf (headerfile, "} u;\n");
	space(level);
	fprintf (headerfile, "} %s;\n", name);
	break;
    }
    case TUTCTime:
	space(level);
	fprintf (headerfile, "time_t %s;\n", name);
	break;
    case TUTF8String:
	space(level);
	fprintf (headerfile, "heim_utf8_string %s;\n", name);
	break;
    case TPrintableString:
	space(level);
	fprintf (headerfile, "heim_printable_string %s;\n", name);
	break;
    case TIA5String:
	space(level);
	fprintf (headerfile, "heim_ia5_string %s;\n", name);
	break;
    case TBMPString:
	space(level);
	fprintf (headerfile, "heim_bmp_string %s;\n", name);
	break;
    case TUniversalString:
	space(level);
	fprintf (headerfile, "heim_universal_string %s;\n", name);
	break;
    case TVisibleString:
	space(level);
	fprintf (headerfile, "heim_visible_string %s;\n", name);
	break;
    case TOID :
	space(level);
	fprintf (headerfile, "heim_oid %s;\n", name);
	break;
    case TNull:
	space(level);
	fprintf (headerfile, "int %s;\n", name);
	break;
    default:
	abort ();
    }
}

static void
generate_type_header (const Symbol *s)
{
    int preservep = preserve_type(s->name) ? TRUE : FALSE;

    fprintf (headerfile, "/*\n");
    fprintf (headerfile, "%s ::= ", s->name);
    define_asn1 (0, s->type);
    fprintf (headerfile, "\n*/\n\n");

    fprintf (headerfile, "typedef ");
    define_type (0, s->gen_name, s->type, TRUE, preservep);

    fprintf (headerfile, "\n");
}


void
generate_type (const Symbol *s)
{
    generate_header_of_codefile(s->gen_name);

    generate_type_header (s);
    generate_type_encode (s);
    generate_type_decode (s);
    generate_type_free (s);
    generate_type_length (s);
    generate_type_copy (s);
    generate_type_seq (s);
    generate_glue (s->type, s->gen_name);
    fprintf(headerfile, "\n\n");
    close_codefile();
}
