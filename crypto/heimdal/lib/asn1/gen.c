/*
 * Copyright (c) 1997 - 2000 Kungliga Tekniska Högskolan
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

RCSID("$Id: gen.c,v 1.44 2000/06/19 15:17:52 joda Exp $");

FILE *headerfile, *codefile, *logfile;

#define STEM "asn1"

static const char *orig_filename;
static char header[1024];
static char headerbase[1024] = STEM;

const char *
filename (void)
{
    return orig_filename;
}

void
init_generate (const char *filename, const char *base)
{
    orig_filename = filename;
    if(base)
	strcpy(headerbase, base);
    sprintf(header, "%s.h", headerbase);
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
#ifndef HAVE_TIMEGM
    fprintf (headerfile, "time_t timegm (struct tm*);\n\n");
#endif
    fprintf (headerfile,
	     "#ifndef __asn1_common_definitions__\n"
	     "#define __asn1_common_definitions__\n\n");
    fprintf (headerfile,
	     "typedef struct octet_string {\n"
	     "  size_t length;\n"
	     "  void *data;\n"
	     "} octet_string;\n\n");
    fprintf (headerfile,
#if 0
	     "typedef struct general_string {\n"
	     "  size_t length;\n"
	     "  char *data;\n"
	     "} general_string;\n\n"
#else
	     "typedef char *general_string;\n\n"
#endif
	     );
    fprintf (headerfile, "#endif\n\n");
    logfile = fopen(STEM "_files", "w");
    if (logfile == NULL)
	err (1, "open " STEM "_files");
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
generate_constant (const Symbol *s)
{
  fprintf (headerfile, "enum { %s = %d };\n\n",
	   s->gen_name, s->constant);
}

static void
space(int level)
{
    while(level-- > 0)
	fprintf(headerfile, "  ");
}

static void
define_asn1 (int level, Type *t)
{
    switch (t->type) {
    case TType:
	space(level);
	fprintf (headerfile, "%s", t->symbol->name);
	break;
    case TInteger:
	space(level);
	fprintf (headerfile, "INTEGER");
	break;
    case TUInteger:
	space(level);
	fprintf (headerfile, "UNSIGNED INTEGER");
	break;
    case TOctetString:
	space(level);
	fprintf (headerfile, "OCTET STRING");
	break;
    case TBitString: {
	Member *m;
	Type i;
	int tag = -1;

	i.type = TInteger;
	space(level);
	fprintf (headerfile, "BIT STRING {\n");
	for (m = t->members; m && m->val != tag; m = m->next) {
	    if (tag == -1)
		tag = m->val;
	    space(level + 1);
	    fprintf (headerfile, "%s(%d)%s\n", m->name, m->val, 
		     m->next->val == tag?"":",");

	}
	space(level);
	fprintf (headerfile, "}");
	break;
    }
    case TSequence: {
	Member *m;
	int tag;
	int max_width = 0;

	space(level);
	fprintf (headerfile, "SEQUENCE {\n");
	for (m = t->members, tag = -1; m && m->val != tag; m = m->next) {
	    if (tag == -1)
		tag = m->val;
	    if(strlen(m->name) + (m->val > 9) > max_width)
		max_width = strlen(m->name) + (m->val > 9);
	}
	max_width += 3 + 2;
	if(max_width < 16) max_width = 16;
	for (m = t->members, tag = -1 ; m && m->val != tag; m = m->next) {
	    int width;
	    if (tag == -1)
		tag = m->val;
	    space(level + 1);
	    fprintf(headerfile, "%s[%d]", m->name, m->val);
	    width = max_width - strlen(m->name) - 3 - (m->val > 9) - 2;
	    fprintf(headerfile, "%*s", width, "");
	    define_asn1(level + 1, m->type);
	    if(m->optional)
		fprintf(headerfile, " OPTIONAL");
	    if(m->next->val != tag)
		fprintf (headerfile, ",");
	    fprintf (headerfile, "\n");
	}
	space(level);
	fprintf (headerfile, "}");
	break;
    }
    case TSequenceOf: {
	space(level);
	fprintf (headerfile, "SEQUENCE OF ");
	define_asn1 (0, t->subtype);
	break;
    }
    case TGeneralizedTime:
	space(level);
	fprintf (headerfile, "GeneralizedTime");
	break;
    case TGeneralString:
	space(level);
	fprintf (headerfile, "GeneralString");
	break;
    case TApplication:
	fprintf (headerfile, "[APPLICATION %d] ", t->application);
	define_asn1 (level, t->subtype);
	break;
    default:
	abort ();
    }
}

static void
define_type (int level, char *name, Type *t, int typedefp)
{
    switch (t->type) {
    case TType:
	space(level);
	fprintf (headerfile, "%s %s;\n", t->symbol->gen_name, name);
	break;
    case TInteger:
	space(level);
        if(t->members == NULL) {
            fprintf (headerfile, "int %s;\n", name);
        } else {
            Member *m;
            int tag = -1;
            fprintf (headerfile, "enum %s {\n", typedefp ? name : "");
	    for (m = t->members; m && m->val != tag; m = m->next) {
                if(tag == -1)
                    tag = m->val;
                space (level + 1);
                fprintf(headerfile, "%s = %d%s\n", m->gen_name, m->val, 
                        m->next->val == tag ? "" : ",");
            }
            fprintf (headerfile, "} %s;\n", name);
        }
	break;
    case TUInteger:
	space(level);
	fprintf (headerfile, "unsigned int %s;\n", name);
	break;
    case TOctetString:
	space(level);
	fprintf (headerfile, "octet_string %s;\n", name);
	break;
    case TBitString: {
	Member *m;
	Type i;
	int tag = -1;

	i.type = TUInteger;
	space(level);
	fprintf (headerfile, "struct %s {\n", typedefp ? name : "");
	for (m = t->members; m && m->val != tag; m = m->next) {
	    char *n;

	    asprintf (&n, "%s:1", m->gen_name);
	    define_type (level + 1, n, &i, FALSE);
	    free (n);
	    if (tag == -1)
		tag = m->val;
	}
	space(level);
	fprintf (headerfile, "} %s;\n\n", name);
	break;
    }
    case TSequence: {
	Member *m;
	int tag = -1;

	space(level);
	fprintf (headerfile, "struct %s {\n", typedefp ? name : "");
	for (m = t->members; m && m->val != tag; m = m->next) {
	    if (m->optional) {
		char *n;

		asprintf (&n, "*%s", m->gen_name);
		define_type (level + 1, n, m->type, FALSE);
		free (n);
	    } else
		define_type (level + 1, m->gen_name, m->type, FALSE);
	    if (tag == -1)
		tag = m->val;
	}
	space(level);
	fprintf (headerfile, "} %s;\n", name);
	break;
    }
    case TSequenceOf: {
	Type i;

	i.type = TUInteger;
	i.application = 0;

	space(level);
	fprintf (headerfile, "struct %s {\n", typedefp ? name : "");
	define_type (level + 1, "len", &i, FALSE);
	define_type (level + 1, "*val", t->subtype, FALSE);
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
	fprintf (headerfile, "general_string %s;\n", name);
	break;
    case TApplication:
	define_type (level, name, t->subtype, FALSE);
	break;
    default:
	abort ();
    }
}

static void
generate_type_header (const Symbol *s)
{
    fprintf (headerfile, "/*\n");
    fprintf (headerfile, "%s ::= ", s->name);
    define_asn1 (0, s->type);
    fprintf (headerfile, "\n*/\n\n");

    fprintf (headerfile, "typedef ");
    define_type (0, s->gen_name, s->type, TRUE);

    fprintf (headerfile, "\n");
}


void
generate_type (const Symbol *s)
{
    char *filename;

    asprintf (&filename, "%s_%s.x", STEM, s->gen_name);
    codefile = fopen (filename, "w");
    if (codefile == NULL)
	err (1, "fopen %s", filename);
    fprintf(logfile, "%s ", filename);
    free(filename);
    fprintf (codefile, 
	     "/* Generated from %s */\n"
	     "/* Do not edit */\n\n"
	     "#include \"libasn1.h\"\n\n"
#if 0
	     "#include <stdio.h>\n"
	     "#include <stdlib.h>\n"
	     "#include <time.h>\n"
	     "#include <" STEM ".h>\n\n"
	     "#include <asn1_err.h>\n"
	     "#include <der.h>\n"
#endif
	     ,orig_filename);
    generate_type_header (s);
    generate_type_encode (s);
    generate_type_decode (s);
    generate_type_free (s);
    generate_type_length (s);
    generate_type_copy (s);
    generate_glue (s);
    fprintf(headerfile, "\n\n");
    fclose(codefile);
}
