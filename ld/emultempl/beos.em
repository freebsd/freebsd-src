# This shell script emits a C file. -*- C -*-
# It does some substitutions.
if [ -z "$MACHINE" ]; then
  OUTPUT_ARCH=${ARCH}
else
  OUTPUT_ARCH=${ARCH}:${MACHINE}
fi
cat >e${EMULATION_NAME}.c <<EOF
/* This file is part of GLD, the Gnu Linker.
   Copyright 1995, 1996, 1997, 1998, 2000, 2001, 2002, 2003, 2004
   Free Software Foundation, Inc.

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

/* For WINDOWS_NT */
/* The original file generated returned different default scripts depending
   on whether certain switches were set, but these switches pertain to the
   Linux system and that particular version of coff.  In the NT case, we
   only determine if the subsystem is console or windows in order to select
   the correct entry point by default. */

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "getopt.h"
#include "libiberty.h"
#include "ld.h"
#include "ldmain.h"
#include "ldexp.h"
#include "ldlang.h"
#include "ldfile.h"
#include "ldemul.h"
#include <ldgram.h>
#include "ldlex.h"
#include "ldmisc.h"
#include "ldctor.h"
#include "coff/internal.h"
#include "../bfd/libcoff.h"

#define TARGET_IS_${EMULATION_NAME}

static struct internal_extra_pe_aouthdr pe;
static int dll;

extern const char *output_filename;

static void
gld_${EMULATION_NAME}_before_parse (void)
{
  ldfile_set_output_arch ("${OUTPUT_ARCH}", bfd_arch_`echo ${ARCH} | sed -e 's/:.*//'`);
  output_filename = "a.exe";
}

/* PE format extra command line options.  */

/* Used for setting flags in the PE header. */
#define OPTION_BASE_FILE		(300  + 1)
#define OPTION_DLL			(OPTION_BASE_FILE + 1)
#define OPTION_FILE_ALIGNMENT		(OPTION_DLL + 1)
#define OPTION_IMAGE_BASE		(OPTION_FILE_ALIGNMENT + 1)
#define OPTION_MAJOR_IMAGE_VERSION	(OPTION_IMAGE_BASE + 1)
#define OPTION_MAJOR_OS_VERSION		(OPTION_MAJOR_IMAGE_VERSION + 1)
#define OPTION_MAJOR_SUBSYSTEM_VERSION	(OPTION_MAJOR_OS_VERSION + 1)
#define OPTION_MINOR_IMAGE_VERSION	(OPTION_MAJOR_SUBSYSTEM_VERSION + 1)
#define OPTION_MINOR_OS_VERSION		(OPTION_MINOR_IMAGE_VERSION + 1)
#define OPTION_MINOR_SUBSYSTEM_VERSION	(OPTION_MINOR_OS_VERSION + 1)
#define OPTION_SECTION_ALIGNMENT	(OPTION_MINOR_SUBSYSTEM_VERSION + 1)
#define OPTION_STACK                    (OPTION_SECTION_ALIGNMENT + 1)
#define OPTION_SUBSYSTEM                (OPTION_STACK + 1)
#define OPTION_HEAP			(OPTION_SUBSYSTEM + 1)

static void
gld${EMULATION_NAME}_add_options
  (int ns ATTRIBUTE_UNUSED, char **shortopts ATTRIBUTE_UNUSED, int nl,
   struct option **longopts, int nrl ATTRIBUTE_UNUSED,
   struct option **really_longopts ATTRIBUTE_UNUSED)
{
  static const struct option xtra_long[] = {
    /* PE options */
    {"base-file", required_argument, NULL, OPTION_BASE_FILE},
    {"dll", no_argument, NULL, OPTION_DLL},
    {"file-alignment", required_argument, NULL, OPTION_FILE_ALIGNMENT},
    {"heap", required_argument, NULL, OPTION_HEAP},
    {"image-base", required_argument, NULL, OPTION_IMAGE_BASE},
    {"major-image-version", required_argument, NULL, OPTION_MAJOR_IMAGE_VERSION},
    {"major-os-version", required_argument, NULL, OPTION_MAJOR_OS_VERSION},
    {"major-subsystem-version", required_argument, NULL, OPTION_MAJOR_SUBSYSTEM_VERSION},
    {"minor-image-version", required_argument, NULL, OPTION_MINOR_IMAGE_VERSION},
    {"minor-os-version", required_argument, NULL, OPTION_MINOR_OS_VERSION},
    {"minor-subsystem-version", required_argument, NULL, OPTION_MINOR_SUBSYSTEM_VERSION},
    {"section-alignment", required_argument, NULL, OPTION_SECTION_ALIGNMENT},
    {"stack", required_argument, NULL, OPTION_STACK},
    {"subsystem", required_argument, NULL, OPTION_SUBSYSTEM},
    {NULL, no_argument, NULL, 0}
  };

