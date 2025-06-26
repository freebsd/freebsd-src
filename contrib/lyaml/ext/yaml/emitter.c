/*
 * emitter.c, LibYAML emitter binding for Lua
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

#include <assert.h>

#include "lyaml.h"


typedef struct {
   yaml_emitter_t   emitter;

   /* output accumulator */
   lua_State	   *outputL;
   luaL_Buffer	    yamlbuff;

   /* error handling */
   lua_State	   *errL;
   luaL_Buffer	    errbuff;
   int		    error;
} lyaml_emitter;


/* Emit a STREAM_START event. */
static int
emit_STREAM_START (lua_State *L, lyaml_emitter *emitter)
{
   yaml_event_t event;
   yaml_encoding_t yaml_encoding;
   const char *encoding = NULL;

   RAWGET_STRDUP (encoding); lua_pop (L, 1);

#define MENTRY(_s) (STREQ (encoding, #_s)) { yaml_encoding = YAML_##_s##_ENCODING; }
   if (encoding == NULL) { yaml_encoding = YAML_ANY_ENCODING; } else
   if MENTRY( UTF8	) else
   if MENTRY( UTF16LE	) else
   if MENTRY( UTF16BE	) else
   {
      emitter->error++;
      luaL_addsize (&emitter->errbuff,
                    sprintf (luaL_prepbuffer (&emitter->errbuff),
                             "invalid stream encoding '%s'", encoding));
   }
#undef MENTRY

   if (encoding) free ((void *) encoding);

   if (emitter->error != 0)
     return 0;

   yaml_stream_start_event_initialize (&event, yaml_encoding);
   return yaml_emitter_emit (&emitter->emitter, &event);
}


/* Emit a STREAM_END event. */
static int
emit_STREAM_END (lua_State *L, lyaml_emitter *emitter)
{
   yaml_event_t event;
   yaml_stream_end_event_initialize (&event);
   return yaml_emitter_emit (&emitter->emitter, &event);
}


/* Emit a DOCUMENT_START event. */
static int
emit_DOCUMENT_START (lua_State *L, lyaml_emitter *emitter)
{
   yaml_event_t event;
   yaml_version_directive_t version_directive, *Pversion_directive = NULL;
   yaml_tag_directive_t *tag_directives_start = NULL, *tag_directives_end = NULL;
   int implicit = 0;

   RAWGET_PUSHTABLE ("version_directive");
   if (lua_type (L, -1) == LUA_TTABLE)
   {
      RAWGETS_INTEGER (version_directive.major, "major");
      ERROR_IFNIL ("version_directive missing key 'major'");
      if (emitter->error == 0)
      {
         RAWGETS_INTEGER (version_directive.minor, "minor");
         ERROR_IFNIL ("version_directive missing key 'minor'");
      }
      Pversion_directive = &version_directive;
   }
   lua_pop (L, 1);		/* pop version_directive rawget */

   RAWGET_PUSHTABLE ("tag_directives");
   if (lua_type (L, -1) == LUA_TTABLE)
   {
      size_t bytes = lua_objlen (L, -1) * sizeof (yaml_tag_directive_t);

      tag_directives_start = (yaml_tag_directive_t *) malloc (bytes);
      tag_directives_end = tag_directives_start;

      lua_pushnil (L);	/* first key */
      while (lua_next (L, -2) != 0)
      {
         RAWGETS_STRDUP (tag_directives_end->handle, "handle");
         ERROR_IFNIL ("tag_directives item missing key 'handle'");
         lua_pop (L, 1);	/* pop handle */

         RAWGETS_STRDUP (tag_directives_end->prefix, "prefix");
         ERROR_IFNIL ("tag_directives item missing key 'prefix'");
         lua_pop (L, 1);	/* pop prefix */

         tag_directives_end += 1;

         /* pop tag_directives list elewent, leave key for next iteration */
         lua_pop (L, 1);
      }
   }
   lua_pop (L, 1);	/* pop lua_rawget "tag_directives" result */

   RAWGET_BOOLEAN (implicit); lua_pop (L, 1);

   if (emitter->error != 0)
      return 0;

   yaml_document_start_event_initialize (&event, Pversion_directive,
      tag_directives_start, tag_directives_end, implicit);
   return yaml_emitter_emit (&emitter->emitter, &event);
}


/* Emit a DOCUMENT_END event. */
static int
emit_DOCUMENT_END (lua_State *L, lyaml_emitter *emitter)
{
   yaml_event_t event;
   int implicit = 0;

   RAWGET_BOOLEAN (implicit);

   yaml_document_end_event_initialize (&event, implicit);
   return yaml_emitter_emit (&emitter->emitter, &event);
}


