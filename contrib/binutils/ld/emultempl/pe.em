# This shell script emits a C file. -*- C -*-
# It does some substitutions.
cat >e${EMULATION_NAME}.c <<EOF
/* This file is part of GLD, the Gnu Linker.
   Copyright 1995, 96, 97, 1998 Free Software Foundation, Inc.

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
#include "ldgram.h"
#include "ldexp.h"
#include "ldlang.h"
#include "ldemul.h"
#include "ldlex.h"
#include "ldmisc.h"
#include "ldctor.h"
#include "ldfile.h"
#include "coff/internal.h"
#include "../bfd/libcoff.h"

#define TARGET_IS_${EMULATION_NAME}

static void gld_${EMULATION_NAME}_set_symbols PARAMS ((void));
static void gld_${EMULATION_NAME}_after_open PARAMS ((void));
static void gld_${EMULATION_NAME}_before_parse PARAMS ((void));
static void gld_${EMULATION_NAME}_before_allocation PARAMS ((void));
static boolean gld${EMULATION_NAME}_place_orphan
  PARAMS ((lang_input_statement_type *, asection *));
static char *gld_${EMULATION_NAME}_get_script PARAMS ((int *));
static int gld_${EMULATION_NAME}_parse_args PARAMS ((int, char **));

#if 0 /* argument to qsort so don't prototype */
static int sort_by_file_name PARAMS ((void *, void *));
static int sort_by_section_name PARAMS ((void *, void *));
#endif
static lang_statement_union_type **sort_sections_1
  PARAMS ((lang_statement_union_type **, lang_statement_union_type *, int,
	   int (*)()));
static void sort_sections PARAMS ((lang_statement_union_type *));

static struct internal_extra_pe_aouthdr pe;
static int dll;

extern const char *output_filename;

static void
gld_${EMULATION_NAME}_before_parse()
{
  output_filename = "a.exe";
  ldfile_output_architecture = bfd_arch_${ARCH};
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

static struct option longopts[] = {
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
  D(ImageBase,"__image_base__", NT_EXE_IMAGE_BASE),
#define DLLOFF 1
  {&dll, sizeof(dll), 0, "__dll__"},
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
set_pe_name (name, val)
     char *name;
     long val;
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
set_pe_subsystem ()
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
      { "console", 3, "_mainCRTStartup" },
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
	     symbol.  The reason we do this is so that the user
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
	  ldlang_add_undef (v[i].entry);

	  return;
	}
    }
  einfo ("%P%F: invalid subsystem type %s\n", optarg);
}



static void
set_pe_value (name)
     char *name;
     
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
set_pe_stack_heap (resname, comname)
     char *resname;
     char *comname;
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