  *longopts = (struct option *)
    xrealloc (*longopts, nl * sizeof (struct option) + sizeof (xtra_long));
  memcpy (*longopts + nl, &xtra_long, sizeof (xtra_long));
}


/* PE/WIN32; added routines to get the subsystem type, heap and/or stack
   parameters which may be input from the command line */

typedef struct {
  void *ptr;
  int size;
  int value;
  char *symbol;
  int inited;
} definfo;

#define D(field,symbol,def)  {&pe.field,sizeof(pe.field), def, symbol,0}

static definfo init[] =
{
  /* imagebase must be first */
#define IMAGEBASEOFF 0
  D(ImageBase,"__image_base__", BEOS_EXE_IMAGE_BASE),
#define DLLOFF 1
  {&dll, sizeof(dll), 0, "__dll__", 0},
  D(SectionAlignment,"__section_alignment__", PE_DEF_SECTION_ALIGNMENT),
  D(FileAlignment,"__file_alignment__", PE_DEF_FILE_ALIGNMENT),
  D(MajorOperatingSystemVersion,"__major_os_version__", 4),
  D(MinorOperatingSystemVersion,"__minor_os_version__", 0),
  D(MajorImageVersion,"__major_image_version__", 1),
  D(MinorImageVersion,"__minor_image_version__", 0),
  D(MajorSubsystemVersion,"__major_subsystem_version__", 4),
  D(MinorSubsystemVersion,"__minor_subsystem_version__", 0),
  D(Subsystem,"__subsystem__", 3),
  D(SizeOfStackReserve,"__size_of_stack_reserve__", 0x2000000),
  D(SizeOfStackCommit,"__size_of_stack_commit__", 0x1000),
  D(SizeOfHeapReserve,"__size_of_heap_reserve__", 0x100000),
  D(SizeOfHeapCommit,"__size_of_heap_commit__", 0x1000),
  D(LoaderFlags,"__loader_flags__", 0x0),
  { NULL, 0, 0, NULL, 0 }
};


static void
set_pe_name (char *name, long val)
{
  int i;
  /* Find the name and set it. */
  for (i = 0; init[i].ptr; i++)
    {
      if (strcmp (name, init[i].symbol) == 0)
	{
	  init[i].value = val;
	  init[i].inited = 1;
	  return;
	}
    }
  abort();
}


static void
set_pe_subsystem (void)
{
  const char *sver;
  int len;
  int i;
  static const struct
    {
      const char *name;
      const int value;
      const char *entry;
    }
  v[] =
    {
      { "native", 1, "_NtProcessStartup" },
      { "windows", 2, "_WinMainCRTStartup" },
      { "wwindows", 2, "_wWinMainCRTStartup" },
      { "console", 3, "_mainCRTStartup" },
      { "wconsole", 3, "_wmainCRTStartup" },
#if 0
      /* The Microsoft linker does not recognize this.  */
      { "os2", 5, "" },
#endif
      { "posix", 7, "___PosixProcessStartup"},
      { 0, 0, 0 }
    };

  sver = strchr (optarg, ':');
  if (sver == NULL)
    len = strlen (optarg);
  else
    {
      char *end;

      len = sver - optarg;
      set_pe_name ("__major_subsystem_version__",
		   strtoul (sver + 1, &end, 0));
      if (*end == '.')
	set_pe_name ("__minor_subsystem_version__",
		     strtoul (end + 1, &end, 0));
      if (*end != '\0')
	einfo ("%P: warning: bad version number in -subsystem option\n");
    }

  for (i = 0; v[i].name; i++)
    {
      if (strncmp (optarg, v[i].name, len) == 0
	  && v[i].name[len] == '\0')
	{
	  set_pe_name ("__subsystem__", v[i].value);

	  /* If the subsystem is windows, we use a different entry
	     point.  We also register the entry point as an undefined
	     symbol. from lang_add_entry() The reason we do
	     this is so that the user
	     doesn't have to because they would have to use the -u
	     switch if they were specifying an entry point other than
	     _mainCRTStartup.  Specifically, if creating a windows
	     application, entry point _WinMainCRTStartup must be
	     specified.  What I have found for non console
	     applications (entry not _mainCRTStartup) is that the .obj
	     that contains mainCRTStartup is brought in since it is
	     the first encountered in libc.lib and it has other
	     symbols in it which will be pulled in by the link
	     process.  To avoid this, adding -u with the entry point
	     name specified forces the correct .obj to be used.  We
	     can avoid making the user do this by always adding the
	     entry point name as an undefined symbol.  */
	  lang_add_entry (v[i].entry, 1);

	  return;
	}
    }
  einfo ("%P%F: invalid subsystem type %s\n", optarg);
}


