/* Interface to LUCID Cadillac system for GNU compiler.
   Copyright (C) 1988, 1992, 1993 Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "config.h"

#include "tree.h"
#include "flags.h"
#include <stdio.h>
#include "cp-tree.h"
#include "obstack.h"

#ifdef CADILLAC
#include <compilerreq.h>
#include <compilerconn.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/file.h>

#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free free

void init_cadillac ();

extern char *input_filename;
extern int lineno;

/* Put random information we might want to get back from
   Cadillac here.  */
typedef struct
{
  /* The connection to the Cadillac kernel.  */
  Connection *conn;

  /* Input and output file descriptors for Cadillac.  */
  short fd_input, fd_output;

  /* #include nesting of current file.  */
  short depth;

  /* State variables for the connection.  */
  char messages;
  char conversion;
  char emission;
  char process_until;

  /* #if level of current file.  */
  int iflevel;

  /* Line number that starts current source file.  */
  int lineno;

  /* Name of current file.  */
  char *filename;

  /* Where to stop processing (if process_until is set).  */
  char *end_filename;
  int end_position;

} cadillac_struct;
static cadillac_struct cadillacObj;

/* Nonzero if in the process of exiting.  */
static int exiting;

void cadillac_note_source ();
static void CWriteLanguageDecl ();
static void CWriteLanguageType ();
static void CWriteTopLevel ();
static void cadillac_note_filepos ();
static void cadillac_process_request (), cadillac_process_requests ();
static void cadillac_switch_source ();
static void exit_cadillac ();

/* Blocking test.  */
static int
readable_p (fd)
     int fd;
{
  fd_set f;

  FD_ZERO (&f);
  FD_SET (fd, &f);

  return select (32, &f, NULL, NULL, 0) == 1;
}

static CObjectType *tree_to_cadillac_map;
struct obstack cadillac_obstack;


#include "stack.h"

struct context_level
{
  struct stack_level base;

  tree context;
};

/* Stack for maintaining contexts (in case functions or types are nested).
   When defining a struct type, the `context' field is the RECORD_TYPE.
   When defining a function, the `context' field is the FUNCTION_DECL.  */

static struct context_level *context_stack;

static struct context_level *
push_context_level (stack, obstack)
     struct stack_level *stack;
     struct obstack *obstack;
{
  struct context_level tem;

  tem.base.prev = stack;
  return (struct context_level *)push_stack_level (obstack, &tem, sizeof (tem));
}

/* Discard a level of search allocation.  */

static struct context_level *
pop_context_level (stack)
     struct context_level *stack;
{
  stack = (struct context_level *)pop_stack_level (stack);
  return stack;
}

void
init_cadillac ()
{
  extern FILE *finput;
  extern int errno;
  CCompilerMessage* req;
  cadillac_struct *cp = &cadillacObj;
  int i;

  if (! flag_cadillac)
    return;

  tree_to_cadillac_map = (CObjectType*) xmalloc (sizeof (CObjectType) * LAST_CPLUS_TREE_CODE);
  for (i = 0; i < LAST_CPLUS_TREE_CODE; i++)
    tree_to_cadillac_map[i] = MiscOType;
  tree_to_cadillac_map[RECORD_TYPE] = StructOType;
  tree_to_cadillac_map[UNION_TYPE] = UnionOType;
  tree_to_cadillac_map[ENUMERAL_TYPE] = EnumTypeOType;
  tree_to_cadillac_map[TYPE_DECL] = TypedefOType;
  tree_to_cadillac_map[VAR_DECL] = VariableOType;
  tree_to_cadillac_map[CONST_DECL] = EnumConstantOType;
  tree_to_cadillac_map[FUNCTION_DECL] = FunctionOType;
  tree_to_cadillac_map[FIELD_DECL] = FieldOType;

#ifdef sun
  on_exit (&exit_cadillac, 0);
#endif

  gcc_obstack_init (&cadillac_obstack);

  /* Yow!  This is the way Cadillac was designed to deal with
     Oregon C++ compiler!  */
  cp->fd_input = flag_cadillac;
  cp->fd_output = flag_cadillac;

  /* Start in "turned-on" state.  */
  cp->messages = 1;
  cp->conversion = 1;
  cp->emission = 1;

  /* Establish a connection with Cadillac here.  */
  cp->conn = NewConnection (cp, cp->fd_input, cp->fd_output);

  CWriteHeader (cp->conn, WaitingMType, 0);
  CWriteRequestBuffer (cp->conn);

  if (!readable_p (cp->fd_input))
    ;

  req = CReadCompilerMessage (cp->conn);

  if (!req)
    switch (errno)
      {
      case EWOULDBLOCK:
	sleep (5);
	return;
      
      case 0:
	fatal ("init_cadillac: EOF on connection to kernel, exiting\n");
	break;

      default:
	perror ("Editor to kernel connection");
	exit (0);
      }
}

