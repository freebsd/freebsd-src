/* ld-emul.h - Linker emulation header file
   Copyright 1991, 92, 93, 94, 95, 96, 97, 1998, 2000 Free Software Foundation, Inc.

   This file is part of GLD, the Gnu Linker.

   GLD is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   GLD is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#ifndef LDEMUL_H
#define LDEMUL_H

#if ANSI_PROTOTYPES
struct lang_input_statement_struct;
struct search_dirs;
#endif

extern void ldemul_hll PARAMS ((char *));
extern void ldemul_syslib PARAMS ((char *));
extern void ldemul_after_parse PARAMS ((void));
extern void ldemul_before_parse PARAMS ((void));
extern void ldemul_after_open PARAMS ((void));
extern void ldemul_after_allocation PARAMS ((void));
extern void ldemul_before_allocation PARAMS ((void));
extern void ldemul_set_output_arch PARAMS ((void));
extern char *ldemul_choose_target PARAMS ((void));
extern void ldemul_choose_mode PARAMS ((char *));
extern void ldemul_list_emulations PARAMS ((FILE *));
extern void ldemul_list_emulation_options PARAMS ((FILE *));
extern char *ldemul_get_script PARAMS ((int *isfile));
extern void ldemul_finish PARAMS ((void));
extern void ldemul_set_symbols PARAMS ((void));
extern void ldemul_create_output_section_statements PARAMS ((void));
extern boolean ldemul_place_orphan
  PARAMS ((struct lang_input_statement_struct *, asection *));
extern int ldemul_parse_args PARAMS ((int, char **));
extern boolean ldemul_unrecognized_file
  PARAMS ((struct lang_input_statement_struct *));
extern boolean ldemul_recognized_file
  PARAMS ((struct lang_input_statement_struct *));
extern boolean ldemul_open_dynamic_archive
  PARAMS ((const char *, struct search_dirs *,
	   struct lang_input_statement_struct *));
extern char *ldemul_default_target PARAMS ((void));
extern void after_parse_default PARAMS ((void));
extern void after_open_default PARAMS ((void));
extern void after_allocation_default PARAMS ((void));
extern void before_allocation_default PARAMS ((void));
extern void set_output_arch_default PARAMS ((void));
extern void syslib_default PARAMS ((char*));
extern void hll_default PARAMS ((char*));
extern int  ldemul_find_potential_libraries
  PARAMS ((char *, struct lang_input_statement_struct *));

typedef struct ld_emulation_xfer_struct
{
  /* Run before parsing the command line and script file.
     Set the architecture, maybe other things.  */
  void   (*before_parse) PARAMS ((void));

  /* Handle the SYSLIB (low level library) script command.  */
  void   (*syslib) PARAMS ((char *));

  /* Handle the HLL (high level library) script command.  */
  void   (*hll) PARAMS ((char *));

  /* Run after parsing the command line and script file.  */
  void   (*after_parse) PARAMS ((void));

  /* Run after opening all input files, and loading the symbols.  */
  void   (*after_open) PARAMS ((void));

  /* Run after allocating output sections.  */
  void   (*after_allocation) PARAMS ( (void));

  /* Set the output architecture and machine if possible.  */
  void   (*set_output_arch) PARAMS ((void));

  /* Decide which target name to use.  */
  char * (*choose_target) PARAMS ((void));

  /* Run before allocating output sections.  */
  void   (*before_allocation) PARAMS ((void));

  /* Return the appropriate linker script.  */
  char * (*get_script) PARAMS ((int *isfile));

  /* The name of this emulation.  */
  char *emulation_name;

  /* The output format.  */
  char *target_name;

  /* Run after assigning values from the script.  */
  void	(*finish) PARAMS ((void));

  /* Create any output sections needed by the target.  */
  void	(*create_output_section_statements) PARAMS ((void));

  /* Try to open a dynamic library.  ARCH is an architecture name, and
     is normally the empty string.  ENTRY is the lang_input_statement
     that should be opened.  */
  boolean (*open_dynamic_archive)
    PARAMS ((const char *arch, struct search_dirs *,
	     struct lang_input_statement_struct *entry));

  /* Place an orphan section.  Return true if it was placed, false if
     the default action should be taken.  This field may be NULL, in
     which case the default action will always be taken.  */
  boolean (*place_orphan)
    PARAMS ((struct lang_input_statement_struct *, asection *));

  /* Run after assigning parsing with the args, but before 
     reading the script.  Used to initialize symbols used in the script. */
  void	(*set_symbols) PARAMS ((void));

  /* Run to parse args which the base linker doesn't
     understand. Return non zero on sucess. */
  int (*parse_args) PARAMS ((int, char **));

  /* Run to handle files which are not recognized as object files or
     archives.  Return true if the file was handled.  */
  boolean (*unrecognized_file)
    PARAMS ((struct lang_input_statement_struct *));

  /* Run to list the command line options which parse_args handles.  */
  void (* list_options) PARAMS ((FILE *));

  /* Run to specially handle files which *are* recognized as object
     files or archives.  Return true if the file was handled.  */
  boolean (*recognized_file)
    PARAMS ((struct lang_input_statement_struct *));

  /* Called when looking for libraries in a directory specified
     via a linker command line option or linker script option.
     Files that match the pattern "lib*.a" have already been scanned.
     (For VMS files matching ":lib*.a" have also been scanned).  */
  int (* find_potential_libraries)
    PARAMS ((char *, struct lang_input_statement_struct *));
  
} ld_emulation_xfer_type;

typedef enum 
{
  intel_ic960_ld_mode_enum,
  default_mode_enum ,
  intel_gld960_ld_mode_enum
} lang_emulation_mode_enum_type;

extern ld_emulation_xfer_type *ld_emulations[];

#endif
