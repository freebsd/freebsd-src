/*
 * Copyright (c) 1997, 1999 Kungliga Tekniska Högskolan
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

RCSID("$Id: gen_glue.c,v 1.7 1999/12/02 17:05:02 joda Exp $");

static void
generate_2int (const Symbol *s)
{
    Type *t = s->type;
    Member *m;
    int tag = -1;

    fprintf (headerfile,
	     "unsigned %s2int(%s);\n",
	     s->gen_name, s->gen_name);

    fprintf (codefile,
	     "unsigned %s2int(%s f)\n"
	     "{\n"
	     "unsigned r = 0;\n",
	     s->gen_name, s->gen_name);

    for (m = t->members; m && m->val != tag; m = m->next) {
	fprintf (codefile, "if(f.%s) r |= (1U << %d);\n",
		 m->gen_name, m->val);
	
	if (tag == -1)
	    tag = m->val;
    }
    fprintf (codefile, "return r;\n"
	     "}\n\n");
}

static void
generate_int2 (const Symbol *s)
{
    Type *t = s->type;
    Member *m;
    int tag = -1;

    fprintf (headerfile,
	     "%s int2%s(unsigned);\n",
	     s->gen_name, s->gen_name);

    fprintf (codefile,
	     "%s int2%s(unsigned n)\n"
	     "{\n"
	     "\t%s flags;\n\n",
	     s->gen_name, s->gen_name, s->gen_name);

    for (m = t->members; m && m->val != tag; m = m->next) {
	fprintf (codefile, "\tflags.%s = (n >> %d) & 1;\n",
		 m->gen_name, m->val);
	
	if (tag == -1)
	    tag = m->val;
    }
    fprintf (codefile, "\treturn flags;\n"
	     "}\n\n");
}

/*
 * This depends on the bit string being declared in increasing order
 */

static void
generate_units (const Symbol *s)
{
    Type *t = s->type;
    Member *m;
    int tag = -1;

    fprintf (headerfile,
	     "extern struct units %s_units[];",
	     s->gen_name);

    fprintf (codefile,
	     "struct units %s_units[] = {\n",
	     s->gen_name);

    if(t->members)
	for (m = t->members->prev; m && m->val != tag; m = m->prev) {
	    fprintf (codefile,
		     "\t{\"%s\",\t1U << %d},\n", m->gen_name, m->val);
	    
	    if (tag == -1)
		tag = m->val;
	}

    fprintf (codefile,
	     "\t{NULL,\t0}\n"
	     "};\n\n");
}

void
generate_glue (const Symbol *s)
{
    switch(s->type->type) {
    case TBitString :
	generate_2int (s);
	generate_int2 (s);
	generate_units (s);
	break;
    default :
	break;
    }
}