/* Emit a MAPPING_START event. */
static int
emit_MAPPING_START (lua_State *L, lyaml_emitter *emitter)
{
   yaml_event_t event;
   yaml_mapping_style_t yaml_style;
   yaml_char_t *anchor = NULL, *tag = NULL;
   int implicit = 1;
   const char *style = NULL;

   RAWGET_STRDUP (style);  lua_pop (L, 1);

#define MENTRY(_s) (STREQ (style, #_s)) { yaml_style = YAML_##_s##_MAPPING_STYLE; }
   if (style == NULL) { yaml_style = YAML_ANY_MAPPING_STYLE; } else
   if MENTRY( BLOCK	) else
   if MENTRY( FLOW	) else
   {
      emitter->error++;
      luaL_addsize (&emitter->errbuff,
                    sprintf (luaL_prepbuffer (&emitter->errbuff),
                             "invalid mapping style '%s'", style));
   }
#undef MENTRY

   if (style) free ((void *) style);

   RAWGET_YAML_CHARP (anchor); lua_pop (L, 1);
   RAWGET_YAML_CHARP (tag);    lua_pop (L, 1);
   RAWGET_BOOLEAN (implicit);  lua_pop (L, 1);

   yaml_mapping_start_event_initialize (&event, anchor, tag, implicit, yaml_style);
   return yaml_emitter_emit (&emitter->emitter, &event);
}


/* Emit a MAPPING_END event. */
static int
emit_MAPPING_END (lua_State *L, lyaml_emitter *emitter)
{
   yaml_event_t event;
   yaml_mapping_end_event_initialize (&event);
   return yaml_emitter_emit (&emitter->emitter, &event);
}


/* Emit a SEQUENCE_START event. */
static int
emit_SEQUENCE_START (lua_State *L, lyaml_emitter *emitter)
{
   yaml_event_t event;
   yaml_sequence_style_t yaml_style;
   yaml_char_t *anchor = NULL, *tag = NULL;
   int implicit = 1;
   const char *style = NULL;

   RAWGET_STRDUP (style);  lua_pop (L, 1);

#define MENTRY(_s) (STREQ (style, #_s)) { yaml_style = YAML_##_s##_SEQUENCE_STYLE; }
   if (style == NULL) { yaml_style = YAML_ANY_SEQUENCE_STYLE; } else
   if MENTRY( BLOCK	) else
   if MENTRY( FLOW	) else
   {
      emitter->error++;
      luaL_addsize (&emitter->errbuff,
                    sprintf (luaL_prepbuffer (&emitter->errbuff),
                             "invalid sequence style '%s'", style));
   }
#undef MENTRY

   if (style) free ((void *) style);

   RAWGET_YAML_CHARP (anchor); lua_pop (L, 1);
   RAWGET_YAML_CHARP (tag);    lua_pop (L, 1);
   RAWGET_BOOLEAN (implicit);  lua_pop (L, 1);

   yaml_sequence_start_event_initialize (&event, anchor, tag, implicit, yaml_style);
   return yaml_emitter_emit (&emitter->emitter, &event);
}


/* Emit a SEQUENCE_END event. */
static int
emit_SEQUENCE_END (lua_State *L, lyaml_emitter *emitter)
{
   yaml_event_t event;
   yaml_sequence_end_event_initialize (&event);
   return yaml_emitter_emit (&emitter->emitter, &event);
}


/* Emit a SCALAR event. */
static int
emit_SCALAR (lua_State *L, lyaml_emitter *emitter)
{
   yaml_event_t event;
   yaml_scalar_style_t yaml_style;
   yaml_char_t *anchor = NULL, *tag = NULL, *value;
   int length = 0, plain_implicit = 1, quoted_implicit = 1;
   const char *style = NULL;

   RAWGET_STRDUP (style);  lua_pop (L, 1);

#define MENTRY(_s) (STREQ (style, #_s)) { yaml_style = YAML_##_s##_SCALAR_STYLE; }
   if (style == NULL) { yaml_style = YAML_ANY_SCALAR_STYLE; } else
   if MENTRY( PLAIN		) else
   if MENTRY( SINGLE_QUOTED	) else
   if MENTRY( DOUBLE_QUOTED	) else
   if MENTRY( LITERAL		) else
   if MENTRY( FOLDED		) else
   {
      emitter->error++;
      luaL_addsize (&emitter->errbuff,
                    sprintf (luaL_prepbuffer (&emitter->errbuff),
                             "invalid scalar style '%s'", style));
   }
#undef MENTRY

   if (style) free ((void *) style);

   RAWGET_YAML_CHARP (anchor); lua_pop (L, 1);
   RAWGET_YAML_CHARP (tag);    lua_pop (L, 1);
   RAWGET_YAML_CHARP (value);  length = lua_objlen (L, -1); lua_pop (L, 1);
   RAWGET_BOOLEAN (plain_implicit);
   RAWGET_BOOLEAN (quoted_implicit);

   yaml_scalar_event_initialize (&event, anchor, tag, value, length,
      plain_implicit, quoted_implicit, yaml_style);
   return yaml_emitter_emit (&emitter->emitter, &event);
}


/* Emit an ALIAS event. */
static int
emit_ALIAS (lua_State *L, lyaml_emitter *emitter)
{
   yaml_event_t event;
   yaml_char_t *anchor;

   RAWGET_YAML_CHARP (anchor);

   yaml_alias_event_initialize (&event, anchor);
   return yaml_emitter_emit (&emitter->emitter, &event);
}


