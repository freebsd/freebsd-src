/* Parser for linespec for the GNU debugger, GDB.
   Copyright 1986, 1987, 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995,
   1996, 1997, 1998, 1999, 2000, 2001
   Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "symtab.h"
#include "frame.h"
#include "command.h"
#include "symfile.h"
#include "objfiles.h"
#include "demangle.h"
#include "value.h"
#include "completer.h"
#include "cp-abi.h"

/* Prototype for one function in parser-defs.h,
   instead of including that entire file. */

extern char *find_template_name_end (char *);

/* We share this one with symtab.c, but it is not exported widely. */

extern char *operator_chars (char *, char **);

/* Prototypes for local functions */

static void cplusplus_error (const char *name, const char *fmt, ...) ATTR_FORMAT (printf, 2, 3);

static int total_number_of_methods (struct type *type);

static int find_methods (struct type *, char *, struct symbol **);

static void build_canonical_line_spec (struct symtab_and_line *,
				       char *, char ***);

static char *find_toplevel_char (char *s, char c);

static struct symtabs_and_lines decode_line_2 (struct symbol *[],
					       int, int, char ***);

/* Helper functions. */

/* Issue a helpful hint on using the command completion feature on
   single quoted demangled C++ symbols as part of the completion
   error.  */

static void
cplusplus_error (const char *name, const char *fmt, ...)
{
  struct ui_file *tmp_stream;
  tmp_stream = mem_fileopen ();
  make_cleanup_ui_file_delete (tmp_stream);

  {
    va_list args;
    va_start (args, fmt);
    vfprintf_unfiltered (tmp_stream, fmt, args);
    va_end (args);
  }

  while (*name == '\'')
    name++;
  fprintf_unfiltered (tmp_stream,
		      ("Hint: try '%s<TAB> or '%s<ESC-?>\n"
		       "(Note leading single quote.)"),
		      name, name);
  error_stream (tmp_stream);
}

/* Return the number of methods described for TYPE, including the
   methods from types it derives from. This can't be done in the symbol
   reader because the type of the baseclass might still be stubbed
   when the definition of the derived class is parsed.  */

static int
total_number_of_methods (struct type *type)
{
  int n;
  int count;

  CHECK_TYPEDEF (type);
  if (TYPE_CPLUS_SPECIFIC (type) == NULL)
    return 0;
  count = TYPE_NFN_FIELDS_TOTAL (type);

  for (n = 0; n < TYPE_N_BASECLASSES (type); n++)
    count += total_number_of_methods (TYPE_BASECLASS (type, n));

  return count;
}

/* Recursive helper function for decode_line_1.
   Look for methods named NAME in type T.
   Return number of matches.
   Put matches in SYM_ARR, which should have been allocated with
   a size of total_number_of_methods (T) * sizeof (struct symbol *).
   Note that this function is g++ specific.  */

