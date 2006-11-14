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

RCSID("$Id: gen_copy.c,v 1.12 2001/09/25 13:39:26 assar Exp $");

static void
copy_primitive (const char *typename, const char *from, const char *to)
{
    fprintf (codefile, "if(copy_%s(%s, %s)) return ENOMEM;\n", 
	     typename, from, to);
}

static void
copy_type (const char *from, const char *to, const Type *t)
{
  switch (t->type) {
  case TType:
#if 0
      copy_type (from, to, t->symbol->type);
#endif
      fprintf (codefile, "if(copy_%s(%s, %s)) return ENOMEM;\n", 
	       t->symbol->gen_name, from, to);
      break;
  case TInteger:
  case TUInteger:
  case TEnumerated :
      fprintf(codefile, "*(%s) = *(%s);\n", to, from);
      break;
  case TOctetString:
      copy_primitive ("octet_string", from, to);
      break;
  case TOID:
      copy_primitive ("oid", from, to);
      break;
  case TBitString: {
      fprintf(codefile, "*(%s) = *(%s);\n", to, from);
      break;
  }
  case TSequence: {
      Member *m;
      int tag = -1;

      if (t->members == NULL)
	  break;
      
      for (m = t->members; m && tag != m->val; m = m->next) {
	  char *f;
	  char *t;

	  asprintf (&f, "%s(%s)->%s",
		    m->optional ? "" : "&", from, m->gen_name);
	  asprintf (&t, "%s(%s)->%s",
		    m->optional ? "" : "&", to, m->gen_name);
	  if(m->optional){
	      fprintf(codefile, "if(%s) {\n", f);
	      fprintf(codefile, "%s = malloc(sizeof(*%s));\n", t, t);
	      fprintf(codefile, "if(%s == NULL) return ENOMEM;\n", t);
	  }
	  copy_type (f, t, m->type);
	  if(m->optional){
	      fprintf(codefile, "}else\n");
	      fprintf(codefile, "%s = NULL;\n", t);
	  }
	  if (tag == -1)
	      tag = m->val;
	  free (f);
	  free (t);
      }
      break;
  }
  case TSequenceOf: {
      char *f;
      char *T;

      fprintf (codefile, "if(((%s)->val = "
	       "malloc((%s)->len * sizeof(*(%s)->val))) == NULL && (%s)->len != 0)\n", 
	       to, from, to, from);
      fprintf (codefile, "return ENOMEM;\n");
      fprintf(codefile,
	      "for((%s)->len = 0; (%s)->len < (%s)->len; (%s)->len++){\n",
	      to, to, from, to);
      asprintf(&f, "&(%s)->val[(%s)->len]", from, to);
      asprintf(&T, "&(%s)->val[(%s)->len]", to, to);
      copy_type(f, T, t->subtype);
      fprintf(codefile, "}\n");
      free(f);
      free(T);
      break;
  }
  case TGeneralizedTime:
      fprintf(codefile, "*(%s) = *(%s);\n", to, from);
      break;
  case TGeneralString:
      copy_primitive ("general_string", from, to);
      break;
  case TApplication:
      copy_type (from, to, t->subtype);
      break;
  default :
      abort ();
  }
}

void
generate_type_copy (const Symbol *s)
{
  fprintf (headerfile,
	   "int    copy_%s  (const %s *, %s *);\n",
	   s->gen_name, s->gen_name, s->gen_name);

  fprintf (codefile, "int\n"
	   "copy_%s(const %s *from, %s *to)\n"
	   "{\n",
	   s->gen_name, s->gen_name, s->gen_name);

  copy_type ("from", "to", s->type);
  fprintf (codefile, "return 0;\n}\n\n");
}