static void
set_pe_value (char *name)
{
  char *end;
  set_pe_name (name,  strtoul (optarg, &end, 0));
  if (end == optarg)
    {
      einfo ("%P%F: invalid hex number for PE parameter '%s'\n", optarg);
    }

  optarg = end;
}

static void
set_pe_stack_heap (char *resname, char *comname)
{
  set_pe_value (resname);
  if (*optarg == ',')
    {
      optarg++;
      set_pe_value (comname);
    }
  else if (*optarg)
    {
      einfo ("%P%F: strange hex info for PE parameter '%s'\n", optarg);
    }
}


static bfd_boolean
gld${EMULATION_NAME}_handle_option (int optc)
{
  switch (optc)
    {
    default:
      return FALSE;

    case OPTION_BASE_FILE:
      link_info.base_file = fopen (optarg, FOPEN_WB);
      if (link_info.base_file == NULL)
	{
	  fprintf (stderr, "%s: Can't open base file %s\n",
		   program_name, optarg);
	  xexit (1);
	}
      break;

      /* PE options */
    case OPTION_HEAP:
      set_pe_stack_heap ("__size_of_heap_reserve__", "__size_of_heap_commit__");
      break;
    case OPTION_STACK:
      set_pe_stack_heap ("__size_of_stack_reserve__", "__size_of_stack_commit__");
      break;
    case OPTION_SUBSYSTEM:
      set_pe_subsystem ();
      break;
    case OPTION_MAJOR_OS_VERSION:
      set_pe_value ("__major_os_version__");
      break;
    case OPTION_MINOR_OS_VERSION:
      set_pe_value ("__minor_os_version__");
      break;
    case OPTION_MAJOR_SUBSYSTEM_VERSION:
      set_pe_value ("__major_subsystem_version__");
      break;
    case OPTION_MINOR_SUBSYSTEM_VERSION:
      set_pe_value ("__minor_subsystem_version__");
      break;
    case OPTION_MAJOR_IMAGE_VERSION:
      set_pe_value ("__major_image_version__");
      break;
    case OPTION_MINOR_IMAGE_VERSION:
      set_pe_value ("__minor_image_version__");
      break;
    case OPTION_FILE_ALIGNMENT:
      set_pe_value ("__file_alignment__");
      break;
    case OPTION_SECTION_ALIGNMENT:
      set_pe_value ("__section_alignment__");
      break;
    case OPTION_DLL:
      set_pe_name ("__dll__", 1);
      break;
    case OPTION_IMAGE_BASE:
      set_pe_value ("__image_base__");
      break;
    }
  return TRUE;
}

/* Assign values to the special symbols before the linker script is
   read.  */