static void
cadillac_process_requests (conn)
     Connection *conn;
{
  CCompilerMessage *req;
  while (req = (CCompilerMessage*) CPeekNextRequest (conn))
    {
      req = CReadCompilerMessage (conn);
      cadillac_process_request (&cadillacObj, req);
    }
}

static void
cadillac_process_request (cp, req)
     cadillac_struct *cp;
     CCompilerMessage *req;
{
  if (! req)
    return;

  switch (req->reqType)
    {
    case ProcessUntilMType:
      if (cp->process_until)
	my_friendly_abort (23);
      cp->process_until = 1;
      /* This is not really right.  */
      cp->end_position = ((CCompilerCommand*)req)->processuntil.position;
#if 0
      cp->end_filename = req->processuntil.filename;
#endif
      break;

    case CommandMType:
      switch (req->header.data)
	{
	case MessagesOnCType:
	  cp->messages = 1;
	  break;
	case MessagesOffCType:
	  cp->messages = 0;
	  break;
	case ConversionOnCType:
	  cp->conversion = 1;
	  break;
	case ConversionOffCType:
	  cp->conversion = 0;
	  break;
	case EmissionOnCType:
	  cp->emission = 1;
	  break;
	case EmissionOffCType:
	  cp->emission = 0;
	  break;

	case FinishAnalysisCType:
	  return;

	case PuntAnalysisCType:
	case ContinueAnalysisCType:
	case GotoFileposCType:
	case OpenSucceededCType:
	case OpenFailedCType:
	  fprintf (stderr, "request type %d not implemented\n", req->reqType);
	  return;

	case DieCType:
	  if (! exiting)
	    my_friendly_abort (24);
	  return;

	}
      break;

    default:
      fatal ("unknown request type %d", req->reqType);
    }
}

void
cadillac_start ()
{
  Connection *conn = cadillacObj.conn;
  CCompilerMessage *req;

  /* Let Cadillac know that we start in C++ language scope.  */
  CWriteHeader (conn, ForeignLinkageMType, LinkCPlus);
  CWriteLength (conn);
  CWriteRequestBuffer (conn);

  cadillac_process_requests (conn);
}

static void
cadillac_printf (msg, name)
{
  if (cadillacObj.messages)
    printf ("[%s,%4d] %s `%s'\n", input_filename, lineno, msg, name);
}

void
cadillac_start_decl (decl)
     tree decl;
{
  Connection *conn = cadillacObj.conn;
  CObjectType object_type = tree_to_cadillac_map [TREE_CODE (decl)];

  if (context_stack)
    switch (TREE_CODE (context_stack->context))
      {
      case FUNCTION_DECL:
	/* Currently, cadillac only implements top-level forms.  */
	return;
      case RECORD_TYPE:
      case UNION_TYPE:
	cadillac_printf ("start class-level decl", IDENTIFIER_POINTER (DECL_NAME (decl)));
	break;
      default:
	my_friendly_abort (25);
      }
  else
    {
      cadillac_printf ("start top-level decl", IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl)));
      CWriteTopLevel (conn, StartMType);
    }

  CWriteLanguageDecl (conn, decl, tree_to_cadillac_map[TREE_CODE (decl)]);
  CWriteRequestBuffer (conn);
  cadillac_process_requests (conn);
}

void
cadillac_finish_decl (decl)
     tree decl;
{
  Connection *conn = cadillacObj.conn;

  if (context_stack)
    switch (TREE_CODE (context_stack->context))
      {
      case FUNCTION_DECL:
	return;
      case RECORD_TYPE:
      case UNION_TYPE:
	cadillac_printf ("end class-level decl", IDENTIFIER_POINTER (DECL_NAME (decl)));
	CWriteHeader (conn, EndDefMType, 0);
	CWriteLength (conn);
	break;
      default:
	my_friendly_abort (26);
      }
  else
    {
      cadillac_printf ("end top-level decl", IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl)));
      CWriteHeader (conn, EndDefMType, 0);
      CWriteLength (conn);
      CWriteTopLevel (conn, StopMType);
    }

  CWriteRequestBuffer (conn);
  cadillac_process_requests (conn);
}

