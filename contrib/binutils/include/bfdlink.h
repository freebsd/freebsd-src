/* bfdlink.h -- header file for BFD link routines
   Copyright 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000
   Free Software Foundation, Inc.
   Written by Steve Chamberlain and Ian Lance Taylor, Cygnus Support.

This file is part of BFD, the Binary File Descriptor library.

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

#ifndef BFDLINK_H
#define BFDLINK_H

/* Which symbols to strip during a link.  */
enum bfd_link_strip
{
  strip_none,		/* Don't strip any symbols.  */
  strip_debugger,	/* Strip debugging symbols.  */
  strip_some,		/* keep_hash is the list of symbols to keep.  */
  strip_all		/* Strip all symbols.  */
};

/* Which local symbols to discard during a link.  This is irrelevant
   if strip_all is used.  */
enum bfd_link_discard
{
  discard_sec_merge,	/* Discard local temporary symbols in SEC_MERGE
			   sections.  */
  discard_none,		/* Don't discard any locals.  */
  discard_l,		/* Discard local temporary symbols.  */
  discard_all		/* Discard all locals.  */
};

/* Describes the type of hash table entry structure being used.
   Different hash table structure have different fields and so
   support different linking features.  */
enum bfd_link_hash_table_type
  {
    bfd_link_generic_hash_table,
    bfd_link_elf_hash_table
  };

/* These are the possible types of an entry in the BFD link hash
   table.  */

enum bfd_link_hash_type
{
  bfd_link_hash_new,		/* Symbol is new.  */
  bfd_link_hash_undefined,	/* Symbol seen before, but undefined.  */
  bfd_link_hash_undefweak,	/* Symbol is weak and undefined.  */
  bfd_link_hash_defined,	/* Symbol is defined.  */
  bfd_link_hash_defweak,	/* Symbol is weak and defined.  */
  bfd_link_hash_common,		/* Symbol is common.  */
  bfd_link_hash_indirect,	/* Symbol is an indirect link.  */
  bfd_link_hash_warning		/* Like indirect, but warn if referenced.  */
};

/* The linking routines use a hash table which uses this structure for
   its elements.  */

struct bfd_link_hash_entry
{
  /* Base hash table entry structure.  */
  struct bfd_hash_entry root;
  /* Type of this entry.  */
  enum bfd_link_hash_type type;

  /* Undefined and common symbols are kept in a linked list through
     this field.  This field is not in the union because that would
     force us to remove entries from the list when we changed their
     type, which would force the list to be doubly linked, which would
     waste more memory.  When an undefined or common symbol is
     created, it should be added to this list, the head of which is in
     the link hash table itself.  As symbols are defined, they need
     not be removed from the list; anything which reads the list must
     doublecheck the symbol type.

     Weak symbols are not kept on this list.

     Defined and defweak symbols use this field as a reference marker.
     If the field is not NULL, or this structure is the tail of the
     undefined symbol list, the symbol has been referenced.  If the
     symbol is undefined and becomes defined, this field will
     automatically be non-NULL since the symbol will have been on the
     undefined symbol list.  */
  struct bfd_link_hash_entry *next;
  /* A union of information depending upon the type.  */
  union
    {
      /* Nothing is kept for bfd_hash_new.  */
      /* bfd_link_hash_undefined, bfd_link_hash_undefweak.  */
      struct
	{
	  bfd *abfd;		/* BFD symbol was found in.  */
	} undef;
      /* bfd_link_hash_defined, bfd_link_hash_defweak.  */
      struct
	{
	  bfd_vma value;	/* Symbol value.  */
	  asection *section;	/* Symbol section.  */
	} def;
      /* bfd_link_hash_indirect, bfd_link_hash_warning.  */
      struct
	{
	  struct bfd_link_hash_entry *link;	/* Real symbol.  */
	  const char *warning;	/* Warning (bfd_link_hash_warning only).  */
	} i;
      /* bfd_link_hash_common.  */
      struct
	{
	  /* The linker needs to know three things about common
             symbols: the size, the alignment, and the section in
             which the symbol should be placed.  We store the size
             here, and we allocate a small structure to hold the
             section and the alignment.  The alignment is stored as a
             power of two.  We don't store all the information
             directly because we don't want to increase the size of
             the union; this structure is a major space user in the
             linker.  */
	  bfd_size_type size;	/* Common symbol size.  */
	  struct bfd_link_hash_common_entry
	    {
	      unsigned int alignment_power;	/* Alignment.  */
	      asection *section;		/* Symbol section.  */
	    } *p;
	} c;
    } u;
};