static void
gld_${EMULATION_NAME}_set_symbols (void)
{
  /* Run through and invent symbols for all the
     names and insert the defaults. */
  int j;
  lang_statement_list_type *save;

  if (!init[IMAGEBASEOFF].inited)
    {
      if (link_info.relocatable)
	init[IMAGEBASEOFF].value = 0;
      else if (init[DLLOFF].value)
	init[IMAGEBASEOFF].value = BEOS_DLL_IMAGE_BASE;
      else
	init[IMAGEBASEOFF].value = BEOS_EXE_IMAGE_BASE;
    }

  /* Don't do any symbol assignments if this is a relocatable link.  */
  if (link_info.relocatable)
    return;

  /* Glue the assignments into the abs section */
  save = stat_ptr;

  stat_ptr = &(abs_output_section->children);

  for (j = 0; init[j].ptr; j++)
    {
      long val = init[j].value;
      lang_add_assignment (exp_assop ('=', init[j].symbol, exp_intop (val)));
      if (init[j].size == sizeof(short))
	*(short *)init[j].ptr = val;
      else if (init[j].size == sizeof(int))
	*(int *)init[j].ptr = val;
      else if (init[j].size == sizeof(long))
	*(long *)init[j].ptr = val;
      /* This might be a long long or other special type.  */
      else if (init[j].size == sizeof(bfd_vma))
	*(bfd_vma *)init[j].ptr = val;
      else	abort();
    }
  /* Restore the pointer. */
  stat_ptr = save;

  if (pe.FileAlignment >
      pe.SectionAlignment)
    {
      einfo ("%P: warning, file alignment > section alignment.\n");
    }
}

static void
gld_${EMULATION_NAME}_after_open (void)
{
  /* Pass the wacky PE command line options into the output bfd.
     FIXME: This should be done via a function, rather than by
     including an internal BFD header.  */
  if (!coff_data(output_bfd)->pe)
    {
      einfo ("%F%P: PE operations on non PE file.\n");
    }

  pe_data(output_bfd)->pe_opthdr = pe;
  pe_data(output_bfd)->dll = init[DLLOFF].value;

}

/* Callback functions for qsort in sort_sections. */

static int
sort_by_file_name (const void *a, const void *b)
{
  const lang_statement_union_type *const *ra = a;
  const lang_statement_union_type *const *rb = b;
  int i, a_sec, b_sec;

  i = strcmp ((*ra)->input_section.ifile->the_bfd->my_archive->filename,
	      (*rb)->input_section.ifile->the_bfd->my_archive->filename);
  if (i != 0)
    return i;

  i = strcmp ((*ra)->input_section.ifile->filename,
		 (*rb)->input_section.ifile->filename);
  if (i != 0)
    return i;
  /* the tail idata4/5 are the only ones without relocs to an
     idata$6 section unless we are importing by ordinal,
     so sort them to last to terminate the IAT
     and HNT properly. if no reloc this one is import by ordinal
     so we have to sort by section contents */

  if ( ((*ra)->input_section.section->reloc_count + (*rb)->input_section.section->reloc_count) )
    {
       i =  (((*ra)->input_section.section->reloc_count >
		 (*rb)->input_section.section->reloc_count) ? -1 : 0);
       if ( i != 0)
         return i;

        return  (((*ra)->input_section.section->reloc_count >
		 (*rb)->input_section.section->reloc_count) ? 0 : 1);
    }
  else
    {
       if ( (strcmp( (*ra)->input_section.section->name, ".idata$6") == 0) )
          return 0; /* don't sort .idata$6 or .idata$7 FIXME dlltool eliminate .idata$7 */

       if (! bfd_get_section_contents ((*ra)->input_section.ifile->the_bfd,
         (*ra)->input_section.section, &a_sec, (file_ptr) 0, (bfd_size_type)sizeof(a_sec)))
            einfo ("%F%B: Can't read contents of section .idata: %E\n",
                 (*ra)->input_section.ifile->the_bfd);

       if (! bfd_get_section_contents ((*rb)->input_section.ifile->the_bfd,
        (*rb)->input_section.section, &b_sec, (file_ptr) 0, (bfd_size_type)sizeof(b_sec) ))
           einfo ("%F%B: Can't read contents of section .idata: %E\n",
                (*rb)->input_section.ifile->the_bfd);

      i =  ((a_sec < b_sec) ? -1 : 0);
      if ( i != 0)
        return i;
      return  ((a_sec < b_sec) ? 0 : 1);
   }
return 0;
}