static int
find_methods (struct type *t, char *name, struct symbol **sym_arr)
{
  int i1 = 0;
  int ibase;
  char *class_name = type_name_no_tag (t);

  /* Ignore this class if it doesn't have a name.  This is ugly, but
     unless we figure out how to get the physname without the name of
     the class, then the loop can't do any good.  */
  if (class_name
      && (lookup_symbol (class_name, (struct block *) NULL,
			 STRUCT_NAMESPACE, (int *) NULL,
			 (struct symtab **) NULL)))
    {
      int method_counter;

      CHECK_TYPEDEF (t);

      /* Loop over each method name.  At this level, all overloads of a name
         are counted as a single name.  There is an inner loop which loops over
         each overload.  */

      for (method_counter = TYPE_NFN_FIELDS (t) - 1;
	   method_counter >= 0;
	   --method_counter)
	{
	  int field_counter;
	  char *method_name = TYPE_FN_FIELDLIST_NAME (t, method_counter);
	  char dem_opname[64];

	  if (strncmp (method_name, "__", 2) == 0 ||
	      strncmp (method_name, "op", 2) == 0 ||
	      strncmp (method_name, "type", 4) == 0)
	    {
	      if (cplus_demangle_opname (method_name, dem_opname, DMGL_ANSI))
		method_name = dem_opname;
	      else if (cplus_demangle_opname (method_name, dem_opname, 0))
		method_name = dem_opname;
	    }

	  if (strcmp_iw (name, method_name) == 0)
	    /* Find all the overloaded methods with that name.  */
	    for (field_counter = TYPE_FN_FIELDLIST_LENGTH (t, method_counter) - 1;
		 field_counter >= 0;
		 --field_counter)
	      {
		struct fn_field *f;
		char *phys_name;

		f = TYPE_FN_FIELDLIST1 (t, method_counter);

		if (TYPE_FN_FIELD_STUB (f, field_counter))
		  {
		    char *tmp_name;

		    tmp_name = gdb_mangle_name (t,
						method_counter,
						field_counter);
		    phys_name = alloca (strlen (tmp_name) + 1);
		    strcpy (phys_name, tmp_name);
		    xfree (tmp_name);
		  }
		else
		  phys_name = TYPE_FN_FIELD_PHYSNAME (f, field_counter);
		
		/* Destructor is handled by caller, dont add it to the list */
		if (is_destructor_name (phys_name) != 0)
		  continue;

		sym_arr[i1] = lookup_symbol (phys_name,
					     NULL, VAR_NAMESPACE,
					     (int *) NULL,
					     (struct symtab **) NULL);
		if (sym_arr[i1])
		  i1++;
		else
		  {
		    /* This error message gets printed, but the method
		       still seems to be found
		       fputs_filtered("(Cannot find method ", gdb_stdout);
		       fprintf_symbol_filtered (gdb_stdout, phys_name,
		       language_cplus,
		       DMGL_PARAMS | DMGL_ANSI);
		       fputs_filtered(" - possibly inlined.)\n", gdb_stdout);
		     */
		  }
	      }
	  else if (strcmp_iw (class_name, name) == 0)
	    {
	      /* For GCC 3.x and stabs, constructors and destructors have names
		 like __base_ctor and __complete_dtor.  Check the physname for now
		 if we're looking for a constructor.  */
	      for (field_counter
		     = TYPE_FN_FIELDLIST_LENGTH (t, method_counter) - 1;
		   field_counter >= 0;
		   --field_counter)
		{
		  struct fn_field *f;
		  char *phys_name;
		  
		  f = TYPE_FN_FIELDLIST1 (t, method_counter);

		  /* GCC 3.x will never produce stabs stub methods, so we don't need
		     to handle this case.  */
		  if (TYPE_FN_FIELD_STUB (f, field_counter))
		    continue;
		  phys_name = TYPE_FN_FIELD_PHYSNAME (f, field_counter);
		  if (! is_constructor_name (phys_name))
		    continue;

		  /* If this method is actually defined, include it in the
		     list.  */
		  sym_arr[i1] = lookup_symbol (phys_name,
					       NULL, VAR_NAMESPACE,
					       (int *) NULL,
					       (struct symtab **) NULL);
		  if (sym_arr[i1])
		    i1++;
		}
	    }
	}
    }

  /* Only search baseclasses if there is no match yet, since names in
     derived classes override those in baseclasses.

     FIXME: The above is not true; it is only true of member functions
     if they have the same number of arguments (??? - section 13.1 of the
     ARM says the function members are not in the same scope but doesn't
     really spell out the rules in a way I understand.  In any case, if
     the number of arguments differ this is a case in which we can overload
     rather than hiding without any problem, and gcc 2.4.5 does overload
     rather than hiding in this case).  */

  if (i1 == 0)
    for (ibase = 0; ibase < TYPE_N_BASECLASSES (t); ibase++)
      i1 += find_methods (TYPE_BASECLASS (t, ibase), name, sym_arr + i1);

  return i1;
}

/* Helper function for decode_line_1.
   Build a canonical line spec in CANONICAL if it is non-NULL and if
   the SAL has a symtab.
   If SYMNAME is non-NULL the canonical line spec is `filename:symname'.
   If SYMNAME is NULL the line number from SAL is used and the canonical
   line spec is `filename:linenum'.  */

static void
build_canonical_line_spec (struct symtab_and_line *sal, char *symname,
			   char ***canonical)
{
  char **canonical_arr;
  char *canonical_name;
  char *filename;
  struct symtab *s = sal->symtab;

  if (s == (struct symtab *) NULL
      || s->filename == (char *) NULL
      || canonical == (char ***) NULL)
    return;

  canonical_arr = (char **) xmalloc (sizeof (char *));
  *canonical = canonical_arr;

  filename = s->filename;
  if (symname != NULL)
    {
      canonical_name = xmalloc (strlen (filename) + strlen (symname) + 2);
      sprintf (canonical_name, "%s:%s", filename, symname);
    }
  else
    {
      canonical_name = xmalloc (strlen (filename) + 30);
      sprintf (canonical_name, "%s:%d", filename, sal->line);
    }
  canonical_arr[0] = canonical_name;
}



/* Find an instance of the character C in the string S that is outside
   of all parenthesis pairs, single-quoted strings, and double-quoted
   strings.  */
static char *
find_toplevel_char (char *s, char c)
{
  int quoted = 0;		/* zero if we're not in quotes;
				   '"' if we're in a double-quoted string;
				   '\'' if we're in a single-quoted string.  */
  int depth = 0;		/* number of unclosed parens we've seen */
  char *scan;

  for (scan = s; *scan; scan++)
    {
      if (quoted)
	{
	  if (*scan == quoted)
	    quoted = 0;
	  else if (*scan == '\\' && *(scan + 1))
	    scan++;
	}
      else if (*scan == c && ! quoted && depth == 0)
	return scan;
      else if (*scan == '"' || *scan == '\'')
	quoted = *scan;
      else if (*scan == '(')
	depth++;
      else if (*scan == ')' && depth > 0)
	depth--;
    }

  return 0;
}

/* Given a list of NELTS symbols in SYM_ARR, return a list of lines to
   operate on (ask user if necessary).
   If CANONICAL is non-NULL return a corresponding array of mangled names
   as canonical line specs there.  */