/* This is the link hash table.  It is a derived class of
   bfd_hash_table.  */

struct bfd_link_hash_table
{
  /* The hash table itself.  */
  struct bfd_hash_table table;
  /* The back end which created this hash table.  This indicates the
     type of the entries in the hash table, which is sometimes
     important information when linking object files of different
     types together.  */
  const bfd_target *creator;
  /* A linked list of undefined and common symbols, linked through the
     next field in the bfd_link_hash_entry structure.  */
  struct bfd_link_hash_entry *undefs;
  /* Entries are added to the tail of the undefs list.  */
  struct bfd_link_hash_entry *undefs_tail;
  /* The type of the ink hash table.  */
  enum bfd_link_hash_table_type type;
};

/* Look up an entry in a link hash table.  If FOLLOW is true, this
   follows bfd_link_hash_indirect and bfd_link_hash_warning links to
   the real symbol.  */
extern struct bfd_link_hash_entry *bfd_link_hash_lookup
  PARAMS ((struct bfd_link_hash_table *, const char *, boolean create,
	   boolean copy, boolean follow));

/* Look up an entry in the main linker hash table if the symbol might
   be wrapped.  This should only be used for references to an
   undefined symbol, not for definitions of a symbol.  */

extern struct bfd_link_hash_entry *bfd_wrapped_link_hash_lookup
  PARAMS ((bfd *, struct bfd_link_info *, const char *, boolean, boolean,
	   boolean));

/* Traverse a link hash table.  */
extern void bfd_link_hash_traverse
  PARAMS ((struct bfd_link_hash_table *,
	   boolean (*) (struct bfd_link_hash_entry *, PTR),
	   PTR));

/* Add an entry to the undefs list.  */
extern void bfd_link_add_undef
  PARAMS ((struct bfd_link_hash_table *, struct bfd_link_hash_entry *));

/* This structure holds all the information needed to communicate
   between BFD and the linker when doing a link.  */

struct bfd_link_info
{
  /* Function callbacks.  */
  const struct bfd_link_callbacks *callbacks;
  /* true if BFD should generate a relocateable object file.  */
  boolean relocateable;
  /* true if BFD should generate relocation information in the final executable.  */
  boolean emitrelocations;
  /* true if BFD should generate a "task linked" object file,
     similar to relocatable but also with globals converted to statics. */
  boolean task_link;
  /* true if BFD should generate a shared object.  */
  boolean shared;
  /* true if BFD should pre-bind symbols in a shared object.  */
  boolean symbolic;
  /* true if BFD should export all symbols in the dynamic symbol table
     of an executable, rather than only those used.  */
  boolean export_dynamic;
  /* true if shared objects should be linked directly, not shared.  */
  boolean static_link;
  /* true if the output file should be in a traditional format.  This
     is equivalent to the setting of the BFD_TRADITIONAL_FORMAT flag
     on the output file, but may be checked when reading the input
     files.  */
  boolean traditional_format;
  /* true if we want to produced optimized output files.  This might
     need much more time and therefore must be explicitly selected.  */
  boolean optimize;
  /* true if BFD should generate errors for undefined symbols
     even if generating a shared object.  */
  boolean no_undefined;
  /* true if BFD should allow undefined symbols in shared objects even
     when no_undefined is set to disallow undefined symbols.  The net
     result will be that undefined symbols in regular objects will
     still trigger an error, but undefined symbols in shared objects
     will be ignored.  The implementation of no_undefined makes the
     assumption that the runtime linker will choke on undefined
     symbols.  However there is at least one system (BeOS) where
     undefined symbols in shared libraries is normal since the kernel
     patches them at load time to select which function is most
     appropriate for the current architecture.  I.E. dynamically
     select an appropriate memset function.  Apparently it is also
     normal for HPPA shared libraries to have undefined symbols.  */
  boolean allow_shlib_undefined;
  /* Which symbols to strip.  */
  enum bfd_link_strip strip;
  /* Which local symbols to discard.  */
  enum bfd_link_discard discard;
  /* true if symbols should be retained in memory, false if they
     should be freed and reread.  */
  boolean keep_memory;
  /* The list of input BFD's involved in the link.  These are chained
     together via the link_next field.  */
  bfd *input_bfds;
  /* If a symbol should be created for each input BFD, this is section
     where those symbols should be placed.  It must be a section in
     the output BFD.  It may be NULL, in which case no such symbols
     will be created.  This is to support CREATE_OBJECT_SYMBOLS in the
     linker command language.  */
  asection *create_object_symbols_section;
  /* Hash table handled by BFD.  */
  struct bfd_link_hash_table *hash;
  /* Hash table of symbols to keep.  This is NULL unless strip is
     strip_some.  */
  struct bfd_hash_table *keep_hash;
  /* true if every symbol should be reported back via the notice
     callback.  */
  boolean notice_all;
  /* Hash table of symbols to report back via the notice callback.  If
     this is NULL, and notice_all is false, then no symbols are
     reported back.  */
  struct bfd_hash_table *notice_hash;
  /* Hash table of symbols which are being wrapped (the --wrap linker
     option).  If this is NULL, no symbols are being wrapped.  */
  struct bfd_hash_table *wrap_hash;
  /* If a base output file is wanted, then this points to it */
  PTR base_file;