static int
sort_by_section_name (const void *a, const void *b)
{
  const lang_statement_union_type *const *ra = a;
  const lang_statement_union_type *const *rb = b;
  int i;
  i = strcmp ((*ra)->input_section.section->name,
		 (*rb)->input_section.section->name);
/* this is a hack to make .stab and .stabstr last, so we don't have
   to fix strip/objcopy for .reloc sections.
   FIXME stripping images with a .rsrc section still needs to be fixed */
  if ( i != 0)
    {
      if ((strncmp ((*ra)->input_section.section->name, ".stab", 5) == 0)
           && (strncmp ((*rb)->input_section.section->name, ".stab", 5) != 0))
         return 1;
      return i;
    }
  return i;
}

/* Subroutine of sort_sections to a contiguous subset of a list of sections.
   NEXT_AFTER is the element after the last one to sort.
   The result is a pointer to the last element's "next" pointer.  */

static lang_statement_union_type **
sort_sections_1 (lang_statement_union_type **startptr,
		 lang_statement_union_type *next_after,
		 int count,
		 int (*sort_func) (const void *, const void *))
{
  lang_statement_union_type **vec;
  lang_statement_union_type *p;
  int i;
  lang_statement_union_type **ret;

  if (count == 0)
    return startptr;

  vec = ((lang_statement_union_type **)
	 xmalloc (count * sizeof (lang_statement_union_type *)));

  for (p = *startptr, i = 0; i < count; i++, p = p->header.next)
    vec[i] = p;

  qsort (vec, count, sizeof (vec[0]), sort_func);

  /* Fill in the next pointers again. */
  *startptr = vec[0];
  for (i = 0; i < count - 1; i++)
    vec[i]->header.next = vec[i + 1];
  vec[i]->header.next = next_after;
  ret = &vec[i]->header.next;
  free (vec);
  return ret;
}

/* Sort the .idata\$foo input sections of archives into filename order.
   The reason is so dlltool can arrange to have the pe dll import information
   generated correctly - the head of the list goes into dh.o, the tail into
   dt.o, and the guts into ds[nnnn].o.  Note that this is only needed for the
   .idata section.
   FIXME: This may no longer be necessary with grouped sections.  Instead of
   sorting on dh.o, ds[nnnn].o, dt.o, one could, for example, have dh.o use
   .idata\$4h, have ds[nnnn].o use .idata\$4s[nnnn], and have dt.o use .idata\$4t.
   This would have to be elaborated upon to handle multiple dll's
   [assuming such an eloboration is possible of course].

   We also sort sections in '\$' wild statements.  These are created by the
   place_orphans routine to implement grouped sections.  */

static void
sort_sections (lang_statement_union_type *s)
{
  for (; s ; s = s->header.next)
    switch (s->header.type)
      {
      case lang_output_section_statement_enum:
	sort_sections (s->output_section_statement.children.head);
	break;
      case lang_wild_statement_enum:
	{
	  lang_statement_union_type **p = &s->wild_statement.children.head;
	  struct wildcard_list *sec;

	  for (sec = s->wild_statement.section_list; sec; sec = sec->next)
	    {
	      /* Is this the .idata section?  */
	      if (sec->spec.name != NULL
		  && strncmp (sec->spec.name, ".idata", 6) == 0)
		{
		  /* Sort the children.  We want to sort any objects in
		     the same archive.  In order to handle the case of
		     including a single archive multiple times, we sort
		     all the children by archive name and then by object
		     name.  After sorting them, we re-thread the pointer
		     chain.  */

		  while (*p)
		    {
		      lang_statement_union_type *start = *p;
		      if (start->header.type != lang_input_section_enum
			  || !start->input_section.ifile->the_bfd->my_archive)
			p = &(start->header.next);
		      else
			{
			  lang_statement_union_type *end;
			  int count;

			  for (end = start, count = 0;
			       end && (end->header.type
				       == lang_input_section_enum);
			       end = end->header.next)
			    count++;

			  p = sort_sections_1 (p, end, count,
					       sort_by_file_name);
			}
		    }
		  break;
		}

	      /* If this is a collection of grouped sections, sort them.
		 The linker script must explicitly mention "*(.foo\$)" or
		 "*(.foo\$*)".  Don't sort them if \$ is not the last
		 character (not sure if this is really useful, but it
		 allows explicitly mentioning some \$ sections and letting
		 the linker handle the rest).  */
	      if (sec->spec.name != NULL)
		{
		  char *q = strchr (sec->spec.name, '\$');

		  if (q != NULL
		      && (q[1] == '\0'
			  || (q[1] == '*' && q[2] == '\0')))
		    {
		      lang_statement_union_type *end;
		      int count;

		      for (end = *p, count = 0; end; end = end->header.next)
			{
			  if (end->header.type != lang_input_section_enum)
			    abort ();
			  count++;
			}
		      (void) sort_sections_1 (p, end, count,
					      sort_by_section_name);
		    }
		  break;
		}
	    }
	}
	break;
      default:
	break;
      }
}