static int
gld_${EMULATION_NAME}_parse_args(argc, argv)
     int argc;
     char **argv;
{
  int longind;
  int optc;
  int prevoptind = optind;
  int prevopterr = opterr;
  int wanterror;
  static int lastoptind = -1;

  if (lastoptind != optind)
    opterr = 0;
  wanterror = opterr;

  lastoptind = optind;

  optc = getopt_long_only (argc, argv, "-", longopts, &longind);
  opterr = prevopterr;

  switch (optc)
    {
    default:
      if (wanterror)
	xexit (1);
      optind =  prevoptind;
      return 0;

    case OPTION_BASE_FILE:
      link_info.base_file = (PTR) fopen (optarg, FOPEN_WB);
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
  return 1;
}

/* Assign values to the special symbols before the linker script is
   read.  */

static void
gld_${EMULATION_NAME}_set_symbols()
{
  /* Run through and invent symbols for all the
     names and insert the defaults. */
  int j;
  lang_statement_list_type *save;

  if (!init[IMAGEBASEOFF].inited)
    {
      if (link_info.relocateable)
	init[IMAGEBASEOFF].value = 0;
      else if (init[DLLOFF].value)
	init[IMAGEBASEOFF].value = NT_DLL_IMAGE_BASE;
      else
	init[IMAGEBASEOFF].value = NT_EXE_IMAGE_BASE;
    }

  /* Don't do any symbol assignments if this is a relocateable link.  */
  if (link_info.relocateable)
    return;

  /* Glue the assignments into the abs section */
  save = stat_ptr;

  stat_ptr = &(abs_output_section->children);

  for (j = 0; init[j].ptr; j++)
    {
      long val = init[j].value;
      lang_add_assignment (exp_assop ('=' ,init[j].symbol, exp_intop (val)));
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
gld_${EMULATION_NAME}_after_open()
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
sort_by_file_name (a, b)
     void *a;
     void *b;
{
  lang_statement_union_type **ra = a;
  lang_statement_union_type **rb = b;
  int i;

  i = strcmp ((*ra)->input_section.ifile->the_bfd->my_archive->filename,
	      (*rb)->input_section.ifile->the_bfd->my_archive->filename);
  if (i != 0)
    return i;

  return strcmp ((*ra)->input_section.ifile->filename,
		 (*rb)->input_section.ifile->filename);
}

static int
sort_by_section_name (a, b)
     void *a;
     void *b;
{
  lang_statement_union_type **ra = a;
  lang_statement_union_type **rb = b;
  return strcmp ((*ra)->input_section.section->name,
		 (*rb)->input_section.section->name);
}

/* Subroutine of sort_sections to a contiguous subset of a list of sections.
   NEXT_AFTER is the element after the last one to sort.
   The result is a pointer to the last element's "next" pointer.  */

static lang_statement_union_type **
sort_sections_1 (startptr, next_after, count, sort_func)
     lang_statement_union_type **startptr,*next_after;
     int count;
     int (*sort_func) ();
{
  lang_statement_union_type **vec;
  lang_statement_union_type *p;
  int i;
  lang_statement_union_type **ret;

  if (count == 0)
    return startptr;

  vec = ((lang_statement_union_type **)
	 xmalloc (count * sizeof (lang_statement_union_type *)));

  for (p = *startptr, i = 0; i < count; i++, p = p->next)
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
sort_sections (s)
     lang_statement_union_type *s;
{
  for (; s ; s = s->next)
    switch (s->header.type)
      {
      case lang_output_section_statement_enum:
	sort_sections (s->output_section_statement.children.head);
	break;
      case lang_wild_statement_enum:
	{
	  lang_statement_union_type **p = &s->wild_statement.children.head;

	  /* Is this the .idata section?  */
	  if (s->wild_statement.section_name != NULL
	      && strncmp (s->wild_statement.section_name, ".idata", 6) == 0)
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
			   end && end->header.type == lang_input_section_enum;
			   end = end->next)
			count++;

		      p = sort_sections_1 (p, end, count, sort_by_file_name);
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
	  if (s->wild_statement.section_name != NULL)
	    {
	      char *q = strchr (s->wild_statement.section_name, '\$');

	      if (q != NULL
		  && (q[1] == '\0'
		      || (q[1] == '*' && q[2] == '\0')))
		{
		  lang_statement_union_type *end;
		  int count;

		  for (end = *p, count = 0; end; end = end->next)
		    {
		      if (end->header.type != lang_input_section_enum)
			abort ();
		      count++;
		    }
		  (void) sort_sections_1 (p, end, count, sort_by_section_name);
		}
	      break;
	    }
	}
	break;
      default:
	break;
      }
}

static void  
gld_${EMULATION_NAME}_before_allocation()
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

/*ARGSUSED*/
static boolean
gld${EMULATION_NAME}_place_orphan (file, s)
     lang_input_statement_type *file;
     asection *s;
{
  const char *secname;
  char *output_secname, *ps;
  lang_output_section_statement_type *os;
  lang_statement_union_type *l;

  if ((s->flags & SEC_ALLOC) == 0)
    return false;

  /* Don't process grouped sections unless doing a final link.
     If they're marked as COMDAT sections, we don't want .text\$foo to
     end up in .text and then have .text disappear because it's marked
     link-once-discard.  */
  if (link_info.relocateable)
    return false;

  secname = bfd_get_section_name (s->owner, s);

  /* Everything from the '\$' on gets deleted so don't allow '\$' as the
     first character.  */
  if (*secname == '\$')
    einfo ("%P%F: section %s has '\$' as first character\n", secname);
  if (strchr (secname + 1, '\$') == NULL)
    return false;

  /* Look up the output section.  The Microsoft specs say sections names in
     image files never contain a '\$'.  Fortunately, lang_..._lookup creates
     the section if it doesn't exist.  */
  output_secname = buystring (secname);
  ps = strchr (output_secname + 1, '\$');
  *ps = 0;
  os = lang_output_section_statement_lookup (output_secname);

  /* Find the '\$' wild statement for this section.  We currently require the
     linker script to explicitly mention "*(.foo\$)".
     FIXME: ppcpe.sc has .CRT\$foo in the .rdata section.  According to the
     Microsoft docs this isn't correct so it's not (currently) handled.  */

  ps[0] = '\$';
  ps[1] = 0;
  for (l = os->children.head; l; l = l->next)
    {
      if (l->header.type == lang_wild_statement_enum
	  && strcmp (l->wild_statement.section_name, output_secname) == 0)
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
      lang_wild_statement_type *new = new_stat (lang_wild_statement,
						&os->children);
      new->section_name = xmalloc (strlen (output_secname) + 2);
      sprintf (new->section_name, "%s\$", output_secname);
      new->filename = NULL;
      lang_list_init (&new->children);
      l = new;
    }
#endif

  /* Link the input section in and we're done for now.
     The sections still have to be sorted, but that has to wait until
     all such sections have been processed by us.  The sorting is done by
     sort_sections.  */
  wild_doit (&l->wild_statement.children, s, os, file);

  return true;
}

static char *
gld_${EMULATION_NAME}_get_script(isfile)
     int *isfile;
EOF
# Scripts compiled in.
# sed commands to quote an ld script as a C string.
sc="-f ${srcdir}/emultempl/stringify.sed"

cat >>e${EMULATION_NAME}.c <<EOF
{			     
  *isfile = 0;

  if (link_info.relocateable == true && config.build_constructors == true)
    return
EOF
sed $sc ldscripts/${EMULATION_NAME}.xu                     >> e${EMULATION_NAME}.c
echo '  ; else if (link_info.relocateable == true) return' >> e${EMULATION_NAME}.c
sed $sc ldscripts/${EMULATION_NAME}.xr                     >> e${EMULATION_NAME}.c
echo '  ; else if (!config.text_read_only) return'         >> e${EMULATION_NAME}.c
sed $sc ldscripts/${EMULATION_NAME}.xbn                    >> e${EMULATION_NAME}.c
echo '  ; else if (!config.magic_demand_paged) return'     >> e${EMULATION_NAME}.c
sed $sc ldscripts/${EMULATION_NAME}.xn                     >> e${EMULATION_NAME}.c
echo '  ; else return'                                     >> e${EMULATION_NAME}.c
sed $sc ldscripts/${EMULATION_NAME}.x                      >> e${EMULATION_NAME}.c
echo '; }'                                                 >> e${EMULATION_NAME}.c

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
  gld_${EMULATION_NAME}_parse_args
};
EOF