  /* If non-zero, specifies that branches which are problematic for the
  MPC860 C0 (or earlier) should be checked for and modified.  It gives the
  number of bytes that should be checked at the end of each text page. */
  int mpc860c0;

  /* The function to call when the executable or shared object is
     loaded.  */
  const char *init_function;
  /* The function to call when the executable or shared object is
     unloaded.  */
  const char *fini_function;

  /* true if the new ELF dynamic tags are enabled. */ 
  boolean new_dtags;

  /* May be used to set DT_FLAGS for ELF. */
  bfd_vma flags;

  /* May be used to set DT_FLAGS_1 for ELF. */
  bfd_vma flags_1;

  /* True if auto-import thunks for DATA items in pei386 DLLs 
     should be generated/linked against.  */
  boolean pei386_auto_import;

  /* True if non-PLT relocs should be merged into one reloc section
     and sorted so that relocs against the same symbol come together.  */
  boolean combreloc;

  /* True if executable should not contain copy relocs.
     Setting this true may result in a non-sharable text segment.  */
  boolean nocopyreloc;

  /* True if .eh_frame_hdr section and PT_GNU_EH_FRAME ELF segment
     should be created.  */
  boolean eh_frame_hdr;

  /* How many spare .dynamic DT_NULL entries should be added?  */
  unsigned int spare_dynamic_tags;
};

/* This structures holds a set of callback functions.  These are
   called by the BFD linker routines.  The first argument to each
   callback function is the bfd_link_info structure being used.  Each
   function returns a boolean value.  If the function returns false,
   then the BFD function which called it will return with a failure
   indication.  */