void
cadillac_start_function (fndecl)
     tree fndecl;
{
  Connection *conn = cadillacObj.conn;

  if (context_stack)
    /* nested functions not yet handled.  */
    my_friendly_abort (27);

  cadillac_printf ("start top-level function", lang_printable_name (fndecl));
  context_stack = push_context_level (context_stack, &cadillac_obstack);
  context_stack->context = fndecl;

  CWriteTopLevel (conn, StartMType);
  my_friendly_assert (TREE_CODE (fndecl) == FUNCTION_DECL, 202);
  CWriteLanguageDecl (conn, fndecl,
		      (TREE_CODE (TREE_TYPE (fndecl)) == METHOD_TYPE
		       ? MemberFnOType : FunctionOType));
  CWriteRequestBuffer (conn);
  cadillac_process_requests (conn);
}

void
cadillac_finish_function (fndecl)
     tree fndecl;
{
  Connection *conn = cadillacObj.conn;

  cadillac_printf ("end top-level function", lang_printable_name (fndecl));
  context_stack = pop_context_level (context_stack);

  if (context_stack)
    /* nested functions not yet implemented.  */
    my_friendly_abort (28);

  CWriteHeader (conn, EndDefMType, 0);
  CWriteLength (conn);
  CWriteTopLevel (conn, StopMType);
  CWriteRequestBuffer (conn);
  cadillac_process_requests (conn);
}

void
cadillac_finish_anon_union (decl)
     tree decl;
{
  Connection *conn = cadillacObj.conn;

  if (! global_bindings_p ())
    return;
  cadillac_printf ("finish top-level anon union", "");
  CWriteHeader (conn, EndDefMType, 0);
  CWriteLength (conn);
  CWriteTopLevel (conn, StopMType);
  CWriteRequestBuffer (conn);
  cadillac_process_requests (conn);
}

void
cadillac_start_enum (type)
     tree type;
{
  Connection *conn = cadillacObj.conn;

  tree name = TYPE_NAME (type);

  if (TREE_CODE (name) == TYPE_DECL)
    name = DECL_NAME (name);

  if (context_stack)
    switch (TREE_CODE (context_stack->context))
      {
      case FUNCTION_DECL:
	return;
      case RECORD_TYPE:
      case UNION_TYPE:
	break;
      default:
	my_friendly_abort (29);
      }
  else
    {
      cadillac_printf ("start top-level enum", IDENTIFIER_POINTER (name));
      CWriteTopLevel (conn, StartMType);
    }

  CWriteLanguageType (conn, type, tree_to_cadillac_map[ENUMERAL_TYPE]);
}

void
cadillac_finish_enum (type)
     tree type;
{
  Connection *conn = cadillacObj.conn;
  tree name = TYPE_NAME (type);

  if (TREE_CODE (name) == TYPE_DECL)
    name = DECL_NAME (name);

  if (context_stack)
    switch (TREE_CODE (context_stack->context))
      {
      case FUNCTION_DECL:
	return;
      case RECORD_TYPE:
      case UNION_TYPE:
	CWriteHeader (conn, EndDefMType, 0);
	CWriteLength (conn);
	break;
      default:
	my_friendly_abort (30);
      }
  else
    {
      CWriteHeader (conn, EndDefMType, 0);
      CWriteLength (conn);
      cadillac_printf ("finish top-level enum", IDENTIFIER_POINTER (name));
      CWriteTopLevel (conn, StopMType);
    }

  CWriteRequestBuffer (conn);
  cadillac_process_requests (conn);
}

void
cadillac_start_struct (type)
     tree type;
{
  Connection *conn = cadillacObj.conn;
  tree name = TYPE_NAME (type);

  if (TREE_CODE (name) == TYPE_DECL)
    name = DECL_NAME (name);

  if (context_stack)
    switch (TREE_CODE (context_stack->context))
      {
      case FUNCTION_DECL:
	return;
      case RECORD_TYPE:
      case UNION_TYPE:
	return;
      default:
	my_friendly_abort (31);
      }
  else
    {
      cadillac_printf ("start struct", IDENTIFIER_POINTER (name));
      CWriteTopLevel (conn, StartMType);
    }

  context_stack = push_context_level (context_stack, &cadillac_obstack);
  context_stack->context = type;

  CWriteLanguageType (conn, type,
		      TYPE_LANG_SPECIFIC (type) && CLASSTYPE_DECLARED_CLASS (type) ? ClassOType : tree_to_cadillac_map[TREE_CODE (type)]);
}