static void
gld_${EMULATION_NAME}_before_allocation (void)
{
  extern lang_statement_list_type *stat_ptr;

#ifdef TARGET_IS_ppcpe
  /* Here we rummage through the found bfds to collect toc information */
  {
    LANG_FOR_EACH_INPUT_STATEMENT (is)
      {
	if (!ppc_process_before_allocation(is->the_bfd, &link_info))
	  {
	    einfo("Errors encountered processing file %s\n", is->filename);
	  }
      }
  }

  /* We have seen it all. Allocate it, and carry on */
  ppc_allocate_toc_section (&link_info);
#else
#ifdef TARGET_IS_armpe
  /* FIXME: we should be able to set the size of the interworking stub
     section.

     Here we rummage through the found bfds to collect glue
     information.  FIXME: should this be based on a command line
     option?  krk@cygnus.com */
  {
    LANG_FOR_EACH_INPUT_STATEMENT (is)
      {
	if (!arm_process_before_allocation (is->the_bfd, & link_info))
	  {
	    einfo ("Errors encountered processing file %s", is->filename);
	  }
      }
  }

  /* We have seen it all. Allocate it, and carry on */
  arm_allocate_interworking_sections (& link_info);
#endif /* TARGET_IS_armpe */
#endif /* TARGET_IS_ppcpe */

  sort_sections (stat_ptr->head);
}

/* Place an orphan section.  We use this to put sections with a '\$' in them
   into the right place.  Any section with a '\$' in them (e.g. .text\$foo)
   gets mapped to the output section with everything from the '\$' on stripped
   (e.g. .text).
   See the Microsoft Portable Executable and Common Object File Format
   Specification 4.1, section 4.2, Grouped Sections.

   FIXME: This is now handled by the linker script using wildcards,
   but I'm leaving this here in case we want to enable it for sections
   which are not mentioned in the linker script.  */

