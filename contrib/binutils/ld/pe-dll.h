/* pe-dll.h: Header file for routines used to build Windows DLLs.
   Copyright (C) 1999, 2000 Free Software Foundation, Inc.

   This file is part of GLD, the Gnu Linker.

   GLD is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GLD is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GLD; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#ifndef PE_DLL_H
#define PE_DLL_H

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "deffile.h"

extern def_file *pe_def_file;
extern int pe_dll_export_everything;
extern int pe_dll_do_default_excludes;
extern int pe_dll_kill_ats;
extern int pe_dll_stdcall_aliases;
extern int pe_dll_warn_dup_exports;
extern int pe_dll_compat_implib;

extern void pe_dll_id_target PARAMS ((const char *));
extern void pe_dll_add_excludes PARAMS ((const char *));
extern void pe_dll_generate_def_file PARAMS ((const char *));
extern void pe_dll_generate_implib PARAMS ((def_file *, const char *));
extern void pe_process_import_defs PARAMS ((bfd *, struct bfd_link_info *));
extern boolean pe_implied_import_dll PARAMS ((const char *));
extern void pe_dll_build_sections PARAMS ((bfd *, struct bfd_link_info *));
extern void pe_exe_build_sections PARAMS ((bfd *, struct bfd_link_info *));
extern void pe_dll_fill_sections PARAMS ((bfd *, struct bfd_link_info *));
extern void pe_exe_fill_sections PARAMS ((bfd *, struct bfd_link_info *));

#endif /* PE_DLL_H */
