/* Shared code to pre-read a stab (dbx-style), when building a psymtab.
   Copyright 1986, 1987, 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996
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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* The following need to be defined:
   SET_NAMESTRING() --Set namestring to name of symbol.
   CUR_SYMBOL_TYPE --Type code of current symbol.
   CUR_SYMBOL_VALUE --Value field of current symbol.  May be adjusted here.
   namestring - variable pointing to the name of the stab.
   section_offsets - variable pointing to the section offsets.
   pst - the partial symbol table being built.

   psymtab_include_list, includes_used, includes_allocated - list of include
     file names (N_SOL) seen so far.
   dependency_list, dependencies_used, dependencies_allocated - list of
     N_EXCL stabs seen so far.

   END_PSYMTAB -- end a partial symbol table.
   START_PSYMTAB -- start a partial symbol table.
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
		  && namestring [nsl - 2] == '.'))
	    {
	      if (objfile -> ei.entry_point <  CUR_SYMBOL_VALUE &&
		  objfile -> ei.entry_point >= last_o_file_start)
		{
		  objfile -> ei.entry_file_lowpc = last_o_file_start;
		  objfile -> ei.entry_file_highpc = CUR_SYMBOL_VALUE;
		}
	      if (past_first_source_file && pst
		  /* The gould NP1 uses low values for .o and -l symbols
		     which are not the address.  */
		  && CUR_SYMBOL_VALUE >= pst->textlow)
		{
		  END_PSYMTAB (pst, psymtab_include_list, includes_used,
			       symnum * symbol_size,
			       CUR_SYMBOL_VALUE > pst->texthigh
				 ? CUR_SYMBOL_VALUE : pst->texthigh, 
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
#ifdef SOFUN_ADDRESS_MAYBE_MISSING
	  /* A zero value is probably an indication for the SunPRO 3.0
	     compiler. end_psymtab explicitly tests for zero, so
	     don't relocate it.  */
	  if (CUR_SYMBOL_VALUE == 0)
	    valu = 0;
#endif

	  past_first_source_file = 1;

	  if (prev_so_symnum != symnum - 1)
	    {			/* Here if prev stab wasn't N_SO */
	      first_so_symnum = symnum;

	      if (pst)
		{
		  END_PSYMTAB (pst, psymtab_include_list, includes_used,
			       symnum * symbol_size,
			       valu > pst->texthigh ? valu : pst->texthigh,
			       dependency_list, dependencies_used);
		  pst = (struct partial_symtab *) 0;
		  includes_used = 0;
		  dependencies_used = 0;
		}
	    }

	  prev_so_symnum = symnum;

	  /* End the current partial symtab and start a new one */

	  SET_NAMESTRING();

	  /* Null name means end of .o file.  Don't start a new one. */
	  if (*namestring == '\000')
	    continue;

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
	  {
#ifdef DBXREAD_ONLY
	    enum language tmp_language;
	    /* Add this bincl to the bincl_list for future EXCLs.  No
	       need to save the string; it'll be around until
	       read_dbx_symtab function returns */

	    SET_NAMESTRING();

	    tmp_language = deduce_language_from_filename (namestring);

	    /* Only change the psymtab's language if we've learned
	       something useful (eg. tmp_language is not language_unknown).
	       In addition, to match what start_subfile does, never change
	       from C++ to C.  */
	    if (tmp_language != language_unknown
		&& (tmp_language != language_c
		    || psymtab_language != language_cplus))
	      psymtab_language = tmp_language;

	    add_bincl_to_list (pst, namestring, CUR_SYMBOL_VALUE);

	    /* Mark down an include file in the current psymtab */

	    goto record_include_file;

#else /* DBXREAD_ONLY */
	    continue;
#endif
	  }

	case N_SOL:
	  {
	    enum language tmp_language;
	    /* Mark down an include file in the current psymtab */
	    
	    SET_NAMESTRING();
  
	    tmp_language = deduce_language_from_filename (namestring);
  
	    /* Only change the psymtab's language if we've learned
	       something useful (eg. tmp_language is not language_unknown).
	       In addition, to match what start_subfile does, never change
	       from C++ to C.  */
	    if (tmp_language != language_unknown
		&& (tmp_language != language_c
		    || psymtab_language != language_cplus))
	      psymtab_language = tmp_language;
	    
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
	  }
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

#ifdef DBXREAD_ONLY
	  /* See if this is an end of function stab.  */
	  if (CUR_SYMBOL_TYPE == N_FUN && ! strcmp (namestring, ""))
	    {
	      unsigned long valu;

	      /* It's value is the size (in bytes) of the function for
		 function relative stabs, or the address of the function's
		 end for old style stabs.  */
	      valu = CUR_SYMBOL_VALUE + last_function_start;
	      if (pst->texthigh == 0 || valu > pst->texthigh)
		pst->texthigh = valu;
	      break;
	     }
#endif

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
#ifdef STATIC_TRANSFORM_NAME
	      namestring = STATIC_TRANSFORM_NAME (namestring);
#endif
	      add_psymbol_to_list (namestring, p - namestring,
				   VAR_NAMESPACE, LOC_STATIC,
				   &objfile->static_psymbols,
				   0, CUR_SYMBOL_VALUE,
				   psymtab_language, objfile);
	      continue;
	    case 'G':
	      CUR_SYMBOL_VALUE += ANOFFSET (section_offsets, SECT_OFF_DATA);
	      /* The addresses in these entries are reported to be
		 wrong.  See the code that reads 'G's for symtabs. */
	      add_psymbol_to_list (namestring, p - namestring,
				   VAR_NAMESPACE, LOC_STATIC,
				   &objfile->global_psymbols,
				   0, CUR_SYMBOL_VALUE,
				   psymtab_language, objfile);
	      continue;

	    case 'T':
	      if (p != namestring)	/* a name is there, not just :T... */
		{
		  add_psymbol_to_list (namestring, p - namestring,
				       STRUCT_NAMESPACE, LOC_TYPEDEF,
				       &objfile->static_psymbols,
				       CUR_SYMBOL_VALUE, 0,
				       psymtab_language, objfile);
		  if (p[2] == 't')
		    {
		      /* Also a typedef with the same name.  */
		      add_psymbol_to_list (namestring, p - namestring,
					   VAR_NAMESPACE, LOC_TYPEDEF,
					   &objfile->static_psymbols,
					   CUR_SYMBOL_VALUE, 0,
					   psymtab_language, objfile);
		      p += 1;
		    }
		  /* The semantics of C++ state that "struct foo { ... }"
		     also defines a typedef for "foo".  Unfortuantely, cfront
		     never makes the typedef when translating from C++ to C.
		     We make the typedef here so that "ptype foo" works as
		     expected for cfront translated code.  */
		  else if (psymtab_language == language_cplus)
		   {
		      /* Also a typedef with the same name.  */
		      add_psymbol_to_list (namestring, p - namestring,
					   VAR_NAMESPACE, LOC_TYPEDEF,
					   &objfile->static_psymbols,
					   CUR_SYMBOL_VALUE, 0,
					   psymtab_language, objfile);
		   }
		}
	      goto check_enum;
	    case 't':
	      if (p != namestring)	/* a name is there, not just :T... */
		{
		  add_psymbol_to_list (namestring, p - namestring,
				       VAR_NAMESPACE, LOC_TYPEDEF,
				       &objfile->static_psymbols,
				       CUR_SYMBOL_VALUE, 0,
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
		  /* The aix4 compiler emits extra crud before the members.  */
		  if (*p == '-')
		    {
		      /* Skip over the type (?).  */
		      while (*p != ':')
			p++;

		      /* Skip over the colon.  */
		      p++;
		    }

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
		      if (*p == '\\' || (*p == '?' && p[1] == '\0'))
			p = next_symbol_text (objfile);

		      /* Point to the character after the name
			 of the enum constant.  */
		      for (q = p; *q && *q != ':'; q++)
			;
		      /* Note that the value doesn't matter for
			 enum constants in psymtabs, just in symtabs.  */
		      add_psymbol_to_list (p, q - p,
					   VAR_NAMESPACE, LOC_CONST,
					   &objfile->static_psymbols, 0,
					   0, psymtab_language, objfile);
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
	      add_psymbol_to_list (namestring, p - namestring,
				   VAR_NAMESPACE, LOC_CONST,
				   &objfile->static_psymbols, CUR_SYMBOL_VALUE,
				   0, psymtab_language, objfile);
	      continue;

	    case 'f':
	      CUR_SYMBOL_VALUE += ANOFFSET (section_offsets, SECT_OFF_TEXT);
#ifdef DBXREAD_ONLY
	      /* Keep track of the start of the last function so we
		 can handle end of function symbols.  */
	      last_function_start = CUR_SYMBOL_VALUE;
	      /* Kludges for ELF/STABS with Sun ACC */
	      last_function_name = namestring;
#ifdef SOFUN_ADDRESS_MAYBE_MISSING
	      /* Do not fix textlow==0 for .o or NLM files, as 0 is a legit
		 value for the bottom of the text seg in those cases. */
	      if (pst && pst->textlow == 0 && !symfile_relocatable)
		pst->textlow =
		  find_stab_function_addr (namestring, pst, objfile);
#endif
#if 0
	      if (startup_file_end == 0)
		startup_file_end = CUR_SYMBOL_VALUE;
#endif
	      /* End kludge.  */

	      /* In reordered executables this function may lie outside
		 the bounds created by N_SO symbols.  If that's the case
		 use the address of this function as the low bound for
		 the partial symbol table.  */
	      if (pst->textlow == 0
		  || (CUR_SYMBOL_VALUE < pst->textlow
		      && CUR_SYMBOL_VALUE
			   != ANOFFSET (section_offsets, SECT_OFF_TEXT)))
		pst->textlow = CUR_SYMBOL_VALUE;
#endif /* DBXREAD_ONLY */
	      add_psymbol_to_list (namestring, p - namestring,
				   VAR_NAMESPACE, LOC_BLOCK,
				   &objfile->static_psymbols, CUR_SYMBOL_VALUE,
				   0, psymtab_language, objfile);
	      continue;

	      /* Global functions were ignored here, but now they
	         are put into the global psymtab like one would expect.
		 They're also in the minimal symbol table.  */
	    case 'F':
	      CUR_SYMBOL_VALUE += ANOFFSET (section_offsets, SECT_OFF_TEXT);
#ifdef DBXREAD_ONLY
	      /* Keep track of the start of the last function so we
		 can handle end of function symbols.  */
	      last_function_start = CUR_SYMBOL_VALUE;
	      /* Kludges for ELF/STABS with Sun ACC */
	      last_function_name = namestring;
#ifdef SOFUN_ADDRESS_MAYBE_MISSING
	      /* Do not fix textlow==0 for .o or NLM files, as 0 is a legit
		 value for the bottom of the text seg in those cases. */
	      if (pst && pst->textlow == 0 && !symfile_relocatable)
		pst->textlow =
		  find_stab_function_addr (namestring, pst, objfile);
#endif
#if 0
	      if (startup_file_end == 0)
		startup_file_end = CUR_SYMBOL_VALUE;
#endif
	      /* End kludge.  */
	      /* In reordered executables this function may lie outside
		 the bounds created by N_SO symbols.  If that's the case
		 use the address of this function as the low bound for
		 the partial symbol table.  */
	      if (pst->textlow == 0
		  || (CUR_SYMBOL_VALUE < pst->textlow
		      && CUR_SYMBOL_VALUE
			   != ANOFFSET (section_offsets, SECT_OFF_TEXT)))
		pst->textlow = CUR_SYMBOL_VALUE;
#endif /* DBXREAD_ONLY */
	      add_psymbol_to_list (namestring, p - namestring,
				   VAR_NAMESPACE, LOC_BLOCK,
				   &objfile->global_psymbols, CUR_SYMBOL_VALUE,
				   0, psymtab_language, objfile);
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
	    case '-':
	      continue;

	    case ':':
	      /* It is a C++ nested symbol.  We don't need to record it
		 (I don't think); if we try to look up foo::bar::baz,
		 then symbols for the symtab containing foo should get
		 read in, I think.  */
	      /* Someone says sun cc puts out symbols like
		 /foo/baz/maclib::/usr/local/bin/maclib,
		 which would get here with a symbol type of ':'.  */
	      continue;

	    default:
	      /* Unexpected symbol descriptor.  The second and subsequent stabs
		 of a continued stab can show up here.  The question is
		 whether they ever can mimic a normal stab--it would be
		 nice if not, since we certainly don't want to spend the
		 time searching to the end of every string looking for
		 a backslash.  */

	      complain (&unknown_symchar_complaint, p[1]);

	      /* Ignore it; perhaps it is an extension that we don't
		 know about.  */
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
		    fprintf_unfiltered (gdb_stderr, "Had to reallocate dependency list.\n");
		    fprintf_unfiltered (gdb_stderr, "New dependencies allocated: %d\n",
			     dependencies_allocated);
#endif
		  }
	      }
	  }
#endif /* DBXREAD_ONLY */
	  continue;

	case N_ENDM:
#ifdef SOFUN_ADDRESS_MAYBE_MISSING
	  /* Solaris 2 end of module, finish current partial symbol table.
	     END_PSYMTAB will set pst->texthigh to the proper value, which
	     is necessary if a module compiled without debugging info
	     follows this module.  */
	  if (pst)
	    {
	      END_PSYMTAB (pst, psymtab_include_list, includes_used,
			   symnum * symbol_size,
			   (CORE_ADDR) 0,
			   dependency_list, dependencies_used);
	      pst = (struct partial_symtab *) 0;
	      includes_used = 0;
	      dependencies_used = 0;
	    }
#endif
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
	  /* These symbols aren't interesting; don't worry about them */

	  continue;

	default:
	  /* If we haven't found it yet, ignore it.  It's probably some
	     new type we don't know about yet.  */
	  complain (&unknown_symtype_complaint,
		    local_hex_string (CUR_SYMBOL_TYPE));
	  continue;
	}