static bfd_boolean
gld${EMULATION_NAME}_place_orphan (lang_input_statement_type *file, asection *s)
{
  const char *secname;
  char *output_secname, *ps;
  lang_output_section_statement_type *os;
  lang_statement_union_type *l;

  if ((s->flags & SEC_ALLOC) == 0)
    return FALSE;

  /* Don't process grouped sections unless doing a final link.
     If they're marked as COMDAT sections, we don't want .text\$foo to
     end up in .text and then have .text disappear because it's marked
     link-once-discard.  */
  if (link_info.relocatable)
    return FALSE;

  secname = bfd_get_section_name (s->owner, s);

  /* Everything from the '\$' on gets deleted so don't allow '\$' as the
     first character.  */
  if (*secname == '\$')
    einfo ("%P%F: section %s has '\$' as first character\n", secname);
  if (strchr (secname + 1, '\$') == NULL)
    return FALSE;

  /* Look up the output section.  The Microsoft specs say sections names in
     image files never contain a '\$'.  Fortunately, lang_..._lookup creates
     the section if it doesn't exist.  */
  output_secname = xstrdup (secname);
  ps = strchr (output_secname + 1, '\$');
  *ps = 0;
  os = lang_output_section_statement_lookup (output_secname);

  /* Find the '\$' wild statement for this section.  We currently require the
     linker script to explicitly mention "*(.foo\$)".
     FIXME: ppcpe.sc has .CRT\$foo in the .rdata section.  According to the
     Microsoft docs this isn't correct so it's not (currently) handled.  */

  ps[0] = '\$';
  ps[1] = 0;
  for (l = os->children.head; l; l = l->header.next)
    if (l->header.type == lang_wild_statement_enum)
      {
	struct wildcard_list *sec;

	for (sec = l->wild_statement.section_list; sec; sec = sec->next)
	  if (sec->spec.name && strcmp (sec->spec.name, output_secname) == 0)
	    break;
	if (sec)
	  break;
      }
  ps[0] = 0;
  if (l == NULL)
#if 1
    einfo ("%P%F: *(%s\$) missing from linker script\n", output_secname);
#else /* FIXME: This block is untried.  It exists to convey the intent,
	 should one decide to not require *(.foo\$) to appear in the linker
	 script.  */
    {
      lang_wild_statement_type *new;
      struct wildcard_list *tmp;

      tmp = (struct wildcard_list *) xmalloc (sizeof *tmp);
      tmp->next = NULL;
      tmp->spec.name = xmalloc (strlen (output_secname) + 2);
      sprintf (tmp->spec.name, "%s\$", output_secname);
      tmp->spec.exclude_name_list = NULL;
      tmp->sorted = FALSE;
      new = new_stat (lang_wild_statement, &os->children);
      new->filename = NULL;
      new->filenames_sorted = FALSE;
      new->section_list = tmp;
      new->keep_sections = FALSE;
      lang_list_init (&new->children);
      l = new;
    }
#endif

  /* Link the input section in and we're done for now.
     The sections still have to be sorted, but that has to wait until
     all such sections have been processed by us.  The sorting is done by
     sort_sections.  */
  lang_add_section (&l->wild_statement.children, s, os, file);

  return TRUE;
}

static char *
gld_${EMULATION_NAME}_get_script (int *isfile)
EOF
# Scripts compiled in.
# sed commands to quote an ld script as a C string.
sc="-f stringify.sed"

cat >>e${EMULATION_NAME}.c <<EOF
{
  *isfile = 0;

  if (link_info.relocatable && config.build_constructors)
    return
EOF
sed $sc ldscripts/${EMULATION_NAME}.xu                 >> e${EMULATION_NAME}.c
echo '  ; else if (link_info.relocatable) return'     >> e${EMULATION_NAME}.c
sed $sc ldscripts/${EMULATION_NAME}.xr                 >> e${EMULATION_NAME}.c
echo '  ; else if (!config.text_read_only) return'     >> e${EMULATION_NAME}.c
sed $sc ldscripts/${EMULATION_NAME}.xbn                >> e${EMULATION_NAME}.c
echo '  ; else if (!config.magic_demand_paged) return' >> e${EMULATION_NAME}.c
sed $sc ldscripts/${EMULATION_NAME}.xn                 >> e${EMULATION_NAME}.c
echo '  ; else return'                                 >> e${EMULATION_NAME}.c
sed $sc ldscripts/${EMULATION_NAME}.x                  >> e${EMULATION_NAME}.c
echo '; }'                                             >> e${EMULATION_NAME}.c

cat >>e${EMULATION_NAME}.c <<EOF


struct ld_emulation_xfer_struct ld_${EMULATION_NAME}_emulation =
{
  gld_${EMULATION_NAME}_before_parse,
  syslib_default,
  hll_default,
  after_parse_default,
  gld_${EMULATION_NAME}_after_open,
  after_allocation_default,
  set_output_arch_default,
  ldemul_default_target,
  gld_${EMULATION_NAME}_before_allocation,
  gld_${EMULATION_NAME}_get_script,
  "${EMULATION_NAME}",
  "${OUTPUT_FORMAT}",
  NULL, /* finish */
  NULL, /* create output section statements */
  NULL, /* open dynamic archive */
  gld${EMULATION_NAME}_place_orphan,
  gld_${EMULATION_NAME}_set_symbols,
  NULL, /* parse_args */
  gld${EMULATION_NAME}_add_options,
  gld${EMULATION_NAME}_handle_option,
  NULL,	/* unrecognized file */
  NULL,	/* list options */
  NULL,	/* recognized file */
  NULL,	/* find_potential_libraries */
  NULL	/* new_vers_pattern */
};
EOF