void
cadillac_finish_struct (type)
     tree type;
{
  Connection *conn = cadillacObj.conn;
  tree name = TYPE_NAME (type);

  if (TREE_CODE (name) == TYPE_DECL)
    name = DECL_NAME (name);

  context_stack = pop_context_level (context_stack);
  if (context_stack)
    return;

  cadillac_printf ("finish struct", IDENTIFIER_POINTER (name));
  CWriteHeader (conn, EndDefMType, 0);
  CWriteLength (conn);
  CWriteTopLevel (conn, StopMType);
  CWriteRequestBuffer (conn);
  cadillac_process_requests (conn);
}

void
cadillac_finish_exception (type)
     tree type;
{
  Connection *conn = cadillacObj.conn;

  fatal ("cadillac_finish_exception");
  CWriteHeader (conn, EndDefMType, 0);
  CWriteLength (conn);
  CWriteTopLevel (conn, StopMType);
  CWriteRequestBuffer (conn);
  cadillac_process_requests (conn);
}

void
cadillac_push_class (type)
     tree type;
{
}

void
cadillac_pop_class ()
{
}

void
cadillac_push_lang (name)
     tree name;
{
  Connection *conn = cadillacObj.conn;
  CLinkLanguageType m;

  if (name == lang_name_cplusplus)
    m = LinkCPlus;
  else if (name == lang_name_c)
    m = LinkC;
  else
    my_friendly_abort (32);
  CWriteHeader (conn, ForeignLinkageMType, m);
  CWriteRequestBuffer (conn);
  cadillac_process_requests (conn);
}

void
cadillac_pop_lang ()
{
  Connection *conn = cadillacObj.conn;

  CWriteHeader (conn, ForeignLinkageMType, LinkPop);
  CWriteRequestBuffer (conn);
  cadillac_process_requests (conn);
}

void
cadillac_finish_stmt ()
{
}

void
cadillac_note_source ()
{
  cadillacObj.lineno = lineno;
  cadillacObj.filename = input_filename;
}

static void
CWriteTopLevel (conn, m)
     Connection *conn;
     CMessageSubType m;
{
  static context_id = 0;
  CWriteHeader (conn, TopLevelFormMType, m);
  cadillac_note_filepos ();

  /* Eventually, this will point somewhere into the digest file.  */
  context_id += 1;
  CWriteSomething (conn, &context_id, sizeof (BITS32));

  CWriteSomething (conn, &cadillacObj.iflevel, sizeof (BITS32));
  CWriteLength (conn);
}

static void
cadillac_note_filepos ()
{
  extern FILE *finput;
  int pos = ftell (finput);
  CWriteSomething (cadillacObj.conn, &pos, sizeof (BITS32));
}

void
cadillac_switch_source (startflag)
     int startflag;
{
  Connection *conn = cadillacObj.conn;
  /* Send out the name of the source file being compiled.  */

  CWriteHeader (conn, SourceFileMType, startflag ? StartMType : StopMType);
  CWriteSomething (conn, &cadillacObj.depth, sizeof (BITS16));
  CWriteVstring0 (conn, input_filename);
  CWriteLength (conn);
  CWriteRequestBuffer (conn);
  cadillac_process_requests (conn);
}

void
cadillac_push_source ()
{
  cadillacObj.depth += 1;
  cadillac_switch_source (1);
}

void
cadillac_pop_source ()
{
  cadillacObj.depth -= 1;
  cadillac_switch_source (0);
}

struct cadillac_mdep
{
  short object_type;
  char linkage;
  char access;
  short length;
};

static void
CWriteLanguageElem (conn, p, name)
     Connection *conn;
     struct cadillac_mdep *p;
     char *name;
{
  CWriteSomething (conn, &p->object_type, sizeof (BITS16));
  CWriteSomething (conn, &p->linkage, sizeof (BITS8));
  CWriteSomething (conn, &p->access, sizeof (BITS8));
  CWriteSomething (conn, &p->length, sizeof (BITS16));
  CWriteVstring0 (conn, name);

#if 0
  /* Don't write date_type.  */
  CWriteVstring0 (conn, "");
#endif
  CWriteLength (conn);
}

static void
CWriteLanguageDecl (conn, decl, object_type)
     Connection *conn;
     tree decl;
     CObjectType object_type;
{
  struct cadillac_mdep foo;
  tree name;

  CWriteHeader (conn, LanguageElementMType, StartDefineMType);
  foo.object_type = object_type;
  if (decl_type_context (decl))
    {
      foo.linkage = ParentLinkage;
      if (TREE_PRIVATE (decl))
	foo.access = PrivateAccess;
      else if (TREE_PROTECTED (decl))
	foo.access = ProtectedAccess;
      else
	foo.access = PublicAccess;
    }
  else
    {
      if (TREE_PUBLIC (decl))
	foo.linkage = GlobalLinkage;
      else
	foo.linkage = FileLinkage;
      foo.access = PublicAccess;
    }
  name = DECL_NAME (decl);
  foo.length = IDENTIFIER_LENGTH (name);

  CWriteLanguageElem (conn, &foo, IDENTIFIER_POINTER (name));
  CWriteRequestBuffer (conn);
  cadillac_process_requests (conn);
}