static int
emit (lua_State *L)
{
   lyaml_emitter *emitter;
   int yaml_ok = 0;
   int finalize = 0;

   luaL_argcheck (L, lua_istable (L, 1), 1, "expected table");

   emitter = (lyaml_emitter *) lua_touserdata (L, lua_upvalueindex (1));

   {
     const char *type;

     RAWGET_STRDUP (type); lua_pop (L, 1);

     if (type == NULL)
     {
        emitter->error++;
        luaL_addstring (&emitter->errbuff, "no type field in event table");
     }
#define MENTRY(_s) (STREQ (type, #_s)) { yaml_ok = emit_##_s (L, emitter); }
     /* Minimize comparisons by putting more common types earlier. */
     else if MENTRY( SCALAR		)
     else if MENTRY( MAPPING_START	)
     else if MENTRY( MAPPING_END	)
     else if MENTRY( SEQUENCE_START	)
     else if MENTRY( SEQUENCE_END	)
     else if MENTRY( DOCUMENT_START	)
     else if MENTRY( DOCUMENT_END	)
     else if MENTRY( STREAM_START	)
     else if MENTRY( STREAM_END		)
     else if MENTRY( ALIAS		)
#undef MENTRY
     else
     {
        emitter->error++;
        luaL_addsize (&emitter->errbuff,
                      sprintf (luaL_prepbuffer (&emitter->errbuff),
                               "invalid event type '%s'", type));
     }

     /* If the stream has finished, finalize the YAML output. */
     if (type && STREQ (type, "STREAM_END"))
       finalize = 1;

     if (type) free ((void *) type);
   }

   /* Copy any yaml_emitter_t errors into the error buffer. */
   if (!emitter->error && !yaml_ok)
   {
      if (emitter->emitter.problem)
        luaL_addstring (&emitter->errbuff, emitter->emitter.problem);
      else
        luaL_addstring (&emitter->errbuff, "LibYAML call failed");
      emitter->error++;
   }

   /* Report errors back to the caller as `false, "error message"`. */
   if (emitter->error != 0)
   {
      assert (emitter->error == 1);  /* bail on uncaught additional errors */
      lua_pushboolean (L, 0);
      luaL_pushresult (&emitter->errbuff);
      lua_xmove (emitter->errL, L, 1);
      return 2;
   }

   /* Return `true, "YAML string"` after accepting a STREAM_END event. */
   if (finalize)
   {
      lua_pushboolean (L, 1);
      luaL_pushresult (&emitter->yamlbuff);
      lua_xmove (emitter->outputL, L, 1);
      return 2;
   }

   /* Otherwise, just report success to the caller as `true`. */
   lua_pushboolean (L, 1);
   return 1;
}


static int
append_output (void *arg, unsigned char *buff, size_t len)
{
   lyaml_emitter *emitter = (lyaml_emitter *) arg;
   luaL_addlstring (&emitter->yamlbuff, (char *) buff, len);
   return 1;
}


static int
emitter_gc (lua_State *L)
{
   lyaml_emitter *emitter = (lyaml_emitter *) lua_touserdata (L, 1);

   if (emitter)
      yaml_emitter_delete (&emitter->emitter);

   return 0;
}


int
Pemitter (lua_State *L)
{
   lyaml_emitter *emitter;

   lua_newtable (L);	/* object table */

   /* Create a user datum to store the emitter. */
   emitter = (lyaml_emitter *) lua_newuserdata (L, sizeof (*emitter));
   emitter->error = 0;

   /* Initialize the emitter. */
   if (!yaml_emitter_initialize (&emitter->emitter))
   {
      if (!emitter->emitter.problem)
         emitter->emitter.problem = "cannot initialize emitter";
      return luaL_error (L, "%s", emitter->emitter.problem);
   }
   yaml_emitter_set_unicode (&emitter->emitter, 1);
   yaml_emitter_set_width   (&emitter->emitter, 2);
   yaml_emitter_set_output  (&emitter->emitter, &append_output, emitter);

   /* Set it's metatable, and ensure it is garbage collected properly. */
   luaL_newmetatable (L, "lyaml.emitter");
   lua_pushcfunction (L, emitter_gc);
   lua_setfield      (L, -2, "__gc");
   lua_setmetatable  (L, -2);

   /* Set the emit method of object as a closure over the user datum, and
      return the whole object. */
   lua_pushcclosure (L, emit, 1);
   lua_setfield (L, -2, "emit");

   /* Set up a separate thread to collect error messages; save the thread
      in the returned table so that it's not garbage collected when the
      function call stack for Pemitter is cleaned up.  */
   emitter->errL = lua_newthread (L);
   luaL_buffinit (emitter->errL, &emitter->errbuff);
   lua_setfield (L, -2, "errthread");

   /* Create a thread for the YAML buffer. */
   emitter->outputL = lua_newthread (L);
   luaL_buffinit (emitter->outputL, &emitter->yamlbuff);
   lua_setfield (L, -2, "outputthread");

   return 1;
}
