/*
 * scanner.c, libyaml scanner binding for Lua
 * Written by Gary V. Vaughan, 2013
 *
 * Copyright (C) 2013-2022 Gary V. Vaughan
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "lyaml.h"


typedef struct {
   lua_State	 *L;
   yaml_parser_t  parser;
   yaml_token_t	  token;
   char		  validtoken;
   int		  document_count;
} lyaml_scanner;


static void
scanner_delete_token (lyaml_scanner *scanner)
{
   if (scanner->validtoken)
   {
      yaml_token_delete (&scanner->token);
      scanner->validtoken = 0;
   }
}

/* With the token result table on the top of the stack, insert
   a mark entry. */
static void
scanner_set_mark (lua_State *L, const char *k, yaml_mark_t mark)
{
   lua_pushstring  (L, k);
   lua_createtable (L, 0, 3);
#define MENTRY(_s)	RAWSET_INTEGER (#_s, mark._s)
         MENTRY( index	);
         MENTRY( line	);
         MENTRY( column	);
#undef MENTRY
   lua_rawset (L, -3);
}

/* Push a new token table, pre-populated with shared elements. */
static void
scanner_push_tokentable (lyaml_scanner *scanner, const char *v, int n)
{
   lua_State *L = scanner->L;

   lua_createtable (L, 0, n + 3);
   RAWSET_STRING   ("type", v);

#define MENTRY(_s)	scanner_set_mark (L, #_s, scanner->token._s)
         MENTRY( start_mark	);
         MENTRY( end_mark	);
#undef MENTRY
}

static void
scan_STREAM_START (lyaml_scanner *scanner)
{
#define EVENTF(_f)	(scanner->token.data.stream_start._f)
   lua_State *L = scanner->L;
   const char *encoding;

   switch (EVENTF (encoding))
   {
#define MENTRY(_s)		\
      case YAML_##_s##_ENCODING: encoding = #_s; break
         MENTRY( UTF8		);
         MENTRY( UTF16LE	);
         MENTRY( UTF16BE	);
#undef MENTRY

      default:
         lua_pushfstring (L, "invalid encoding %d", EVENTF (encoding));
         lua_error (L);
   }

   scanner_push_tokentable (scanner, "STREAM_START", 1);
   RAWSET_STRING ("encoding", encoding);
#undef EVENTF
}

static void
scan_VERSION_DIRECTIVE (lyaml_scanner *scanner)
{
#define EVENTF(_f)	(scanner->token.data.version_directive._f)
   lua_State *L = scanner->L;

   scanner_push_tokentable (scanner, "VERSION_DIRECTIVE", 2);

#define MENTRY(_s)	RAWSET_INTEGER (#_s, EVENTF (_s))
         MENTRY( major	);
         MENTRY( minor	);
#undef MENTRY
#undef EVENTF
}

static void
scan_TAG_DIRECTIVE (lyaml_scanner *scanner)
{
#define EVENTF(_f)	(scanner->token.data.tag_directive._f)
   lua_State *L = scanner->L;

   scanner_push_tokentable (scanner, "TAG_DIRECTIVE", 2);
   RAWSET_EVENTF( handle	);
   RAWSET_EVENTF( prefix	);
#undef EVENTF
}

static void
scan_ALIAS (lyaml_scanner *scanner)
{
#define EVENTF(_f)	(scanner->token.data.alias._f)
   lua_State *L = scanner->L;

   scanner_push_tokentable (scanner, "ALIAS", 1);
   RAWSET_EVENTF (value);
#undef EVENTF
}

static void
scan_ANCHOR (lyaml_scanner *scanner)
{
#define EVENTF(_f)	(scanner->token.data.anchor._f)
   lua_State *L = scanner->L;

   scanner_push_tokentable (scanner, "ANCHOR", 1);
   RAWSET_EVENTF (value);
#undef EVENTF
}

static void
scan_TAG(lyaml_scanner *scanner)
{
#define EVENTF(_f)	(scanner->token.data.tag._f)
   lua_State *L = scanner->L;

   scanner_push_tokentable (scanner, "TAG", 2);
   RAWSET_EVENTF( handle	);
   RAWSET_EVENTF( suffix	);
#undef EVENTF
}

static void
scan_SCALAR (lyaml_scanner *scanner)
{
#define EVENTF(_f)	(scanner->token.data.scalar._f)
   lua_State *L = scanner->L;
   const char *style;

   switch (EVENTF (style))
   {
#define MENTRY(_s)		\
      case YAML_##_s##_SCALAR_STYLE: style = #_s; break

        MENTRY( PLAIN		);
        MENTRY( SINGLE_QUOTED	);
        MENTRY( DOUBLE_QUOTED	);
	MENTRY( LITERAL		);
	MENTRY( FOLDED		);
#undef MENTRY

      default:
         lua_pushfstring (L, "invalid scalar style %d", EVENTF (style));
         lua_error (L);
   }

   scanner_push_tokentable (scanner, "SCALAR", 3);
   RAWSET_EVENTF  (value);
   RAWSET_INTEGER ("length", EVENTF (length));
   RAWSET_STRING  ("style", style);
#undef EVENTF
}

