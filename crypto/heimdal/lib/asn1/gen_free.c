/*
 * Copyright (c) 1997 - 2003 Kungliga Tekniska Högskolan
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

RCSID("$Id: gen_free.c,v 1.9.6.1 2003/08/20 16:25:01 joda Exp $");

static void
free_primitive (const char *typename, const char *name)
{
    fprintf (codefile, "free_%s(%s);\n", typename, name);
}

static void
free_type (const char *name, const Type *t)
{
  switch (t->type) {
  case TType:
#if 0
      free_type (name, t->symbol->type);
#endif
      fprintf (codefile, "free_%s(%s);\n", t->symbol->gen_name, name);
      break;
  case TInteger:
  case TUInteger:
  case TEnumerated :
      break;
  case TOctetString:
      free_primitive ("octet_string", name);
      break;
  case TOID :
      free_primitive ("oid", name);
      break;
  case TBitString: {
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
	  if(m->optional)
	      fprintf(codefile, "if(%s) {\n", s);
	  free_type (s, m->type);
	  if(m->optional)
	      fprintf(codefile, 
		      "free(%s);\n"
		      "%s = NULL;\n"
		      "}\n", s, s);
	  if (tag == -1)
	      tag = m->val;
	  free (s);
      }
      break;
  }
  case TSequenceOf: {
      char *n;

      fprintf (codefile, "while((%s)->len){\n", name);
      asprintf (&n, "&(%s)->val[(%s)->len-1]", name, name);
      free_type(n, t->subtype);
      fprintf(codefile, 
	      "(%s)->len--;\n"
	      "}\n",
	      name);
      fprintf(codefile,
	      "free((%s)->val);\n"
	      "(%s)->val = NULL;\n", name, name);
      free(n);
      break;
  }
  case TGeneralizedTime:
      break;
  case TGeneralString:
      free_primitive ("general_string", name);
      break;
  case TApplication:
      free_type (name, t->subtype);
      break;
  default :
      abort ();
  }
}

void
generate_type_free (const Symbol *s)
{
  fprintf (headerfile,
	   "void   free_%s  (%s *);\n",
	   s->gen_name, s->gen_name);

  fprintf (codefile, "void\n"
	   "free_%s(%s *data)\n"
	   "{\n",
	   s->gen_name, s->gen_name);

  free_type ("data", s->type);
  fprintf (codefile, "}\n\n");
}

