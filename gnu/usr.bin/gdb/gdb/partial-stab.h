/* Shared code to pre-read a stab (dbx-style), when building a psymtab.
   Copyright 1986, 1987, 1988, 1989, 1990, 1991, 1992, 1993
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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* The following need to be defined:
   SET_NAMESTRING() --Set namestring to name of symbol.
   CUR_SYMBOL_TYPE --Type code of current symbol.
   CUR_SYMBOL_VALUE --Value field of current symbol.  May be adjusted here.
 */

/* End of macro definitions, now let's handle them symbols!  */

      switch (CUR_SYMBOL_TYPE)
	{
	  char *p;
	  /*
	   * Standard, external, non-debugger, symbols
	   */

	case N_TEXT | N_EXT:
	case N_NBTEXT | N_EXT:
	  CUR_SYMBOL_VALUE += ANOFFSET (section_offsets, SECT_OFF_TEXT);
	  goto record_it;

	case N_DATA | N_EXT:
	case N_NBDATA | N_EXT:
	  CUR_SYMBOL_VALUE += ANOFFSET (section_offsets, SECT_OFF_DATA);
	  goto record_it;

	case N_BSS:
	case N_BSS | N_EXT:
	case N_NBBSS | N_EXT:
        case N_SETV | N_EXT:		/* FIXME, is this in BSS? */
	  CUR_SYMBOL_VALUE += ANOFFSET (section_offsets, SECT_OFF_BSS);
	  goto record_it;

	case N_ABS | N_EXT:
	record_it:
#ifdef DBXREAD_ONLY
	  SET_NAMESTRING();

	bss_ext_symbol:
	  record_minimal_symbol (namestring, CUR_SYMBOL_VALUE,
				 CUR_SYMBOL_TYPE, objfile); /* Always */
#endif /* DBXREAD_ONLY */
	  continue;

	  /* Standard, local, non-debugger, symbols */

	case N_NBTEXT:

	  /* We need to be able to deal with both N_FN or N_TEXT,
	     because we have no way of knowing whether the sys-supplied ld
	     or GNU ld was used to make the executable.  Sequents throw
	     in another wrinkle -- they renumbered N_FN.  */

	case N_FN:
	case N_FN_SEQ:
	case N_TEXT:
#ifdef DBXREAD_ONLY
	  CUR_SYMBOL_VALUE += ANOFFSET (section_offsets, SECT_OFF_TEXT);
	  SET_NAMESTRING();
	  if ((namestring[0] == '-' && namestring[1] == 'l')
	      || (namestring [(nsl = strlen (namestring)) - 1] == 'o'
		  && namestring [nsl - 2] == '.')
#ifdef GDB_TARGET_IS_HPPA
              /* some cooperation from gcc to get around ld stupidity */
              || (namestring[0] == 'e' && STREQ (namestring, "end_file."))
#endif
	      )
	    {
#ifndef GDB_TARGET_IS_HPPA
	      if (objfile -> ei.entry_point <  CUR_SYMBOL_VALUE &&
		  objfile -> ei.entry_point >= last_o_file_start)
		{
		  objfile -> ei.entry_file_lowpc = last_o_file_start;
		  objfile -> ei.entry_file_highpc = CUR_SYMBOL_VALUE;
		}
#endif
	      if (past_first_source_file && pst
		  /* The gould NP1 uses low values for .o and -l symbols
		     which are not the address.  */
		  && CUR_SYMBOL_VALUE >= pst->textlow)
		{
		  END_PSYMTAB (pst, psymtab_include_list, includes_used,
			       symnum * symbol_size, CUR_SYMBOL_VALUE,
			       dependency_list, dependencies_used);
		  pst = (struct partial_symtab *) 0;
		  includes_used = 0;
		  dependencies_used = 0;
		}
	      else
		past_first_source_file = 1;
	      last_o_file_start = CUR_SYMBOL_VALUE;
	    }
	  else
	    goto record_it;
#endif /* DBXREAD_ONLY */
	  continue;

	case N_DATA:
	  CUR_SYMBOL_VALUE += ANOFFSET (section_offsets, SECT_OFF_DATA);
	  goto record_it;

	case N_UNDF | N_EXT:
#ifdef DBXREAD_ONLY
	  if (CUR_SYMBOL_VALUE != 0) {
	    /* This is a "Fortran COMMON" symbol.  See if the target
	       environment knows where it has been relocated to.  */

	    CORE_ADDR reladdr;

	    SET_NAMESTRING();
	    if (target_lookup_symbol (namestring, &reladdr)) {
	      continue;		/* Error in lookup; ignore symbol for now.  */
	    }
	    CUR_SYMBOL_TYPE ^= (N_BSS^N_UNDF);	/* Define it as a bss-symbol */
	    CUR_SYMBOL_VALUE = reladdr;
	    goto bss_ext_symbol;
	  }
#endif /* DBXREAD_ONLY */
	  continue;	/* Just undefined, not COMMON */

	case N_UNDF:
#ifdef DBXREAD_ONLY
	  if (processing_acc_compilation && bufp->n_strx == 1) {
	    /* Deal with relative offsets in the string table
	       used in ELF+STAB under Solaris.  If we want to use the
	       n_strx field, which contains the name of the file,
	       we must adjust file_string_table_offset *before* calling
	       SET_NAMESTRING().  */
	    past_first_source_file = 1;
	    file_string_table_offset = next_file_string_table_offset;
	    next_file_string_table_offset =
	      file_string_table_offset + bufp->n_value;
	    if (next_file_string_table_offset < file_string_table_offset)
	      error ("string table offset backs up at %d", symnum);
  /* FIXME -- replace error() with complaint.  */
	    continue;
	  }
#endif /* DBXREAD_ONLY */
	  continue;

	    /* Lots of symbol types we can just ignore.  */

	case N_ABS:
	case N_NBDATA:
	case N_NBBSS:
	  continue;

	  /* Keep going . . .*/

	  /*
	   * Special symbol types for GNU
	   */
	case N_INDR:
	case N_INDR | N_EXT:
	case N_SETA:
	case N_SETA | N_EXT:
	case N_SETT:
	case N_SETT | N_EXT:
	case N_SETD:
	case N_SETD | N_EXT:
	case N_SETB:
	case N_SETB | N_EXT:
	case N_SETV:
	  continue;

	  /*
	   * Debugger symbols
	   */

	case N_SO: {
	  unsigned long valu;
	  static int prev_so_symnum = -10;
	  static int first_so_symnum;
	  char *p;

	  valu = CUR_SYMBOL_VALUE + ANOFFSET (section_offsets, SECT_OFF_TEXT);

	  past_first_source_file = 1;

	  if (prev_so_symnum != symnum - 1)
	    {			/* Here if prev stab wasn't N_SO */
	      first_so_symnum = symnum;

	      if (pst)
		{
		  END_PSYMTAB (pst, psymtab_include_list, includes_used,
			       symnum * symbol_size, valu,
			       dependency_list, dependencies_used);
		  pst = (struct partial_symtab *) 0;
		  includes_used = 0;
		  dependencies_used = 0;
		}
	    }

	  prev_so_symnum = symnum;

	  /* End the current partial symtab and start a new one */

	  SET_NAMESTRING();

	  /* Some compilers (including gcc) emit a pair of initial N_SOs.
	     The first one is a directory name; the second the file name.
	     If pst exists, is empty, and has a filename ending in '/',
	     we assume the previous N_SO was a directory name. */

	  p = strrchr (namestring, '/');
	  if (p && *(p+1) == '\000')
	    continue;		/* Simply ignore directory name SOs */

	  /* Some other compilers (C++ ones in particular) emit useless
	     SOs for non-existant .c files.  We ignore all subsequent SOs that
	     immediately follow the first.  */

	  if (!pst)
	    pst = START_PSYMTAB (objfile, section_offsets,
				 namestring, valu,
				 first_so_symnum * symbol_size,
				 objfile -> global_psymbols.next,
				 objfile -> static_psymbols.next);
	  continue;
	}

	case N_BINCL:
#ifdef DBXREAD_ONLY
	  /* Add this bincl to the bincl_list for future EXCLs.  No
	     need to save the string; it'll be around until
	     read_dbx_symtab function returns */

	  SET_NAMESTRING();

	  add_bincl_to_list (pst, namestring, CUR_SYMBOL_VALUE);

	  /* Mark down an include file in the current psymtab */

	  goto record_include_file;

#else /* DBXREAD_ONLY */
	  continue;
#endif

	case N_SOL:
	  /* Mark down an include file in the current psymtab */

	  SET_NAMESTRING();

	  /* In C++, one may expect the same filename to come round many
	     times, when code is coming alternately from the main file
	     and from inline functions in other files. So I check to see
	     if this is a file we've seen before -- either the main
	     source file, or a previously included file.

	     This seems to be a lot of time to be spending on N_SOL, but
	     things like "break c-exp.y:435" need to work (I
	     suppose the psymtab_include_list could be hashed or put
	     in a binary tree, if profiling shows this is a major hog).  */
	  if (pst && STREQ (namestring, pst->filename))
	    continue;
	  {
	    register int i;
	    for (i = 0; i < includes_used; i++)
	      if (STREQ (namestring, psymtab_include_list[i]))
		{
		  i = -1; 
		  break;
		}
	    if (i == -1)
	      continue;
	  }

#ifdef DBXREAD_ONLY
	record_include_file:
#endif

	  psymtab_include_list[includes_used++] = namestring;
	  if (includes_used >= includes_allocated)
	    {
	      char **orig = psymtab_include_list;

	      psymtab_include_list = (char **)
		alloca ((includes_allocated *= 2) *
			sizeof (char *));
	      memcpy ((PTR)psymtab_include_list, (PTR)orig,
		      includes_used * sizeof (char *));
	    }
	  continue;

	case N_LSYM:		/* Typedef or automatic variable. */
	case N_STSYM:		/* Data seg var -- static  */
	case N_LCSYM:		/* BSS      "  */
	case N_ROSYM:		/* Read-only data seg var -- static.  */
	case N_NBSTS:           /* Gould nobase.  */
	case N_NBLCS:           /* symbols.  */
	case N_FUN:
	case N_GSYM:		/* Global (extern) variable; can be
				   data or bss (sigh FIXME).  */

	/* Following may probably be ignored; I'll leave them here
	   for now (until I do Pascal and Modula 2 extensions).  */

	case N_PC:		/* I may or may not need this; I
				   suspect not.  */
	case N_M2C:		/* I suspect that I can ignore this here. */
	case N_SCOPE:		/* Same.   */

	  SET_NAMESTRING();

	  p = (char *) strchr (namestring, ':');
	  if (!p)
	    continue;		/* Not a debugging symbol.   */



	  /* Main processing section for debugging symbols which
	     the initial read through the symbol tables needs to worry
	     about.  If we reach this point, the symbol which we are
	     considering is definitely one we are interested in.
	     p must also contain the (valid) index into the namestring
	     which indicates the debugging type symbol.  */

	  switch (p[1])
	    {
	    case 'S':
	      CUR_SYMBOL_VALUE += ANOFFSET (section_offsets, SECT_OFF_DATA);
	      ADD_PSYMBOL_ADDR_TO_LIST (namestring, p - namestring,
					VAR_NAMESPACE, LOC_STATIC,
					objfile->static_psymbols,
					CUR_SYMBOL_VALUE,
					psymtab_language, objfile);
	      continue;
	    case 'G':
	      CUR_SYMBOL_VALUE += ANOFFSET (section_offsets, SECT_OFF_DATA);
	      /* The addresses in these entries are reported to be
		 wrong.  See the code that reads 'G's for symtabs. */
	      ADD_PSYMBOL_ADDR_TO_LIST (namestring, p - namestring,
					VAR_NAMESPACE, LOC_STATIC,
					objfile->global_psymbols,
					CUR_SYMBOL_VALUE,
					psymtab_language, objfile);
	      continue;

	    case 'T':
	      if (p != namestring)	/* a name is there, not just :T... */
		{
		  ADD_PSYMBOL_TO_LIST (namestring, p - namestring,
				       STRUCT_NAMESPACE, LOC_TYPEDEF,
				       objfile->static_psymbols,
				       CUR_SYMBOL_VALUE,
				       psymtab_language, objfile);
		  if (p[2] == 't')
		    {
		      /* Also a typedef with the same name.  */
		      ADD_PSYMBOL_TO_LIST (namestring, p - namestring,
					   VAR_NAMESPACE, LOC_TYPEDEF,
					   objfile->static_psymbols,
					   CUR_SYMBOL_VALUE, psymtab_language,
					   objfile);
		      p += 1;
		    }
		}
	      goto check_enum;
	    case 't':
	      if (p != namestring)	/* a name is there, not just :T... */
		{
		  ADD_PSYMBOL_TO_LIST (namestring, p - namestring,
				       VAR_NAMESPACE, LOC_TYPEDEF,
				       objfile->static_psymbols,
				       CUR_SYMBOL_VALUE,
				       psymtab_language, objfile);
		}
	    check_enum:
	      /* If this is an enumerated type, we need to
		 add all the enum constants to the partial symbol
		 table.  This does not cover enums without names, e.g.
		 "enum {a, b} c;" in C, but fortunately those are
		 rare.  There is no way for GDB to find those from the
		 enum type without spending too much time on it.  Thus
		 to solve this problem, the compiler needs to put out the
		 enum in a nameless type.  GCC2 does this.  */

	      /* We are looking for something of the form
		 <name> ":" ("t" | "T") [<number> "="] "e"
		 {<constant> ":" <value> ","} ";".  */

	      /* Skip over the colon and the 't' or 'T'.  */
	      p += 2;
	      /* This type may be given a number.  Also, numbers can come
		 in pairs like (0,26).  Skip over it.  */
	      while ((*p >= '0' && *p <= '9')
		     || *p == '(' || *p == ',' || *p == ')'
		     || *p == '=')
		p++;

	      if (*p++ == 'e')
		{
		  /* We have found an enumerated type.  */
		  /* According to comments in read_enum_type
		     a comma could end it instead of a semicolon.
		     I don't know where that happens.
		     Accept either.  */
		  while (*p && *p != ';' && *p != ',')
		    {
		      char *q;

		      /* Check for and handle cretinous dbx symbol name
			 continuation!  */
		      if (*p == '\\')
			p = next_symbol_text ();

		      /* Point to the character after the name
			 of the enum constant.  */
		      for (q = p; *q && *q != ':'; q++)
			;
		      /* Note that the value doesn't matter for
			 enum constants in psymtabs, just in symtabs.  */
		      ADD_PSYMBOL_TO_LIST (p, q - p,
					   VAR_NAMESPACE, LOC_CONST,
					   objfile->static_psymbols, 0,
					   psymtab_language, objfile);
		      /* Point past the name.  */
		      p = q;
		      /* Skip over the value.  */
		      while (*p && *p != ',')
			p++;
		      /* Advance past the comma.  */
		      if (*p)
			p++;
		    }
		}
	      continue;
	    case 'c':
	      /* Constant, e.g. from "const" in Pascal.  */
	      ADD_PSYMBOL_TO_LIST (namestring, p - namestring,
				   VAR_NAMESPACE, LOC_CONST,
				   objfile->static_psymbols, CUR_SYMBOL_VALUE,
				   psymtab_language, objfile);
	      continue;

	    case 'f':
#ifdef DBXREAD_ONLY
	      /* Kludges for ELF/STABS with Sun ACC */
	      last_function_name = namestring;
	      if (pst && pst->textlow == 0)
		pst->textlow = CUR_SYMBOL_VALUE;
#if 0
	      if (startup_file_end == 0)
		startup_file_end = CUR_SYMBOL_VALUE;
#endif
	      /* End kludge.  */
#endif /* DBXREAD_ONLY */
	      ADD_PSYMBOL_TO_LIST (namestring, p - namestring,
				   VAR_NAMESPACE, LOC_BLOCK,
				   objfile->static_psymbols, CUR_SYMBOL_VALUE,
				   psymtab_language, objfile);
	      continue;

	      /* Global functions were ignored here, but now they
	         are put into the global psymtab like one would expect.
		 They're also in the minimal symbol table.  */
	    case 'F':
#ifdef DBXREAD_ONLY
	      /* Kludges for ELF/STABS with Sun ACC */
	      last_function_name = namestring;
	      if (pst && pst->textlow == 0)
		pst->textlow = CUR_SYMBOL_VALUE;
#if 0
	      if (startup_file_end == 0)
		startup_file_end = CUR_SYMBOL_VALUE;
#endif
	      /* End kludge.  */
#endif /* DBXREAD_ONLY */
	      ADD_PSYMBOL_TO_LIST (namestring, p - namestring,
				   VAR_NAMESPACE, LOC_BLOCK,
				   objfile->global_psymbols, CUR_SYMBOL_VALUE,
				   psymtab_language, objfile);
	      continue;

	      /* Two things show up here (hopefully); static symbols of
		 local scope (static used inside braces) or extensions
		 of structure symbols.  We can ignore both.  */
	    case 'V':
	    case '(':
	    case '0':
	    case '1':
	    case '2':
	    case '3':
	    case '4':
	    case '5':
	    case '6':
	    case '7':
	    case '8':
	    case '9':
	      continue;

	    default:
	      /* Unexpected symbol.  Ignore it; perhaps it is an extension
		 that we don't know about.

		 Someone says sun cc puts out symbols like
		 /foo/baz/maclib::/usr/local/bin/maclib,
		 which would get here with a symbol type of ':'.  */
	      complain (&unknown_symchar_complaint, p[1]);
	      continue;
	    }

	case N_EXCL:
#ifdef DBXREAD_ONLY

	  SET_NAMESTRING();

	  /* Find the corresponding bincl and mark that psymtab on the
	     psymtab dependency list */
	  {
	    struct partial_symtab *needed_pst =
	      find_corresponding_bincl_psymtab (namestring, CUR_SYMBOL_VALUE);

	    /* If this include file was defined earlier in this file,
	       leave it alone.  */
	    if (needed_pst == pst) continue;

	    if (needed_pst)
	      {
		int i;
		int found = 0;

		for (i = 0; i < dependencies_used; i++)
		  if (dependency_list[i] == needed_pst)
		    {
		      found = 1;
		      break;
		    }

		/* If it's already in the list, skip the rest.  */
		if (found) continue;

		dependency_list[dependencies_used++] = needed_pst;
		if (dependencies_used >= dependencies_allocated)
		  {
		    struct partial_symtab **orig = dependency_list;
		    dependency_list =
		      (struct partial_symtab **)
			alloca ((dependencies_allocated *= 2)
				* sizeof (struct partial_symtab *));
		    memcpy ((PTR)dependency_list, (PTR)orig,
			   (dependencies_used
			    * sizeof (struct partial_symtab *)));
#ifdef DEBUG_INFO
		    fprintf (stderr, "Had to reallocate dependency list.\n");
		    fprintf (stderr, "New dependencies allocated: %d\n",
			     dependencies_allocated);
#endif
		  }
	      }
	    else
	      error ("Invalid symbol data: \"repeated\" header file not previously seen, at symtab pos %d.",
		     symnum);
	  }
#endif /* DBXREAD_ONLY */
	  continue;

	case N_RBRAC:
#ifdef HANDLE_RBRAC
	  HANDLE_RBRAC(CUR_SYMBOL_VALUE);
	  continue;
#endif
	case N_EINCL:
	case N_DSLINE:
	case N_BSLINE:
	case N_SSYM:		/* Claim: Structure or union element.
				   Hopefully, I can ignore this.  */
	case N_ENTRY:		/* Alternate entry point; can ignore. */
	case N_MAIN:		/* Can definitely ignore this.   */
	case N_CATCH:		/* These are GNU C++ extensions */
	case N_EHDECL:		/* that can safely be ignored here. */
	case N_LENG:
	case N_BCOMM:
	case N_ECOMM:
	case N_ECOML:
	case N_FNAME:
	case N_SLINE:
	case N_RSYM:
	case N_PSYM:
	case N_LBRAC:
	case N_NSYMS:		/* Ultrix 4.0: symbol count */
	case N_DEFD:		/* GNU Modula-2 */

	case N_OBJ:		/* useless types from Solaris */
	case N_OPT:
	case N_ENDM:
	  /* These symbols aren't interesting; don't worry about them */

	  continue;

	default:
	  /* If we haven't found it yet, ignore it.  It's probably some
	     new type we don't know about yet.  */
	  complain (&unknown_symtype_complaint,
		    local_hex_string ((unsigned long) CUR_SYMBOL_TYPE));
	  continue;
	}