static void
scanner_generate_error_message (lyaml_scanner *scanner)
{
   yaml_parser_t *P = &scanner->parser;
   char buf[256];
   luaL_Buffer b;

   luaL_buffinit (scanner->L, &b);
   luaL_addstring (&b, P->problem ? P->problem : "A problem");
   snprintf (buf, sizeof (buf), " at document: %d", scanner->document_count);
   luaL_addstring (&b, buf);

   if (P->problem_mark.line || P->problem_mark.column)
   {
      snprintf (buf, sizeof (buf), ", line: %lu, column: %lu",
         (unsigned long) P->problem_mark.line + 1,
         (unsigned long) P->problem_mark.column + 1);
      luaL_addstring (&b, buf);
   }
   luaL_addstring (&b, "\n");

   if (P->context)
   {
      snprintf (buf, sizeof (buf), "%s at line: %lu, column: %lu\n",
         P->context,
         (unsigned long) P->context_mark.line + 1,
         (unsigned long) P->context_mark.column + 1);
      luaL_addstring (&b, buf);
   }

   luaL_pushresult (&b);
}

static int
token_iter (lua_State *L)
{
   lyaml_scanner *scanner = (lyaml_scanner *)lua_touserdata(L, lua_upvalueindex(1));
   char *str;

   scanner_delete_token (scanner);
   if (yaml_parser_scan (&scanner->parser, &scanner->token) != 1)
   {
      scanner_generate_error_message (scanner);
      return lua_error (L);
   }

   scanner->validtoken = 1;

   lua_newtable    (L);
   lua_pushliteral (L, "type");

   switch (scanner->token.type)
   {
      /* First the simple tokens, generated right here... */
#define MENTRY(_s)			\
      case YAML_##_s##_TOKEN: scanner_push_tokentable (scanner, #_s, 0); break
         MENTRY( STREAM_END		);
         MENTRY( DOCUMENT_START		);
         MENTRY( DOCUMENT_END		);
         MENTRY( BLOCK_SEQUENCE_START	);
         MENTRY( BLOCK_MAPPING_START	);
         MENTRY( BLOCK_END		);
         MENTRY( FLOW_SEQUENCE_START	);
         MENTRY( FLOW_SEQUENCE_END	);
         MENTRY( FLOW_MAPPING_START	);
         MENTRY( FLOW_MAPPING_END	);
	 MENTRY( BLOCK_ENTRY		);
	 MENTRY( FLOW_ENTRY		);
	 MENTRY( KEY			);
	 MENTRY( VALUE			);
#undef MENTRY

      /* ...then the complex tokens, generated by a function call. */
#define MENTRY(_s)		\
      case YAML_##_s##_TOKEN: scan_##_s (scanner); break
         MENTRY( STREAM_START		);
	 MENTRY( VERSION_DIRECTIVE	);
	 MENTRY( TAG_DIRECTIVE		);
         MENTRY( ALIAS			);
	 MENTRY( ANCHOR			);
	 MENTRY( TAG			);
         MENTRY( SCALAR			);
#undef MENTRY

      case YAML_NO_TOKEN:
         lua_pushnil (L);
         break;
      default:
         lua_pushfstring  (L, "invalid token %d", scanner->token.type);
         return lua_error (L);
   }

   return 1;
}

static int
scanner_gc (lua_State *L)
{
   lyaml_scanner *scanner = (lyaml_scanner *) lua_touserdata (L, 1);

   if (scanner)
   {
      scanner_delete_token (scanner);
      yaml_parser_delete (&scanner->parser);
   }
   return 0;
}

void
scanner_init (lua_State *L)
{
   luaL_newmetatable (L, "lyaml.scanner");
   lua_pushcfunction (L, scanner_gc);
   lua_setfield      (L, -2, "__gc");
}

int
Pscanner (lua_State *L)
{
   lyaml_scanner *scanner;
   const unsigned char *str;

   /* requires a single string type argument */
   luaL_argcheck (L, lua_isstring (L, 1), 1, "must provide a string argument");
   str = (const unsigned char *) lua_tostring (L, 1);

   /* create a user datum to store the scanner */
   scanner = (lyaml_scanner *) lua_newuserdata (L, sizeof (*scanner));
   memset ((void *) scanner, 0, sizeof (*scanner));
   scanner->L = L;

   /* set its metatable */
   luaL_getmetatable (L, "lyaml.scanner");
   lua_setmetatable  (L, -2);

   /* try to initialize the scanner */
   if (yaml_parser_initialize (&scanner->parser) == 0)
      luaL_error (L, "cannot initialize parser for %s", str);
   yaml_parser_set_input_string (&scanner->parser, str, lua_strlen (L, 1));

   /* create and return the iterator function, with the loader userdatum as
      its sole upvalue */
   lua_pushcclosure (L, token_iter, 1);
   return 1;
}