struct bfd_link_callbacks
{
  /* A function which is called when an object is added from an
     archive.  ABFD is the archive element being added.  NAME is the
     name of the symbol which caused the archive element to be pulled
     in.  */
  boolean (*add_archive_element) PARAMS ((struct bfd_link_info *,
					  bfd *abfd,
					  const char *name));
  /* A function which is called when a symbol is found with multiple
     definitions.  NAME is the symbol which is defined multiple times.
     OBFD is the old BFD, OSEC is the old section, OVAL is the old
     value, NBFD is the new BFD, NSEC is the new section, and NVAL is
     the new value.  OBFD may be NULL.  OSEC and NSEC may be
     bfd_com_section or bfd_ind_section.  */
  boolean (*multiple_definition) PARAMS ((struct bfd_link_info *,
					  const char *name,
					  bfd *obfd,
					  asection *osec,
					  bfd_vma oval,
					  bfd *nbfd,
					  asection *nsec,
					  bfd_vma nval));
  /* A function which is called when a common symbol is defined
     multiple times.  NAME is the symbol appearing multiple times.
     OBFD is the BFD of the existing symbol; it may be NULL if this is
     not known.  OTYPE is the type of the existing symbol, which may
     be bfd_link_hash_defined, bfd_link_hash_defweak,
     bfd_link_hash_common, or bfd_link_hash_indirect.  If OTYPE is
     bfd_link_hash_common, OSIZE is the size of the existing symbol.
     NBFD is the BFD of the new symbol.  NTYPE is the type of the new
     symbol, one of bfd_link_hash_defined, bfd_link_hash_common, or
     bfd_link_hash_indirect.  If NTYPE is bfd_link_hash_common, NSIZE
     is the size of the new symbol.  */
  boolean (*multiple_common) PARAMS ((struct bfd_link_info *,
				      const char *name,
				      bfd *obfd,
				      enum bfd_link_hash_type otype,
				      bfd_vma osize,
				      bfd *nbfd,
				      enum bfd_link_hash_type ntype,
				      bfd_vma nsize));
  /* A function which is called to add a symbol to a set.  ENTRY is
     the link hash table entry for the set itself (e.g.,
     __CTOR_LIST__).  RELOC is the relocation to use for an entry in
     the set when generating a relocateable file, and is also used to
     get the size of the entry when generating an executable file.
     ABFD, SEC and VALUE identify the value to add to the set.  */
  boolean (*add_to_set) PARAMS ((struct bfd_link_info *,
				 struct bfd_link_hash_entry *entry,
				 bfd_reloc_code_real_type reloc,
				 bfd *abfd, asection *sec, bfd_vma value));
  /* A function which is called when the name of a g++ constructor or
     destructor is found.  This is only called by some object file
     formats.  CONSTRUCTOR is true for a constructor, false for a
     destructor.  This will use BFD_RELOC_CTOR when generating a
     relocateable file.  NAME is the name of the symbol found.  ABFD,
     SECTION and VALUE are the value of the symbol.  */
  boolean (*constructor) PARAMS ((struct bfd_link_info *,
				  boolean constructor,
				  const char *name, bfd *abfd, asection *sec,
				  bfd_vma value));
  /* A function which is called to issue a linker warning.  For
     example, this is called when there is a reference to a warning
     symbol.  WARNING is the warning to be issued.  SYMBOL is the name
     of the symbol which triggered the warning; it may be NULL if
     there is none.  ABFD, SECTION and ADDRESS identify the location
     which trigerred the warning; either ABFD or SECTION or both may
     be NULL if the location is not known.  */
  boolean (*warning) PARAMS ((struct bfd_link_info *,
			      const char *warning, const char *symbol,
			      bfd *abfd, asection *section,
			      bfd_vma address));
  /* A function which is called when a relocation is attempted against
     an undefined symbol.  NAME is the symbol which is undefined.
     ABFD, SECTION and ADDRESS identify the location from which the
     reference is made. FATAL indicates whether an undefined symbol is
     a fatal error or not. In some cases SECTION may be NULL.  */
  boolean (*undefined_symbol) PARAMS ((struct bfd_link_info *,
				       const char *name, bfd *abfd,
				       asection *section,
				       bfd_vma address,
				       boolean fatal));
  /* A function which is called when a reloc overflow occurs.  NAME is
     the name of the symbol or section the reloc is against,
     RELOC_NAME is the name of the relocation, and ADDEND is any
     addend that is used.  ABFD, SECTION and ADDRESS identify the
     location at which the overflow occurs; if this is the result of a
     bfd_section_reloc_link_order or bfd_symbol_reloc_link_order, then
     ABFD will be NULL.  */
  boolean (*reloc_overflow) PARAMS ((struct bfd_link_info *,
				     const char *name,
				     const char *reloc_name, bfd_vma addend,
				     bfd *abfd, asection *section,
				     bfd_vma address));
  /* A function which is called when a dangerous reloc is performed.
     The canonical example is an a29k IHCONST reloc which does not
     follow an IHIHALF reloc.  MESSAGE is an appropriate message.
     ABFD, SECTION and ADDRESS identify the location at which the
     problem occurred; if this is the result of a
     bfd_section_reloc_link_order or bfd_symbol_reloc_link_order, then
     ABFD will be NULL.  */
  boolean (*reloc_dangerous) PARAMS ((struct bfd_link_info *,
				      const char *message,
				      bfd *abfd, asection *section,
				      bfd_vma address));
  /* A function which is called when a reloc is found to be attached
     to a symbol which is not being written out.  NAME is the name of
     the symbol.  ABFD, SECTION and ADDRESS identify the location of
     the reloc; if this is the result of a
     bfd_section_reloc_link_order or bfd_symbol_reloc_link_order, then
     ABFD will be NULL.  */
  boolean (*unattached_reloc) PARAMS ((struct bfd_link_info *,
				       const char *name,
				       bfd *abfd, asection *section,
				       bfd_vma address));
  /* A function which is called when a symbol in notice_hash is
     defined or referenced.  NAME is the symbol.  ABFD, SECTION and
     ADDRESS are the value of the symbol.  If SECTION is
     bfd_und_section, this is a reference.  */
  boolean (*notice) PARAMS ((struct bfd_link_info *, const char *name,
			     bfd *abfd, asection *section, bfd_vma address));
};

/* The linker builds link_order structures which tell the code how to
   include input data in the output file.  */

/* These are the types of link_order structures.  */