static struct symtabs_and_lines
decode_line_2 (struct symbol *sym_arr[], int nelts, int funfirstline,
	       char ***canonical)
{
  struct symtabs_and_lines values, return_values;
  char *args, *arg1;
  int i;
  char *prompt;
  char *symname;
  struct cleanup *old_chain;
  char **canonical_arr = (char **) NULL;

  values.sals = (struct symtab_and_line *)
    alloca (nelts * sizeof (struct symtab_and_line));
  return_values.sals = (struct symtab_and_line *)
    xmalloc (nelts * sizeof (struct symtab_and_line));
  old_chain = make_cleanup (xfree, return_values.sals);

  if (canonical)
    {
      canonical_arr = (char **) xmalloc (nelts * sizeof (char *));
      make_cleanup (xfree, canonical_arr);
      memset (canonical_arr, 0, nelts * sizeof (char *));
      *canonical = canonical_arr;
    }

  i = 0;
  printf_unfiltered ("[0] cancel\n[1] all\n");
  while (i < nelts)
    {
      INIT_SAL (&return_values.sals[i]);	/* initialize to zeroes */
      INIT_SAL (&values.sals[i]);
      if (sym_arr[i] && SYMBOL_CLASS (sym_arr[i]) == LOC_BLOCK)
	{
	  values.sals[i] = find_function_start_sal (sym_arr[i], funfirstline);
	  printf_unfiltered ("[%d] %s at %s:%d\n",
			     (i + 2),
			     SYMBOL_SOURCE_NAME (sym_arr[i]),
			     values.sals[i].symtab->filename,
			     values.sals[i].line);
	}
      else
	printf_unfiltered ("?HERE\n");
      i++;
    }

  if ((prompt = getenv ("PS2")) == NULL)
    {
      prompt = "> ";
    }
  args = command_line_input (prompt, 0, "overload-choice");

  if (args == 0 || *args == 0)
    error_no_arg ("one or more choice numbers");

  i = 0;
  while (*args)
    {
      int num;

      arg1 = args;
      while (*arg1 >= '0' && *arg1 <= '9')
	arg1++;
      if (*arg1 && *arg1 != ' ' && *arg1 != '\t')
	error ("Arguments must be choice numbers.");

      num = atoi (args);

      if (num == 0)
	error ("canceled");
      else if (num == 1)
	{
	  if (canonical_arr)
	    {
	      for (i = 0; i < nelts; i++)
		{
		  if (canonical_arr[i] == NULL)
		    {
		      symname = SYMBOL_NAME (sym_arr[i]);
		      canonical_arr[i] = savestring (symname, strlen (symname));
		    }
		}
	    }
	  memcpy (return_values.sals, values.sals,
		  (nelts * sizeof (struct symtab_and_line)));
	  return_values.nelts = nelts;
	  discard_cleanups (old_chain);
	  return return_values;
	}

      if (num >= nelts + 2)
	{
	  printf_unfiltered ("No choice number %d.\n", num);
	}
      else
	{
	  num -= 2;
	  if (values.sals[num].pc)
	    {
	      if (canonical_arr)
		{
		  symname = SYMBOL_NAME (sym_arr[num]);
		  make_cleanup (xfree, symname);
		  canonical_arr[i] = savestring (symname, strlen (symname));
		}
	      return_values.sals[i++] = values.sals[num];
	      values.sals[num].pc = 0;
	    }
	  else
	    {
	      printf_unfiltered ("duplicate request for %d ignored.\n", num);
	    }
	}

      args = arg1;
      while (*args == ' ' || *args == '\t')
	args++;
    }
  return_values.nelts = i;
  discard_cleanups (old_chain);
  return return_values;
}

/* The parser of linespec itself. */

/* Parse a string that specifies a line number.
   Pass the address of a char * variable; that variable will be
   advanced over the characters actually parsed.

   The string can be:

   LINENUM -- that line number in current file.  PC returned is 0.
   FILE:LINENUM -- that line in that file.  PC returned is 0.
   FUNCTION -- line number of openbrace of that function.
   PC returned is the start of the function.
   VARIABLE -- line number of definition of that variable.
   PC returned is 0.
   FILE:FUNCTION -- likewise, but prefer functions in that file.
   *EXPR -- line in which address EXPR appears.

   This may all be followed by an "if EXPR", which we ignore.

   FUNCTION may be an undebuggable function found in minimal symbol table.

   If the argument FUNFIRSTLINE is nonzero, we want the first line
   of real code inside a function when a function is specified, and it is
   not OK to specify a variable or type to get its line number.

   DEFAULT_SYMTAB specifies the file to use if none is specified.
   It defaults to current_source_symtab.
   DEFAULT_LINE specifies the line number to use for relative
   line numbers (that start with signs).  Defaults to current_source_line.
   If CANONICAL is non-NULL, store an array of strings containing the canonical
   line specs there if necessary. Currently overloaded member functions and
   line numbers or static functions without a filename yield a canonical
   line spec. The array and the line spec strings are allocated on the heap,
   it is the callers responsibility to free them.

   Note that it is possible to return zero for the symtab
   if no file is validly specified.  Callers must check that.
   Also, the line number returned may be invalid.  */

/* We allow single quotes in various places.  This is a hideous
   kludge, which exists because the completer can't yet deal with the
   lack of single quotes.  FIXME: write a linespec_completer which we
   can use as appropriate instead of make_symbol_completion_list.  */

