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

RCSID("$Id: gen_length.c,v 1.11 2001/09/25 13:39:26 assar Exp $");

static void
length_primitive (const char *typename,
		  const char *name,
		  const char *variable)
{
    fprintf (codefile, "%s += length_%s(%s);\n", variable, typename, name);
}

static void
length_type (const char *name, const Type *t, const char *variable)
{
    switch (t->type) {
    case TType:
#if 0
	length_type (name, t->symbol->type);
#endif
	fprintf (codefile, "%s += length_%s(%s);\n",
		 variable, t->symbol->gen_name, name);
	break;
    case TInteger:
        if(t->members == NULL)
            length_primitive ("integer", name, variable);
        else {
            char *s;
            asprintf(&s, "(const int*)%s", name);
            if(s == NULL)
		errx (1, "out of memory");
            length_primitive ("integer", s, variable);
            free(s);
        }
	break;
    case TUInteger:
	length_primitive ("unsigned", name, variable);
	break;
    case TEnumerated :
	length_primitive ("enumerated", name, variable);
	break;
    case TOctetString:
	length_primitive ("octet_string", name, variable);
	break;
    case TOID :
	length_primitive ("oid", name, variable);
	break;
    case TBitString: {
	/*
	 * XXX - Hope this is correct
	 * look at TBitString case in `encode_type'
	 */
	fprintf (codefile, "%s += 7;\n", variable);
	break;
    }
    case TSequence: {
	Member *m;
	int tag = -1;

	if (t->members == NULL)
	    break;
      
	for (m = t->members; m && tag != m->val; m = m->next) {
	    char *s;

	    asprintf (&s, "%s(%s)->%s",
		      m->optional ? "" : "&", name, m->gen_name);
	    if (m->optional)
		fprintf (codefile, "if(%s)", s);
	    fprintf (codefile, "{\n"
		     "int oldret = %s;\n"
		     "%s = 0;\n", variable, variable);
	    length_type (s, m->type, "ret");
	    fprintf (codefile, "%s += 1 + length_len(%s) + oldret;\n",
		     variable, variable);
	    fprintf (codefile, "}\n");
	    if (tag == -1)
		tag = m->val;
	    free (s);
	}
	fprintf (codefile,
		 "%s += 1 + length_len(%s);\n", variable, variable);
	break;
    }
    case TSequenceOf: {
	char *n;

	fprintf (codefile,
		 "{\n"
		 "int oldret = %s;\n"
		 "int i;\n"
		 "%s = 0;\n",
		 variable, variable);

	fprintf (codefile, "for(i = (%s)->len - 1; i >= 0; --i){\n", name);
	asprintf (&n, "&(%s)->val[i]", name);
	length_type(n, t->subtype, variable);
	fprintf (codefile, "}\n");

	fprintf (codefile,
		 "%s += 1 + length_len(%s) + oldret;\n"
		 "}\n", variable, variable);
	free(n);
	break;
    }
    case TGeneralizedTime:
	length_primitive ("generalized_time", name, variable);
	break;
    case TGeneralString:
	length_primitive ("general_string", name, variable);
	break;
    case TApplication:
	length_type (name, t->subtype, variable);
	fprintf (codefile, "ret += 1 + length_len (ret);\n");
	break;
    default :
	abort ();
    }
}

void
generate_type_length (const Symbol *s)
{
  fprintf (headerfile,
	   "size_t length_%s(const %s *);\n",
	   s->gen_name, s->gen_name);

  fprintf (codefile,
	   "size_t\n"
	   "length_%s(const %s *data)\n"
	   "{\n"
	   "size_t ret = 0;\n",
	   s->gen_name, s->gen_name);

  length_type ("data", s->type, "ret");
  fprintf (codefile, "return ret;\n}\n\n");
}