static void
CWriteLanguageType (conn, type, object_type)
     Connection *conn;
     tree type;
     CObjectType object_type;
{
  struct cadillac_mdep foo;
  tree name = TYPE_NAME (type);

  CWriteHeader (conn, LanguageElementMType, StartDefineMType);
  foo.object_type = object_type;
  if (current_class_type)
    {
      foo.linkage = ParentLinkage;
      if (TREE_PRIVATE (type))
	foo.access = PrivateAccess;
      else if (TREE_PROTECTED (type))
	foo.access = ProtectedAccess;
      else
	foo.access = PublicAccess;
    }
  else
    {
      foo.linkage = NoLinkage;
      foo.access = PublicAccess;
    }
  if (TREE_CODE (name) == TYPE_DECL)
    name = DECL_NAME (name);

  foo.length = IDENTIFIER_LENGTH (name);

  CWriteLanguageElem (conn, &foo, IDENTIFIER_POINTER (name));
  CWriteRequestBuffer (conn);
  cadillac_process_requests (conn);
}

static void
CWriteUseObject (conn, type, object_type, use)
     Connection *conn;
     tree type;
     CObjectType object_type;
     CMessageSubType use;
{
  struct cadillac_mdep foo;
  tree name = NULL_TREE;

  CWriteHeader (conn, LanguageElementMType, use);
  foo.object_type = object_type;
  if (current_class_type)
    {
      foo.linkage = ParentLinkage;
      if (TREE_PRIVATE (type))
	foo.access = PrivateAccess;
      else if (TREE_PROTECTED (type))
	foo.access = ProtectedAccess;
      else
	foo.access = PublicAccess;
    }
  else
    {
      foo.linkage = NoLinkage;
      foo.access = PublicAccess;
    }
  switch (TREE_CODE (type))
    {
    case VAR_DECL:
    case FIELD_DECL:
    case TYPE_DECL:
    case CONST_DECL:
    case FUNCTION_DECL:
      name = DECL_NAME (type);
      break;

    default:
      my_friendly_abort (33);
  }

  foo.length = IDENTIFIER_LENGTH (name);

  CWriteLanguageElem (conn, &foo, IDENTIFIER_POINTER (name));
  CWriteRequestBuffer (conn);
  cadillac_process_requests (conn);
}

/* Here's how we exit under cadillac.  */

static void
exit_cadillac ()
{
  extern int errorcount;

  Connection *conn = cadillacObj.conn;

  if (flag_cadillac)
    {
      CCompilerMessage *req;

      CWriteHeader (conn, FinishedMType,
		    errorcount ? 0 : CsObjectWritten | CsComplete);
      /* Bye, bye!  */
      CWriteRequestBuffer (conn);

      /* Block on read.  */
      while (! readable_p (cadillacObj.fd_input))
	{
	  if (exiting)
	    my_friendly_abort (34);
	  exiting = 1;
	}
      exiting = 1;

      req = CReadCompilerMessage (conn);
      cadillac_process_request (&cadillacObj, req);
    }
}

#else
/* Stubs.  */
void init_cadillac () {}
void cadillac_start () {}
void cadillac_start_decl (decl)
     tree decl;
{}
void
cadillac_finish_decl (decl)
     tree decl;
{}
void
cadillac_start_function (fndecl)
     tree fndecl;
{}
void
cadillac_finish_function (fndecl)
     tree fndecl;
{}
void
cadillac_finish_anon_union (decl)
     tree decl;
{}
void
cadillac_start_enum (type)
     tree type;
{}
void
cadillac_finish_enum (type)
     tree type;
{}
void
cadillac_start_struct (type)
     tree type;
{}
void
cadillac_finish_struct (type)
     tree type;
{}
void
cadillac_finish_exception (type)
     tree type;
{}
void
cadillac_push_class (type)
     tree type;
{}
void
cadillac_pop_class ()
{}
void
cadillac_push_lang (name)
     tree name;
{}
void
cadillac_pop_lang ()
{}
void
cadillac_note_source ()
{}
void
cadillac_finish_stmt ()
{}
void
cadillac_switch_source ()
{}
void
cadillac_push_source ()
{}
void
cadillac_pop_source ()
{}
#endif