enum bfd_link_order_type
{
  bfd_undefined_link_order,	/* Undefined.  */
  bfd_indirect_link_order,	/* Built from a section.  */
  bfd_fill_link_order,		/* Fill with a 16 bit constant.  */
  bfd_data_link_order,		/* Set to explicit data.  */
  bfd_section_reloc_link_order,	/* Relocate against a section.  */
  bfd_symbol_reloc_link_order	/* Relocate against a symbol.  */
};

/* This is the link_order structure itself.  These form a chain
   attached to the section whose contents they are describing.  */

struct bfd_link_order 
{
  /* Next link_order in chain.  */
  struct bfd_link_order *next;
  /* Type of link_order.  */
  enum bfd_link_order_type type;
  /* Offset within output section.  */
  bfd_vma offset;  
  /* Size within output section.  */
  bfd_size_type size;
  /* Type specific information.  */
  union 
    {
      struct 
	{
	  /* Section to include.  If this is used, then
	     section->output_section must be the section the
	     link_order is attached to, section->output_offset must
	     equal the link_order offset field, and section->_raw_size
	     must equal the link_order size field.  Maybe these
	     restrictions should be relaxed someday.  */
	  asection *section;
	} indirect;
      struct
	{
	  /* Value to fill with.  */
	  unsigned int value;
	} fill;
      struct
	{
	  /* Data to put into file.  The size field gives the number
	     of bytes which this field points to.  */
	  bfd_byte *contents;
	} data;
      struct
	{
	  /* Description of reloc to generate.  Used for
	     bfd_section_reloc_link_order and
	     bfd_symbol_reloc_link_order.  */
	  struct bfd_link_order_reloc *p;
	} reloc;
    } u;
};

/* A linker order of type bfd_section_reloc_link_order or
   bfd_symbol_reloc_link_order means to create a reloc against a
   section or symbol, respectively.  This is used to implement -Ur to
   generate relocs for the constructor tables.  The
   bfd_link_order_reloc structure describes the reloc that BFD should
   create.  It is similar to a arelent, but I didn't use arelent
   because the linker does not know anything about most symbols, and
   any asymbol structure it creates will be partially meaningless.
   This information could logically be in the bfd_link_order struct,
   but I didn't want to waste the space since these types of relocs
   are relatively rare.  */

struct bfd_link_order_reloc
{
  /* Reloc type.  */
  bfd_reloc_code_real_type reloc;

  union
    {
      /* For type bfd_section_reloc_link_order, this is the section
	 the reloc should be against.  This must be a section in the
	 output BFD, not any of the input BFDs.  */
      asection *section;
      /* For type bfd_symbol_reloc_link_order, this is the name of the
	 symbol the reloc should be against.  */
      const char *name;
    } u;

  /* Addend to use.  The object file should contain zero.  The BFD
     backend is responsible for filling in the contents of the object
     file correctly.  For some object file formats (e.g., COFF) the
     addend must be stored into in the object file, and for some
     (e.g., SPARC a.out) it is kept in the reloc.  */
  bfd_vma addend;
};

/* Allocate a new link_order for a section.  */
extern struct bfd_link_order *bfd_new_link_order PARAMS ((bfd *, asection *));

/* These structures are used to describe version information for the
   ELF linker.  These structures could be manipulated entirely inside
   BFD, but it would be a pain.  Instead, the regular linker sets up
   these structures, and then passes them into BFD.  */

/* Regular expressions for a version.  */

struct bfd_elf_version_expr
{
  /* Next regular expression for this version.  */
  struct bfd_elf_version_expr *next;
  /* Regular expression.  */
  const char *pattern;
  /* Matching function.  */
  int (*match) PARAMS((struct bfd_elf_version_expr *, const char *));
};

/* Version dependencies.  */

struct bfd_elf_version_deps
{
  /* Next dependency for this version.  */
  struct bfd_elf_version_deps *next;
  /* The version which this version depends upon.  */
  struct bfd_elf_version_tree *version_needed;
};

/* A node in the version tree.  */

struct bfd_elf_version_tree
{
  /* Next version.  */
  struct bfd_elf_version_tree *next;
  /* Name of this version.  */
  const char *name;
  /* Version number.  */
  unsigned int vernum;
  /* Regular expressions for global symbols in this version.  */
  struct bfd_elf_version_expr *globals;
  /* Regular expressions for local symbols in this version.  */
  struct bfd_elf_version_expr *locals;
  /* List of versions which this version depends upon.  */
  struct bfd_elf_version_deps *deps;
  /* Index of the version name.  This is used within BFD.  */
  unsigned int name_indx;
  /* Whether this version tree was used.  This is used within BFD.  */
  int used;
};

#endif