struct symtabs_and_lines
decode_line_1 (char **argptr, int funfirstline, struct symtab *default_symtab,
	       int default_line, char ***canonical)
{
  struct symtabs_and_lines values;
  struct symtab_and_line val;
  register char *p, *p1;
  char *q, *pp, *ii, *p2;
#if 0
  char *q1;
#endif
  register struct symtab *s;

  register struct symbol *sym;
  /* The symtab that SYM was found in.  */
  struct symtab *sym_symtab;

  register CORE_ADDR pc;
  register struct minimal_symbol *msymbol;
  char *copy;
  struct symbol *sym_class;
  int i1;
  int is_quoted;
  int is_quote_enclosed;
  int has_parens;
  int has_if = 0;
  int has_comma = 0;
  struct symbol **sym_arr;
  struct type *t;
  char *saved_arg = *argptr;
  extern char *gdb_completer_quote_characters;

  INIT_SAL (&val);		/* initialize to zeroes */

  /* Defaults have defaults.  */

  if (default_symtab == 0)
    {
      default_symtab = current_source_symtab;
      default_line = current_source_line;
    }

  /* See if arg is *PC */

  if (**argptr == '*')
    {
      (*argptr)++;
      pc = parse_and_eval_address_1 (argptr);

      values.sals = (struct symtab_and_line *)
	xmalloc (sizeof (struct symtab_and_line));

      values.nelts = 1;
      values.sals[0] = find_pc_line (pc, 0);
      values.sals[0].pc = pc;
      values.sals[0].section = find_pc_overlay (pc);

      return values;
    }

  /* 'has_if' is for the syntax:
   *     (gdb) break foo if (a==b)
   */
  if ((ii = strstr (*argptr, " if ")) != NULL ||
      (ii = strstr (*argptr, "\tif ")) != NULL ||
      (ii = strstr (*argptr, " if\t")) != NULL ||
      (ii = strstr (*argptr, "\tif\t")) != NULL ||
      (ii = strstr (*argptr, " if(")) != NULL ||
      (ii = strstr (*argptr, "\tif( ")) != NULL)
    has_if = 1;
  /* Temporarily zap out "if (condition)" to not
   * confuse the parenthesis-checking code below.
   * This is undone below. Do not change ii!!
   */
  if (has_if)
    {
      *ii = '\0';
    }

  /* Set various flags.
   * 'has_parens' is important for overload checking, where
   * we allow things like: 
   *     (gdb) break c::f(int)
   */

  /* Maybe arg is FILE : LINENUM or FILE : FUNCTION */

  is_quoted = (**argptr
	       && strchr (get_gdb_completer_quote_characters (),
			  **argptr) != NULL);

  has_parens = ((pp = strchr (*argptr, '(')) != NULL
		&& (pp = strrchr (pp, ')')) != NULL);

  /* Now that we're safely past the has_parens check,
   * put back " if (condition)" so outer layers can see it 
   */
  if (has_if)
    *ii = ' ';

  /* Maybe we were called with a line range FILENAME:LINENUM,FILENAME:LINENUM
     and we must isolate the first half.  Outer layers will call again later
     for the second half.

     Don't count commas that appear in argument lists of overloaded
     functions, or in quoted strings.  It's stupid to go to this much
     trouble when the rest of the function is such an obvious roach hotel.  */
  ii = find_toplevel_char (*argptr, ',');
  has_comma = (ii != 0);

  /* Temporarily zap out second half to not
   * confuse the code below.
   * This is undone below. Do not change ii!!
   */
  if (has_comma)
    {
      *ii = '\0';
    }

  /* Maybe arg is FILE : LINENUM or FILE : FUNCTION */
  /* May also be CLASS::MEMBER, or NAMESPACE::NAME */
  /* Look for ':', but ignore inside of <> */

  s = NULL;
  p = *argptr;
  if (p[0] == '"')
    {
      is_quote_enclosed = 1;
      (*argptr)++;
      p++;
    }
  else
    is_quote_enclosed = 0;
  for (; *p; p++)
    {
      if (p[0] == '<')
	{
	  char *temp_end = find_template_name_end (p);
	  if (!temp_end)
	    error ("malformed template specification in command");
	  p = temp_end;
	}
      /* Check for the end of the first half of the linespec.  End of line,
         a tab, a double colon or the last single colon, or a space.  But
         if enclosed in double quotes we do not break on enclosed spaces */
      if (!*p
	  || p[0] == '\t'
	  || ((p[0] == ':')
	      && ((p[1] == ':') || (strchr (p + 1, ':') == NULL)))
	  || ((p[0] == ' ') && !is_quote_enclosed))
	break;
      if (p[0] == '.' && strchr (p, ':') == NULL)	/* Java qualified method. */
	{
	  /* Find the *last* '.', since the others are package qualifiers. */
	  for (p1 = p; *p1; p1++)
	    {
	      if (*p1 == '.')
		p = p1;
	    }
	  break;
	}
    }
  while (p[0] == ' ' || p[0] == '\t')
    p++;

  /* if the closing double quote was left at the end, remove it */
  if (is_quote_enclosed)
    {
      char *closing_quote = strchr (p - 1, '"');
      if (closing_quote && closing_quote[1] == '\0')
	*closing_quote = '\0';
    }

  /* Now that we've safely parsed the first half,
   * put back ',' so outer layers can see it 
   */
  if (has_comma)
    *ii = ',';

  if ((p[0] == ':' || p[0] == '.') && !has_parens)
    {
      /*  C++ */
      /*  ... or Java */
      if (is_quoted)
	*argptr = *argptr + 1;
      if (p[0] == '.' || p[1] == ':')
	{
	  char *saved_arg2 = *argptr;
	  char *temp_end;
	  /* First check for "global" namespace specification,
	     of the form "::foo". If found, skip over the colons
	     and jump to normal symbol processing */
	  if (p[0] == ':' 
	      && ((*argptr == p) || (p[-1] == ' ') || (p[-1] == '\t')))
	    saved_arg2 += 2;

	  /* We have what looks like a class or namespace
	     scope specification (A::B), possibly with many
	     levels of namespaces or classes (A::B::C::D).

	     Some versions of the HP ANSI C++ compiler (as also possibly
	     other compilers) generate class/function/member names with
	     embedded double-colons if they are inside namespaces. To
	     handle this, we loop a few times, considering larger and
	     larger prefixes of the string as though they were single
	     symbols.  So, if the initially supplied string is
	     A::B::C::D::foo, we have to look up "A", then "A::B",
	     then "A::B::C", then "A::B::C::D", and finally
	     "A::B::C::D::foo" as single, monolithic symbols, because
	     A, B, C or D may be namespaces.

	     Note that namespaces can nest only inside other
	     namespaces, and not inside classes.  So we need only
	     consider *prefixes* of the string; there is no need to look up
	     "B::C" separately as a symbol in the previous example. */

	  p2 = p;		/* save for restart */
	  while (1)
	    {
	      /* Extract the class name.  */
	      p1 = p;
	      while (p != *argptr && p[-1] == ' ')
		--p;
	      copy = (char *) alloca (p - *argptr + 1);
	      memcpy (copy, *argptr, p - *argptr);
	      copy[p - *argptr] = 0;

	      /* Discard the class name from the arg.  */
	      p = p1 + (p1[0] == ':' ? 2 : 1);
	      while (*p == ' ' || *p == '\t')
		p++;
	      *argptr = p;

	      sym_class = lookup_symbol (copy, 0, STRUCT_NAMESPACE, 0,
					 (struct symtab **) NULL);

	      if (sym_class &&
		  (t = check_typedef (SYMBOL_TYPE (sym_class)),
		   (TYPE_CODE (t) == TYPE_CODE_STRUCT
		    || TYPE_CODE (t) == TYPE_CODE_UNION)))
		{
		  /* Arg token is not digits => try it as a function name
		     Find the next token(everything up to end or next blank). */
		  if (**argptr
		      && strchr (get_gdb_completer_quote_characters (),
				 **argptr) != NULL)
		    {
		      p = skip_quoted (*argptr);
		      *argptr = *argptr + 1;
		    }
		  else
		    {
		      p = *argptr;
		      while (*p && *p != ' ' && *p != '\t' && *p != ',' && *p != ':')
			p++;
		    }
/*
   q = operator_chars (*argptr, &q1);
   if (q1 - q)
   {
   char *opname;
   char *tmp = alloca (q1 - q + 1);
   memcpy (tmp, q, q1 - q);
   tmp[q1 - q] = '\0';
   opname = cplus_mangle_opname (tmp, DMGL_ANSI);
   if (opname == NULL)
   {
   cplusplus_error (saved_arg, "no mangling for \"%s\"\n", tmp);
   }
   copy = (char*) alloca (3 + strlen(opname));
   sprintf (copy, "__%s", opname);
   p = q1;
   }
   else
 */
		  {
		    copy = (char *) alloca (p - *argptr + 1);
		    memcpy (copy, *argptr, p - *argptr);
		    copy[p - *argptr] = '\0';
		    if (p != *argptr
			&& copy[p - *argptr - 1]
			&& strchr (get_gdb_completer_quote_characters (),
				   copy[p - *argptr - 1]) != NULL)
		      copy[p - *argptr - 1] = '\0';
		  }

		  /* no line number may be specified */
		  while (*p == ' ' || *p == '\t')
		    p++;
		  *argptr = p;

		  sym = 0;
		  i1 = 0;	/*  counter for the symbol array */
		  sym_arr = (struct symbol **) alloca (total_number_of_methods (t)
						* sizeof (struct symbol *));

		  if (destructor_name_p (copy, t))
		    {
		      /* Destructors are a special case.  */
		      int m_index, f_index;

		      if (get_destructor_fn_field (t, &m_index, &f_index))
			{
			  struct fn_field *f = TYPE_FN_FIELDLIST1 (t, m_index);

			  sym_arr[i1] =
			    lookup_symbol (TYPE_FN_FIELD_PHYSNAME (f, f_index),
					   NULL, VAR_NAMESPACE, (int *) NULL,
					   (struct symtab **) NULL);
			  if (sym_arr[i1])
			    i1++;
			}
		    }
		  else
		    i1 = find_methods (t, copy, sym_arr);
		  if (i1 == 1)
		    {
		      /* There is exactly one field with that name.  */
		      sym = sym_arr[0];

		      if (sym && SYMBOL_CLASS (sym) == LOC_BLOCK)
			{
			  values.sals = (struct symtab_and_line *)
			    xmalloc (sizeof (struct symtab_and_line));
			  values.nelts = 1;
			  values.sals[0] = find_function_start_sal (sym,
							      funfirstline);
			}
		      else
			{
			  values.nelts = 0;
			}
		      return values;
		    }
		  if (i1 > 0)
		    {
		      /* There is more than one field with that name
		         (overloaded).  Ask the user which one to use.  */
		      return decode_line_2 (sym_arr, i1, funfirstline, canonical);
		    }
		  else
		    {
		      char *tmp;

		      if (is_operator_name (copy))
			{
			  tmp = (char *) alloca (strlen (copy + 3) + 9);
			  strcpy (tmp, "operator ");
			  strcat (tmp, copy + 3);
			}
		      else
			tmp = copy;
		      if (tmp[0] == '~')
			cplusplus_error (saved_arg,
					 "the class `%s' does not have destructor defined\n",
					 SYMBOL_SOURCE_NAME (sym_class));
		      else
			cplusplus_error (saved_arg,
					 "the class %s does not have any method named %s\n",
					 SYMBOL_SOURCE_NAME (sym_class), tmp);
		    }
		}

	      /* Move pointer up to next possible class/namespace token */
	      p = p2 + 1;	/* restart with old value +1 */
	      /* Move pointer ahead to next double-colon */
	      while (*p && (p[0] != ' ') && (p[0] != '\t') && (p[0] != '\''))
		{
		  if (p[0] == '<')
		    {
		      temp_end = find_template_name_end (p);
		      if (!temp_end)
			error ("malformed template specification in command");
		      p = temp_end;
		    }
		  else if ((p[0] == ':') && (p[1] == ':'))
		    break;	/* found double-colon */
		  else
		    p++;
		}

	      if (*p != ':')
		break;		/* out of the while (1) */

	      p2 = p;		/* save restart for next time around */
	      *argptr = saved_arg2;	/* restore argptr */
	    }			/* while (1) */

	  /* Last chance attempt -- check entire name as a symbol */
	  /* Use "copy" in preparation for jumping out of this block,
	     to be consistent with usage following the jump target */
	  copy = (char *) alloca (p - saved_arg2 + 1);
	  memcpy (copy, saved_arg2, p - saved_arg2);
	  /* Note: if is_quoted should be true, we snuff out quote here anyway */
	  copy[p - saved_arg2] = '\000';
	  /* Set argptr to skip over the name */
	  *argptr = (*p == '\'') ? p + 1 : p;
	  /* Look up entire name */
	  sym = lookup_symbol (copy, 0, VAR_NAMESPACE, 0, &sym_symtab);
	  s = (struct symtab *) 0;
	  /* Prepare to jump: restore the " if (condition)" so outer layers see it */
	  /* Symbol was found --> jump to normal symbol processing.
	     Code following "symbol_found" expects "copy" to have the
	     symbol name, "sym" to have the symbol pointer, "s" to be
	     a specified file's symtab, and sym_symtab to be the symbol's
	     symtab. */
	  /* By jumping there we avoid falling through the FILE:LINE and
	     FILE:FUNC processing stuff below */
	  if (sym)
	    goto symbol_found;

	  /* Couldn't find any interpretation as classes/namespaces, so give up */
	  /* The quotes are important if copy is empty.  */
	  cplusplus_error (saved_arg,
			   "Can't find member of namespace, class, struct, or union named \"%s\"\n",
			   copy);
	}
      /*  end of C++  */


      /* Extract the file name.  */
      p1 = p;
      while (p != *argptr && p[-1] == ' ')
	--p;
      if ((*p == '"') && is_quote_enclosed)
	--p;
      copy = (char *) alloca (p - *argptr + 1);
      if ((**argptr == '"') && is_quote_enclosed)
	{
	  memcpy (copy, *argptr + 1, p - *argptr - 1);
	  /* It may have the ending quote right after the file name */
	  if (copy[p - *argptr - 2] == '"')
	    copy[p - *argptr - 2] = 0;
	  else
	    copy[p - *argptr - 1] = 0;
	}
      else
	{
	  memcpy (copy, *argptr, p - *argptr);
	  copy[p - *argptr] = 0;
	}

      /* Find that file's data.  */
      s = lookup_symtab (copy);
      if (s == 0)
	{
	  if (!have_full_symbols () && !have_partial_symbols ())
	    error ("No symbol table is loaded.  Use the \"file\" command.");
	  error ("No source file named %s.", copy);
	}

      /* Discard the file name from the arg.  */
      p = p1 + 1;
      while (*p == ' ' || *p == '\t')
	p++;
      *argptr = p;
    }
#if 0
  /* No one really seems to know why this was added. It certainly
     breaks the command line, though, whenever the passed
     name is of the form ClassName::Method. This bit of code
     singles out the class name, and if funfirstline is set (for
     example, you are setting a breakpoint at this function),
     you get an error. This did not occur with earlier
     verions, so I am ifdef'ing this out. 3/29/99 */
  else
    {
      /* Check if what we have till now is a symbol name */

      /* We may be looking at a template instantiation such
         as "foo<int>".  Check here whether we know about it,
         instead of falling through to the code below which
         handles ordinary function names, because that code
         doesn't like seeing '<' and '>' in a name -- the
         skip_quoted call doesn't go past them.  So see if we
         can figure it out right now. */

      copy = (char *) alloca (p - *argptr + 1);
      memcpy (copy, *argptr, p - *argptr);
      copy[p - *argptr] = '\000';
      sym = lookup_symbol (copy, 0, VAR_NAMESPACE, 0, &sym_symtab);
      if (sym)
	{
	  /* Yes, we have a symbol; jump to symbol processing */
	  /* Code after symbol_found expects S, SYM_SYMTAB, SYM, 
	     and COPY to be set correctly */
	  *argptr = (*p == '\'') ? p + 1 : p;
	  s = (struct symtab *) 0;
	  goto symbol_found;
	}
      /* Otherwise fall out from here and go to file/line spec
         processing, etc. */
    }
#endif

  /* S is specified file's symtab, or 0 if no file specified.
     arg no longer contains the file name.  */

  /* Check whether arg is all digits (and sign) */

  q = *argptr;
  if (*q == '-' || *q == '+')
    q++;
  while (*q >= '0' && *q <= '9')
    q++;

  if (q != *argptr && (*q == 0 || *q == ' ' || *q == '\t' || *q == ','))
    {
      /* We found a token consisting of all digits -- at least one digit.  */
      enum sign
	{
	  none, plus, minus
	}
      sign = none;

      /* We might need a canonical line spec if no file was specified.  */
      int need_canonical = (s == 0) ? 1 : 0;

      /* This is where we need to make sure that we have good defaults.
         We must guarantee that this section of code is never executed
         when we are called with just a function name, since
         select_source_symtab calls us with such an argument  */

      if (s == 0 && default_symtab == 0)
	{
	  select_source_symtab (0);
	  default_symtab = current_source_symtab;
	  default_line = current_source_line;
	}

      if (**argptr == '+')
	sign = plus, (*argptr)++;
      else if (**argptr == '-')
	sign = minus, (*argptr)++;
      val.line = atoi (*argptr);
      switch (sign)
	{
	case plus:
	  if (q == *argptr)
	    val.line = 5;
	  if (s == 0)
	    val.line = default_line + val.line;
	  break;
	case minus:
	  if (q == *argptr)
	    val.line = 15;
	  if (s == 0)
	    val.line = default_line - val.line;
	  else
	    val.line = 1;
	  break;
	case none:
	  break;		/* No need to adjust val.line.  */
	}

      while (*q == ' ' || *q == '\t')
	q++;
      *argptr = q;
      if (s == 0)
	s = default_symtab;

      /* It is possible that this source file has more than one symtab, 
         and that the new line number specification has moved us from the
         default (in s) to a new one.  */
      val.symtab = find_line_symtab (s, val.line, NULL, NULL);
      if (val.symtab == 0)
	val.symtab = s;

      val.pc = 0;
      values.sals = (struct symtab_and_line *)
	xmalloc (sizeof (struct symtab_and_line));
      values.sals[0] = val;
      values.nelts = 1;
      if (need_canonical)
	build_canonical_line_spec (values.sals, NULL, canonical);
      return values;
    }

  /* Arg token is not digits => try it as a variable name
     Find the next token (everything up to end or next whitespace).  */

  if (**argptr == '$')		/* May be a convenience variable */
    p = skip_quoted (*argptr + (((*argptr)[1] == '$') ? 2 : 1));	/* One or two $ chars possible */
  else if (is_quoted)
    {
      p = skip_quoted (*argptr);
      if (p[-1] != '\'')
	error ("Unmatched single quote.");
    }
  else if (has_parens)
    {
      p = pp + 1;
    }
  else
    {
      p = skip_quoted (*argptr);
    }

  copy = (char *) alloca (p - *argptr + 1);
  memcpy (copy, *argptr, p - *argptr);
  copy[p - *argptr] = '\0';
  if (p != *argptr
      && copy[0]
      && copy[0] == copy[p - *argptr - 1]
      && strchr (get_gdb_completer_quote_characters (), copy[0]) != NULL)
    {
      copy[p - *argptr - 1] = '\0';
      copy++;
    }
  while (*p == ' ' || *p == '\t')
    p++;
  *argptr = p;

  /* If it starts with $: may be a legitimate variable or routine name
     (e.g. HP-UX millicode routines such as $$dyncall), or it may
     be history value, or it may be a convenience variable */

  if (*copy == '$')
    {
      struct value *valx;
      int index = 0;
      int need_canonical = 0;

      p = (copy[1] == '$') ? copy + 2 : copy + 1;
      while (*p >= '0' && *p <= '9')
	p++;
      if (!*p)			/* reached end of token without hitting non-digit */
	{
	  /* We have a value history reference */
	  sscanf ((copy[1] == '$') ? copy + 2 : copy + 1, "%d", &index);
	  valx = access_value_history ((copy[1] == '$') ? -index : index);
	  if (TYPE_CODE (VALUE_TYPE (valx)) != TYPE_CODE_INT)
	    error ("History values used in line specs must have integer values.");
	}
      else
	{
	  /* Not all digits -- may be user variable/function or a
	     convenience variable */

	  /* Look up entire name as a symbol first */
	  sym = lookup_symbol (copy, 0, VAR_NAMESPACE, 0, &sym_symtab);
	  s = (struct symtab *) 0;
	  need_canonical = 1;
	  /* Symbol was found --> jump to normal symbol processing.
	     Code following "symbol_found" expects "copy" to have the
	     symbol name, "sym" to have the symbol pointer, "s" to be
	     a specified file's symtab, and sym_symtab to be the symbol's
	     symtab. */
	  if (sym)
	    goto symbol_found;

	  /* If symbol was not found, look in minimal symbol tables */
	  msymbol = lookup_minimal_symbol (copy, NULL, NULL);
	  /* Min symbol was found --> jump to minsym processing. */
	  if (msymbol)
	    goto minimal_symbol_found;

	  /* Not a user variable or function -- must be convenience variable */
	  need_canonical = (s == 0) ? 1 : 0;
	  valx = value_of_internalvar (lookup_internalvar (copy + 1));
	  if (TYPE_CODE (VALUE_TYPE (valx)) != TYPE_CODE_INT)
	    error ("Convenience variables used in line specs must have integer values.");
	}

      /* Either history value or convenience value from above, in valx */
      val.symtab = s ? s : default_symtab;
      val.line = value_as_long (valx);
      val.pc = 0;

      values.sals = (struct symtab_and_line *) xmalloc (sizeof val);
      values.sals[0] = val;
      values.nelts = 1;

      if (need_canonical)
	build_canonical_line_spec (values.sals, NULL, canonical);

      return values;
    }


  /* Look up that token as a variable.
     If file specified, use that file's per-file block to start with.  */

  sym = lookup_symbol (copy,
		       (s ? BLOCKVECTOR_BLOCK (BLOCKVECTOR (s), STATIC_BLOCK)
			: get_selected_block ()),
		       VAR_NAMESPACE, 0, &sym_symtab);

symbol_found:			/* We also jump here from inside the C++ class/namespace 
				   code on finding a symbol of the form "A::B::C" */

  if (sym != NULL)
    {
      if (SYMBOL_CLASS (sym) == LOC_BLOCK)
	{
	  /* Arg is the name of a function */
	  values.sals = (struct symtab_and_line *)
	    xmalloc (sizeof (struct symtab_and_line));
	  values.sals[0] = find_function_start_sal (sym, funfirstline);
	  values.nelts = 1;

	  /* Don't use the SYMBOL_LINE; if used at all it points to
	     the line containing the parameters or thereabouts, not
	     the first line of code.  */

	  /* We might need a canonical line spec if it is a static
	     function.  */
	  if (s == 0)
	    {
	      struct blockvector *bv = BLOCKVECTOR (sym_symtab);
	      struct block *b = BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK);
	      if (lookup_block_symbol (b, copy, VAR_NAMESPACE) != NULL)
		build_canonical_line_spec (values.sals, copy, canonical);
	    }
	  return values;
	}
      else
	{
	  if (funfirstline)
	    error ("\"%s\" is not a function", copy);
	  else if (SYMBOL_LINE (sym) != 0)
	    {
	      /* We know its line number.  */
	      values.sals = (struct symtab_and_line *)
		xmalloc (sizeof (struct symtab_and_line));
	      values.nelts = 1;
	      memset (&values.sals[0], 0, sizeof (values.sals[0]));
	      values.sals[0].symtab = sym_symtab;
	      values.sals[0].line = SYMBOL_LINE (sym);
	      return values;
	    }
	  else
	    /* This can happen if it is compiled with a compiler which doesn't
	       put out line numbers for variables.  */
	    /* FIXME: Shouldn't we just set .line and .symtab to zero
	       and return?  For example, "info line foo" could print
	       the address.  */
	    error ("Line number not known for symbol \"%s\"", copy);
	}
    }

  msymbol = lookup_minimal_symbol (copy, NULL, NULL);

minimal_symbol_found:		/* We also jump here from the case for variables
				   that begin with '$' */

  if (msymbol != NULL)
    {
      values.sals = (struct symtab_and_line *)
	xmalloc (sizeof (struct symtab_and_line));
      values.sals[0] = find_pc_sect_line (SYMBOL_VALUE_ADDRESS (msymbol),
					  (struct sec *) 0, 0);
      values.sals[0].section = SYMBOL_BFD_SECTION (msymbol);
      if (funfirstline)
	{
	  values.sals[0].pc += FUNCTION_START_OFFSET;
	  values.sals[0].pc = SKIP_PROLOGUE (values.sals[0].pc);
	}
      values.nelts = 1;
      return values;
    }

  if (!have_full_symbols () &&
      !have_partial_symbols () && !have_minimal_symbols ())
    error ("No symbol table is loaded.  Use the \"file\" command.");

  error ("Function \"%s\" not defined.", copy);
  return values;		/* for lint */
}
