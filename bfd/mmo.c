/* BFD back-end for mmo objects (MMIX-specific object-format).
   Copyright 2001, 2002, 2003
   Free Software Foundation, Inc.
   Written by Hans-Peter Nilsson (hp@bitrange.com).
   Infrastructure and other bits originally copied from srec.c and
   binary.c.

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

/*
SECTION
	mmo backend

	The mmo object format is used exclusively together with Professor
	Donald E.@: Knuth's educational 64-bit processor MMIX.  The simulator
	@command{mmix} which is available at
	@url{http://www-cs-faculty.stanford.edu/~knuth/programs/mmix.tar.gz}
	understands this format.  That package also includes a combined
	assembler and linker called @command{mmixal}.  The mmo format has
	no advantages feature-wise compared to e.g. ELF.  It is a simple
	non-relocatable object format with no support for archives or
	debugging information, except for symbol value information and
	line numbers (which is not yet implemented in BFD).  See
	@url{http://www-cs-faculty.stanford.edu/~knuth/mmix.html} for more
	information about MMIX.  The ELF format is used for intermediate
	object files in the BFD implementation.

@c We want to xref the symbol table node.  A feature in "chew"
@c requires that "commands" do not contain spaces in the
@c arguments.  Hence the hyphen in "Symbol-table".
@menu
@* File layout::
@* Symbol-table::
@* mmo section mapping::
@end menu

INODE
File layout, Symbol-table, mmo, mmo
SUBSECTION
	File layout

	The mmo file contents is not partitioned into named sections as
	with e.g.@: ELF.  Memory areas is formed by specifying the
	location of the data that follows.  Only the memory area
	@samp{0x0000@dots{}00} to @samp{0x01ff@dots{}ff} is executable, so
	it is used for code (and constants) and the area
	@samp{0x2000@dots{}00} to @samp{0x20ff@dots{}ff} is used for
	writable data.  @xref{mmo section mapping}.

	Contents is entered as 32-bit words, xor:ed over previous
	contents, always zero-initialized.  A word that starts with the
	byte @samp{0x98} forms a command called a @samp{lopcode}, where
	the next byte distinguished between the thirteen lopcodes.  The
	two remaining bytes, called the @samp{Y} and @samp{Z} fields, or
	the @samp{YZ} field (a 16-bit big-endian number), are used for
	various purposes different for each lopcode.  As documented in
	@url{http://www-cs-faculty.stanford.edu/~knuth/mmixal-intro.ps.gz},
	the lopcodes are:

	There is provision for specifying ``special data'' of 65536
	different types.  We use type 80 (decimal), arbitrarily chosen the
	same as the ELF <<e_machine>> number for MMIX, filling it with
	section information normally found in ELF objects. @xref{mmo
	section mapping}.

	@table @code
	@item lop_quote
	0x98000001.  The next word is contents, regardless of whether it
	starts with 0x98 or not.

	@item lop_loc
	0x9801YYZZ, where @samp{Z} is 1 or 2.  This is a location
	directive, setting the location for the next data to the next
	32-bit word (for @math{Z = 1}) or 64-bit word (for @math{Z = 2}),
	plus @math{Y * 2^56}.  Normally @samp{Y} is 0 for the text segment
	and 2 for the data segment.

	@item lop_skip
	0x9802YYZZ.  Increase the current location by @samp{YZ} bytes.

	@item lop_fixo
	0x9803YYZZ, where @samp{Z} is 1 or 2.  Store the current location
	as 64 bits into the location pointed to by the next 32-bit
	(@math{Z = 1}) or 64-bit (@math{Z = 2}) word, plus @math{Y *
	2^56}.

	@item lop_fixr
	0x9804YYZZ.  @samp{YZ} is stored into the current location plus
	@math{2 - 4 * YZ}.

	@item lop_fixrx
	0x980500ZZ.  @samp{Z} is 16 or 24.  A value @samp{L} derived from
	the following 32-bit word are used in a manner similar to
	@samp{YZ} in lop_fixr: it is xor:ed into the current location
	minus @math{4 * L}.  The first byte of the word is 0 or 1.  If it
	is 1, then @math{L = (@var{lowest 24 bits of word}) - 2^Z}, if 0,
 	then @math{L = (@var{lowest 24 bits of word})}.

	@item lop_file
	0x9806YYZZ.  @samp{Y} is the file number, @samp{Z} is count of
	32-bit words.  Set the file number to @samp{Y} and the line
	counter to 0.  The next @math{Z * 4} bytes contain the file name,
	padded with zeros if the count is not a multiple of four.  The
	same @samp{Y} may occur multiple times, but @samp{Z} must be 0 for
	all but the first occurrence.

	@item lop_line
	0x9807YYZZ.  @samp{YZ} is the line number.  Together with
	lop_file, it forms the source location for the next 32-bit word.
	Note that for each non-lopcode 32-bit word, line numbers are
	assumed incremented by one.

	@item lop_spec
	0x9808YYZZ.  @samp{YZ} is the type number.  Data until the next
	lopcode other than lop_quote forms special data of type @samp{YZ}.
	@xref{mmo section mapping}.

	Other types than 80, (or type 80 with a content that does not
	parse) is stored in sections named <<.MMIX.spec_data.@var{n}>>
	where @var{n} is the @samp{YZ}-type.  The flags for such a
	sections say not to allocate or load the data.  The vma is 0.
	Contents of multiple occurrences of special data @var{n} is
	concatenated to the data of the previous lop_spec @var{n}s.  The
	location in data or code at which the lop_spec occurred is lost.

	@item lop_pre
	0x980901ZZ.  The first lopcode in a file.  The @samp{Z} field forms the
	length of header information in 32-bit words, where the first word
	tells the time in seconds since @samp{00:00:00 GMT Jan 1 1970}.

	@item lop_post
	0x980a00ZZ.  @math{Z > 32}.  This lopcode follows after all
	content-generating lopcodes in a program.  The @samp{Z} field
	denotes the value of @samp{rG} at the beginning of the program.
	The following @math{256 - Z} big-endian 64-bit words are loaded
	into global registers @samp{$G} @dots{} @samp{$255}.

	@item lop_stab
	0x980b0000.  The next-to-last lopcode in a program.  Must follow
	immediately after the lop_post lopcode and its data.  After this
	lopcode follows all symbols in a compressed format
	(@pxref{Symbol-table}).

	@item lop_end
	0x980cYYZZ.  The last lopcode in a program.  It must follow the
	lop_stab lopcode and its data.  The @samp{YZ} field contains the
	number of 32-bit words of symbol table information after the
	preceding lop_stab lopcode.
	@end table

	Note that the lopcode "fixups"; <<lop_fixr>>, <<lop_fixrx>> and
	<<lop_fixo>> are not generated by BFD, but are handled.  They are
	generated by <<mmixal>>.

EXAMPLE
	This trivial one-label, one-instruction file:

| :Main TRAP 1,2,3

	can be represented this way in mmo:

| 0x98090101 - lop_pre, one 32-bit word with timestamp.
| <timestamp>
| 0x98010002 - lop_loc, text segment, using a 64-bit address.
|              Note that mmixal does not emit this for the file above.
| 0x00000000 - Address, high 32 bits.
| 0x00000000 - Address, low 32 bits.
| 0x98060002 - lop_file, 2 32-bit words for file-name.
| 0x74657374 - "test"
| 0x2e730000 - ".s\0\0"
| 0x98070001 - lop_line, line 1.
| 0x00010203 - TRAP 1,2,3
| 0x980a00ff - lop_post, setting $255 to 0.
| 0x00000000
| 0x00000000
| 0x980b0000 - lop_stab for ":Main" = 0, serial 1.
| 0x203a4040   @xref{Symbol-table}.
| 0x10404020
| 0x4d206120
| 0x69016e00
| 0x81000000
| 0x980c0005 - lop_end; symbol table contained five 32-bit words.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "libiberty.h"
#include "elf/mmix.h"
#include "opcode/mmix.h"

#define LOP 0x98
#define LOP_QUOTE 0
#define LOP_LOC 1
#define LOP_SKIP 2
#define LOP_FIXO 3
#define LOP_FIXR 4
#define LOP_FIXRX 5
#define LOP_FILE 6
#define LOP_LINE 7
#define LOP_SPEC 8
#define LOP_PRE 9
#define LOP_POST 10
#define LOP_STAB 11
#define LOP_END 12

#define LOP_QUOTE_NEXT ((LOP << 24) | (LOP_QUOTE << 16) | 1)
#define SPEC_DATA_SECTION 80
#define LOP_SPEC_SECTION \
 ((LOP << 24) | (LOP_SPEC << 16) | SPEC_DATA_SECTION)

/* Must be a power of two.  If you change this to be >= 64k, you need a
   new test-case; the ld test b-loc64k.d touches chunk-size problem areas.  */
#define MMO_SEC_CONTENTS_CHUNK_SIZE (1 << 15)

/* An arbitrary number for the maximum length section name size.  */
#define MAX_SECTION_NAME_SIZE (1024 * 1024)

/* A quite arbitrary number for the maximum length section size.  */
#define MAX_ARTIFICIAL_SECTION_SIZE (1024 * 1024 * 1024)

#define MMO3_WCHAR 0x80
#define MMO3_LEFT 0x40
#define MMO3_MIDDLE 0x20
#define MMO3_RIGHT 0x10
#define MMO3_TYPEBITS 0xf
#define MMO3_REGQUAL_BITS 0xf
#define MMO3_UNDEF 2
#define MMO3_DATA 8
#define MMO3_SYMBITS 0x2f

/* Put these everywhere in new code.  */
#define FATAL_DEBUG						\
 _bfd_abort (__FILE__, __LINE__,				\
	     "Internal: Non-debugged code (test-case missing)")

#define BAD_CASE(x)				\
 _bfd_abort (__FILE__, __LINE__,		\
	     "bad case for " #x)

enum mmo_sym_type { mmo_reg_sym, mmo_undef_sym, mmo_data_sym, mmo_abs_sym};

/* When scanning the mmo file, a linked list of mmo_symbol
   structures is built to represent the symbol table (if there is
   one).  */

struct mmo_symbol
  {
    struct mmo_symbol *next;
    const char *name;
    bfd_vma value;
    enum mmo_sym_type sym_type;
    unsigned int serno;
  };

struct mmo_data_list_struct
  {
    struct mmo_data_list_struct *next;
    bfd_vma where;
    bfd_size_type size;
    bfd_size_type allocated_size;
    bfd_byte data[1];
  };

typedef struct mmo_data_list_struct mmo_data_list_type;

struct mmo_symbol_trie
  {
    struct mmo_symbol_trie *left;
    struct mmo_symbol_trie *right;
    struct mmo_symbol_trie *middle;

    bfd_byte symchar;

    /* A zero name means there's nothing here.  */
    struct mmo_symbol sym;
  };

/* The mmo tdata information.  */

struct mmo_data_struct
  {
    struct mmo_symbol *symbols;
    struct mmo_symbol *symtail;
    asymbol *csymbols;

    /* File representation of time (NULL) when this file was created.  */
    bfd_byte created[4];

    /* When we're reading bytes recursively, check this occasionally.
       Also holds write errors.  */
    bfd_boolean have_error;

    /* Max symbol length that may appear in the lop_stab table.  Note that
       this table might just hold a subset of symbols for not-really large
       programs, as it can only be 65536 * 4 bytes large.  */
    int max_symbol_length;

    /* Here's the symbol we build in lop_stab.  */
    char *lop_stab_symbol;

    /* Index into lop_stab_symbol for the next character when parsing the
       symbol information.  */
    int symbol_position;

    /* When creating arbitrary sections, we need to count section numbers.  */
    int sec_no;

    /* When writing or reading byte-wise, we need to count the bytes
       within a 32-bit word.  */
    int byte_no;

    /* We also need a buffer to hold the bytes we count reading or writing.  */
    bfd_byte buf[4];
  };

typedef struct mmo_data_struct tdata_type;

struct mmo_section_data_struct
  {
    mmo_data_list_type *head;
    mmo_data_list_type *tail;
  };

#define mmo_section_data(sec) \
  ((struct mmo_section_data_struct *) (sec)->used_by_bfd)

/* These structures are used in bfd_map_over_sections constructs.  */

/* Used when writing out sections; all but the register contents section
   which is stored in reg_section.  */
struct mmo_write_sec_info
  {
    asection *reg_section;
    bfd_boolean retval;
  };

/* Used when trying to find a section corresponding to addr.  */
struct mmo_find_sec_info
  {
    asection *sec;
    bfd_vma addr;
  };

static bfd_boolean mmo_bfd_copy_private_bfd_data
  PARAMS ((bfd *, bfd *));
static void mmo_write_section_unless_reg_contents
 PARAMS ((bfd *, asection *, PTR));
static void mmo_find_sec_w_addr
  PARAMS ((bfd *, asection *, PTR));
static void mmo_find_sec_w_addr_grow
  PARAMS ((bfd *, asection *, PTR));
static asection *mmo_make_section
  PARAMS ((bfd *, const char *));
static void mmo_get_symbol_info
  PARAMS ((bfd *, asymbol *, symbol_info *));
static void mmo_print_symbol
  PARAMS ((bfd *, PTR, asymbol *, bfd_print_symbol_type));
static void mmo_init
  PARAMS ((void));
static bfd_boolean mmo_mkobject
  PARAMS ((bfd *));
static bfd_boolean mmo_scan
  PARAMS ((bfd *));
static asection *mmo_decide_section
  PARAMS ((bfd *, bfd_vma));
static asection *mmo_get_generic_spec_data_section
  PARAMS ((bfd *, int));
static asection *mmo_get_spec_section
  PARAMS ((bfd *, int));
static INLINE bfd_byte *mmo_get_loc
  PARAMS ((asection *, bfd_vma, int));
static void mmo_xore_64
  PARAMS ((asection *, bfd_vma vma, bfd_vma value));
static void mmo_xore_32
  PARAMS ((asection *, bfd_vma vma, unsigned int));
static void mmo_xore_16
  PARAMS ((asection *, bfd_vma vma, unsigned int));
static const bfd_target *mmo_object_p
  PARAMS ((bfd *));
static void mmo_map_set_sizes
  PARAMS ((bfd *, asection *, PTR));
static bfd_boolean mmo_get_symbols
  PARAMS ((bfd *));
static bfd_boolean mmo_create_symbol
  PARAMS ((bfd *, const char *, bfd_vma, enum mmo_sym_type, unsigned int));
static bfd_boolean mmo_get_section_contents
  PARAMS ((bfd *, asection *, PTR, file_ptr, bfd_size_type));
static long mmo_get_symtab_upper_bound
  PARAMS ((bfd *));
static long mmo_canonicalize_symtab
  PARAMS ((bfd *, asymbol **));
static void mmo_get_symbol_info
  PARAMS ((bfd *, asymbol *, symbol_info *));
static void mmo_print_symbol
  PARAMS ((bfd *, PTR, asymbol *, bfd_print_symbol_type));
static bfd_boolean mmo_set_section_contents
  PARAMS ((bfd *, sec_ptr, const PTR, file_ptr, bfd_size_type));
static int mmo_sizeof_headers
  PARAMS ((bfd *, bfd_boolean));
static long mmo_get_reloc_upper_bound
  PARAMS ((bfd *, asection *));

static bfd_boolean mmo_internal_write_header
  PARAMS ((bfd *));
static bfd_boolean mmo_internal_write_post
  PARAMS ((bfd *, int, asection *));
static bfd_boolean mmo_internal_add_3_sym
  PARAMS ((bfd *, struct mmo_symbol_trie *, const struct mmo_symbol *));
static unsigned int mmo_internal_3_length
  PARAMS ((bfd *, struct mmo_symbol_trie *));
static void mmo_internal_3_dump
  PARAMS ((bfd *, struct mmo_symbol_trie *));
static void mmo_beb128_out
  PARAMS ((bfd *, int, int));
static bfd_boolean mmo_internal_write_section
  PARAMS ((bfd *, asection *));
static void mmo_write_tetra
  PARAMS ((bfd *, unsigned int));
static void mmo_write_tetra_raw
  PARAMS ((bfd *, unsigned int));
static void mmo_write_octa
  PARAMS ((bfd *, bfd_vma));
static void mmo_write_octa_raw
  PARAMS ((bfd *, bfd_vma));
static bfd_boolean mmo_write_chunk
  PARAMS ((bfd *, const bfd_byte *, unsigned int));
static bfd_boolean mmo_flush_chunk
  PARAMS ((bfd *));
static bfd_boolean mmo_write_loc_chunk
  PARAMS ((bfd *, bfd_vma, const bfd_byte *, unsigned int, bfd_vma *));
static bfd_boolean mmo_write_chunk_list
  PARAMS ((bfd *, mmo_data_list_type *));
static bfd_boolean mmo_write_loc_chunk_list
  PARAMS ((bfd *, mmo_data_list_type *));
static bfd_boolean mmo_write_symbols_and_terminator
  PARAMS ((bfd *));
static flagword mmo_sec_flags_from_bfd_flags
  PARAMS ((flagword));
static flagword bfd_sec_flags_from_mmo_flags
  PARAMS ((flagword));
static bfd_byte mmo_get_byte
  PARAMS ((bfd *));
static void mmo_write_byte
  PARAMS ((bfd *, bfd_byte));
static bfd_boolean mmo_new_section_hook
  PARAMS ((bfd *, asection *));
static int mmo_sort_mmo_symbols
  PARAMS ((const PTR, const PTR));
static bfd_boolean mmo_write_object_contents
  PARAMS ((bfd *));
static long mmo_canonicalize_reloc
  PARAMS ((bfd *, sec_ptr, arelent **, asymbol **));

/* Global "const" variables initialized once.  Must not depend on
   particular input or caller; put such things into the bfd or elsewhere.
   Look ma, no static per-invocation data!  */

static unsigned
char valid_mmo_symbol_character_set[/* A-Z a-z (we assume consecutive
				       codes; sorry EBCDIC:ers!).  */
				    + 'Z' - 'A' + 1 + 'z' - 'a' + 1
				    /* Digits.  */
				    + 10
				    /* ':' and '_'.  */
				    + 1 + 1
				    /* Codes higher than 126.  */
				    + 256 - 126
				    /* Ending zero.  */
				    + 1];


/* Get section SECNAME or create one if it doesn't exist.  When creating
   one, new memory for the name is allocated.  */

static asection *
mmo_make_section (abfd, secname)
     bfd *abfd;
     const char *secname;
{
  asection *sec = bfd_get_section_by_name (abfd, secname);

  if (sec == NULL)
    {
      char *newsecname = strdup (secname);

      if (newsecname == NULL)
	{
	  (*_bfd_error_handler)
	    (_("%s: No core to allocate section name %s\n"),
	     bfd_get_filename (abfd), secname);
	  bfd_set_error (bfd_error_system_call);
	  return NULL;
	}
      sec = bfd_make_section (abfd, newsecname);
    }

  return sec;
}

/* Nothing to do, but keep as a placeholder if we need it.
   Note that state that might differ between bfd:s must not be initialized
   here, nor must it be static.  Add it to tdata information instead.  */

static void
mmo_init ()
{
  static bfd_boolean inited = FALSE;
  int i = 0;
  int j = 0;
  static const char letters[]
    = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789:_";

  if (inited)
    return;
  inited = TRUE;

  /* Fill in the set of valid symbol characters.  */
  strcpy (valid_mmo_symbol_character_set, letters);
  i = strlen (letters);

  for (j = 126; j < 256; j++)
    valid_mmo_symbol_character_set[i++] = j;
}

/* Check whether an existing file is an mmo file.  */

static const bfd_target *
mmo_object_p (abfd)
     bfd *abfd;
{
  struct stat statbuf;
  bfd_byte b[4];

  mmo_init ();

  if (bfd_stat (abfd, &statbuf) < 0
      || bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0
      || bfd_bread (b, 4, abfd) != 4)
    goto bad_final;

  /* All mmo files are a multiple of four bytes long.
     Only recognize version one.  */
  if ((statbuf.st_size % 4) != 0
      || b[0] != LOP || b[1] != LOP_PRE || b[2] != 1)
    goto bad_format;

  /* Get the last 32-bit word.  */
  if (bfd_seek (abfd, (file_ptr) statbuf.st_size - 4, SEEK_SET) != 0
      || bfd_bread (b, 4, abfd) != 4)
    goto bad_final;

  /* Check if the file ends in a lop_end lopcode. */
  if (b[0] != LOP || b[1] != LOP_END || ! mmo_mkobject (abfd))
    goto bad_format;

  /* Compute an upper bound on the max symbol length.  Not really
     important as all of the symbol information can only be 256k.  */
  abfd->tdata.mmo_data->max_symbol_length = (b[2] * 256 + b[3]) * 4;
  abfd->tdata.mmo_data->lop_stab_symbol
    = bfd_malloc (abfd->tdata.mmo_data->max_symbol_length + 1);

  if (abfd->tdata.mmo_data->lop_stab_symbol == NULL)
    {
      (*_bfd_error_handler)
	(_("%s: No core to allocate a symbol %d bytes long\n"),
	 bfd_get_filename (abfd), abfd->tdata.mmo_data->max_symbol_length);
      goto bad_final;
    }

  /* Read in everything.  */
  if (! mmo_scan (abfd))
    goto bad_format_free;

  if (abfd->symcount > 0)
    abfd->flags |= HAS_SYMS;

  /* You'll have to tweak this if you want to use this format for other
     arches (not recommended due to its small-size limitations).  Look at
     the ELF format for how to make it target-generic.  */
  if (! bfd_default_set_arch_mach (abfd, bfd_arch_mmix, 0))
    goto bad_format_free;

  return abfd->xvec;

 bad_format_free:
  free (abfd->tdata.mmo_data->lop_stab_symbol);
 bad_format:
  bfd_set_error (bfd_error_wrong_format);
 bad_final:
  return NULL;
}

/* Set up the mmo tdata information.  */

static bfd_boolean
mmo_mkobject (abfd)
     bfd *abfd;
{
  mmo_init ();

  if (abfd->tdata.mmo_data == NULL)
    {
      time_t created;

      /* All fields are zero-initialized, so we don't have to explicitly
	 initialize most.  */
      tdata_type *tdata = (tdata_type *) bfd_zmalloc (sizeof (tdata_type));
      if (tdata == NULL)
	return FALSE;

      created = time (NULL);
      bfd_put_32 (abfd, created, tdata->created);

      abfd->tdata.mmo_data = tdata;
    }

  return TRUE;
}

static bfd_boolean
mmo_bfd_copy_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  if (bfd_get_flavour (ibfd) != bfd_target_mmo_flavour
      || bfd_get_flavour (obfd) != bfd_target_mmo_flavour)
    return TRUE;

  /* Copy the time the copied-from file was created.  If people want the
     time the file was last *modified*, they have that in the normal file
     information.  */
  memcpy (obfd->tdata.mmo_data->created, ibfd->tdata.mmo_data->created,
	  sizeof (obfd->tdata.mmo_data->created));
  return TRUE;
}

/* Helper functions for mmo_decide_section, used through
   bfd_map_over_sections.  */

static void
mmo_find_sec_w_addr (abfd, sec, p)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     PTR p;
{
  struct mmo_find_sec_info *infop = (struct mmo_find_sec_info *) p;
  bfd_vma vma = bfd_get_section_vma (abfd, sec);

  /* Ignore sections that aren't loaded.  */
  if ((bfd_get_section_flags (abfd, sec) & (SEC_LOAD | SEC_ALLOC))
      !=  (SEC_LOAD | SEC_ALLOC))
    return;

  if (infop->addr >= vma && infop->addr < vma + sec->_raw_size)
    infop->sec = sec;
}

static void
mmo_find_sec_w_addr_grow (abfd, sec, p)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     PTR p;
{
  struct mmo_find_sec_info *infop = (struct mmo_find_sec_info *) p;
  bfd_vma vma = bfd_get_section_vma (abfd, sec);

  /* Ignore sections that aren't loaded.  */
  if ((bfd_get_section_flags (abfd, sec) & (SEC_LOAD | SEC_ALLOC))
      !=  (SEC_LOAD | SEC_ALLOC))
    return;

  if (infop->addr >= vma && infop->addr < vma + MAX_ARTIFICIAL_SECTION_SIZE)
    infop->sec = sec;
}

/* Find a section that corresponds to a VMA.  Automatically create .text
   or .data and set current section to it, depending on what vma.  If we
   can't deduce a section, make one up as ".MMIX.sec.N", where N is an
   increasing number.  */

static asection *
mmo_decide_section (abfd, vma)
     bfd *abfd;
     bfd_vma vma;
{
  asection *sec = NULL;
  char sec_name[sizeof (".MMIX.sec.") + 20];
  struct mmo_find_sec_info info;

  info.addr = vma;
  info.sec = NULL;

  /* First see if there's a section that would match exactly.  */
  bfd_map_over_sections (abfd, mmo_find_sec_w_addr, &info);

  if (info.sec != NULL)
    return info.sec;

  /* If there's no such section, try and expand one of the existing ones,
     up to a limit.  Make sure we have .text and .data before we try that;
     create them corresponding to expected addresses and set flags to make
     them match the "loaded and with contents" expectation.  */
  if ((vma >> 56) == 0)
    {
      sec = bfd_make_section_old_way (abfd, MMO_TEXT_SECTION_NAME);

      if (sec == NULL)
	return NULL;

      if (! sec->user_set_vma)
	bfd_set_section_vma (abfd, sec, vma);
      if (! bfd_set_section_flags (abfd, sec,
				   bfd_get_section_flags (abfd, sec)
				   | SEC_CODE | SEC_LOAD | SEC_ALLOC))
	return NULL;
    }
  else if ((vma >> 56) == 0x20)
    {
      sec = bfd_make_section_old_way (abfd, MMO_DATA_SECTION_NAME);

      if (sec == NULL)
	return NULL;

      if (! sec->user_set_vma)
	bfd_set_section_vma (abfd, sec, vma);
      if (! bfd_set_section_flags (abfd, sec,
				   bfd_get_section_flags (abfd, sec)
				   | SEC_LOAD | SEC_ALLOC))
	return NULL;
    }

  bfd_map_over_sections (abfd, mmo_find_sec_w_addr_grow, &info);

  if (info.sec != NULL)
    return info.sec;

  /* If there's still no suitable section, make a new one.  */
  sprintf (sec_name, ".MMIX.sec.%d", abfd->tdata.mmo_data->sec_no++);
  sec = mmo_make_section (abfd, sec_name);
  if (! sec->user_set_vma)
    bfd_set_section_vma (abfd, sec, vma);

  if (! bfd_set_section_flags (abfd, sec,
			       bfd_get_section_flags (abfd, sec)
			       | SEC_LOAD | SEC_ALLOC))
    return NULL;
  return sec;
}

/* Xor in a 64-bit value VALUE at VMA.  */

static INLINE void
mmo_xore_64 (sec, vma, value)
     asection *sec;
     bfd_vma vma;
     bfd_vma value;
{
  bfd_byte *loc = mmo_get_loc (sec, vma, 8);
  bfd_vma prev = bfd_get_64 (sec->owner, loc);

  value ^= prev;
  bfd_put_64 (sec->owner, value, loc);
}

/* Xor in a 32-bit value VALUE at VMA.  */

static INLINE void
mmo_xore_32 (sec, vma, value)
     asection *sec;
     bfd_vma vma;
     unsigned int value;
{
  bfd_byte *loc = mmo_get_loc (sec, vma, 4);
  unsigned int prev = bfd_get_32 (sec->owner, loc);

  value ^= prev;
  bfd_put_32 (sec->owner, value, loc);
}

/* Xor in a 16-bit value VALUE at VMA.  */

static INLINE void
mmo_xore_16 (sec, vma, value)
     asection *sec;
     bfd_vma vma;
     unsigned int value;
{
  bfd_byte *loc = mmo_get_loc (sec, vma, 2);
  unsigned int prev = bfd_get_16 (sec->owner, loc);

  value ^= prev;
  bfd_put_16 (sec->owner, value, loc);
}

/* Write a 32-bit word to output file, no lop_quote generated.  */

static INLINE void
mmo_write_tetra_raw (abfd, value)
     bfd *abfd;
     unsigned int value;
{
  bfd_byte buf[4];

  bfd_put_32 (abfd, value, buf);

  if (bfd_bwrite ((PTR) buf, 4, abfd) != 4)
    abfd->tdata.mmo_data->have_error = TRUE;
}

/* Write a 32-bit word to output file; lop_quote if necessary.  */

static INLINE void
mmo_write_tetra (abfd, value)
     bfd *abfd;
     unsigned int value;
{
  if (((value >> 24) & 0xff) == LOP)
    mmo_write_tetra_raw (abfd, LOP_QUOTE_NEXT);

  mmo_write_tetra_raw (abfd, value);
}

/* Write a 64-bit word to output file, perhaps with lop_quoting.  */

static INLINE void
mmo_write_octa (abfd, value)
     bfd *abfd;
     bfd_vma value;
{
  mmo_write_tetra (abfd, (unsigned int) (value >> 32));
  mmo_write_tetra (abfd, (unsigned int) value);
}

/* Write a 64-bit word to output file, without lop_quoting.  */

static INLINE void
mmo_write_octa_raw (abfd, value)
     bfd *abfd;
     bfd_vma value;
{
  mmo_write_tetra_raw (abfd, (unsigned int) (value >> 32));
  mmo_write_tetra_raw (abfd, (unsigned int) value);
}

/* Write quoted contents.  Intended to be called multiple times in
   sequence, followed by a call to mmo_flush_chunk.  */

static INLINE bfd_boolean
mmo_write_chunk (abfd, loc, len)
     bfd *abfd;
     const bfd_byte *loc;
     unsigned int len;
{
  bfd_boolean retval = TRUE;

  /* Fill up a tetra from bytes remaining from a previous chunk.  */
  if (abfd->tdata.mmo_data->byte_no != 0)
    {
      while (abfd->tdata.mmo_data->byte_no < 4 && len != 0)
	{
	  abfd->tdata.mmo_data->buf[abfd->tdata.mmo_data->byte_no++] = *loc++;
	  len--;
	}

      if (abfd->tdata.mmo_data->byte_no == 4)
	{
	  mmo_write_tetra (abfd,
			   bfd_get_32 (abfd, abfd->tdata.mmo_data->buf));
	  abfd->tdata.mmo_data->byte_no = 0;
	}
    }

  while (len >= 4)
    {
      if (loc[0] == LOP)
	mmo_write_tetra_raw (abfd, LOP_QUOTE_NEXT);

      retval = (retval
		&& ! abfd->tdata.mmo_data->have_error
		&& 4 == bfd_bwrite ((PTR) loc, 4, abfd));

      loc += 4;
      len -= 4;
    }

  if (len)
    {
      memcpy (abfd->tdata.mmo_data->buf, loc, len);
      abfd->tdata.mmo_data->byte_no = len;
    }

  if (! retval)
    abfd->tdata.mmo_data->have_error = TRUE;
  return retval;
}

/* Flush remaining bytes, from a previous mmo_write_chunk, zero-padded to
   4 bytes.  */

static INLINE bfd_boolean
mmo_flush_chunk (abfd)
     bfd *abfd;
{
  if (abfd->tdata.mmo_data->byte_no != 0)
    {
      memset (abfd->tdata.mmo_data->buf + abfd->tdata.mmo_data->byte_no,
	      0, 4 - abfd->tdata.mmo_data->byte_no);
      mmo_write_tetra (abfd,
		       bfd_get_32 (abfd, abfd->tdata.mmo_data->buf));
      abfd->tdata.mmo_data->byte_no = 0;
    }

  return ! abfd->tdata.mmo_data->have_error;
}

/* Same, but from a list.  */

static INLINE bfd_boolean
mmo_write_chunk_list (abfd, datap)
     bfd *abfd;
     mmo_data_list_type *datap;
{
  for (; datap != NULL; datap = datap->next)
    if (! mmo_write_chunk (abfd, datap->data, datap->size))
      return FALSE;

  return mmo_flush_chunk (abfd);
}

/* Write a lop_loc and some contents.  A caller needs to call
   mmo_flush_chunk after calling this function.  The location is only
   output if different than *LAST_VMAP, which is updated after this call.  */

static bfd_boolean
mmo_write_loc_chunk (abfd, vma, loc, len, last_vmap)
     bfd *abfd;
     bfd_vma vma;
     const bfd_byte *loc;
     unsigned int len;
     bfd_vma *last_vmap;
{
  /* Find an initial and trailing section of zero tetras; we don't need to
     write out zeros.  FIXME: When we do this, we should emit section size
     and address specifiers, else objcopy can't always perform an identity
     translation.  Only do this if we *don't* have left-over data from a
     previous write or the vma of this chunk is *not* the next address,
     because then data isn't tetrabyte-aligned and we're concatenating to
     that left-over data.  */

  if (abfd->tdata.mmo_data->byte_no == 0 || vma != *last_vmap)
    {
      while (len >= 4 && bfd_get_32 (abfd, loc) == 0)
	{
	  vma += 4;
	  len -= 4;
	  loc += 4;
	}

      while (len >= 4 && bfd_get_32 (abfd, loc + len - 4) == 0)
	len -= 4;
    }

  /* Only write out the location if it's different than the one the caller
     (supposedly) previously handled, accounting for omitted leading zeros.  */
  if (vma != *last_vmap)
    {
      /* We might be in the middle of a sequence.  */
      mmo_flush_chunk (abfd);

      /* We always write the location as 64 bits; no use saving bytes
         here.  */
      mmo_write_tetra_raw (abfd, (LOP << 24) | (LOP_LOC << 16) | 2);
      mmo_write_octa_raw (abfd, vma);
    }

  /* Update to reflect end of this chunk, with trailing zeros omitted.  */
  *last_vmap = vma + len;

  return (! abfd->tdata.mmo_data->have_error
	  && mmo_write_chunk (abfd, loc, len));
}

/* Same, but from a list.  */

static INLINE bfd_boolean
mmo_write_loc_chunk_list (abfd, datap)
     bfd *abfd;
     mmo_data_list_type *datap;
{
  /* Get an address different than the address of the first chunk.  */
  bfd_vma last_vma = datap ? datap->where - 1 : 0;

  for (; datap != NULL; datap = datap->next)
    if (! mmo_write_loc_chunk (abfd, datap->where, datap->data, datap->size,
			       &last_vma))
      return FALSE;

  return mmo_flush_chunk (abfd);
}

/* Make a .MMIX.spec_data.N section.  */

static asection *
mmo_get_generic_spec_data_section (abfd, spec_data_number)
     bfd *abfd;
     int spec_data_number;
{
  asection *sec;
  char secname[sizeof (MMIX_OTHER_SPEC_SECTION_PREFIX) + 20]
    = MMIX_OTHER_SPEC_SECTION_PREFIX;

  sprintf (secname + strlen (MMIX_OTHER_SPEC_SECTION_PREFIX),
	   "%d", spec_data_number);

  sec = mmo_make_section (abfd, secname);

  return sec;
}

/* Make a special section for SPEC_DATA_NUMBER.  If it is the one we use
   ourselves, parse some of its data to get at the section name.  */

static asection *
mmo_get_spec_section (abfd, spec_data_number)
     bfd *abfd;
     int spec_data_number;
{
  bfd_byte *secname;
  asection *sec;
  bfd_byte buf[4];
  unsigned int secname_length;
  unsigned int i;
  bfd_vma section_length;
  bfd_vma section_vma;
  mmo_data_list_type *loc;
  flagword flags;
  long orig_pos;

  /* If this isn't the "special" special data, then make a placeholder
     section.  */
  if (spec_data_number != SPEC_DATA_SECTION)
    return mmo_get_generic_spec_data_section (abfd, spec_data_number);

  /* Seek back to this position if there was a format error.  */
  orig_pos = bfd_tell (abfd);

  /* Read the length (in 32-bit words).  */
  if (bfd_bread (buf, 4, abfd) != 4)
    goto format_error;

  if (buf[0] == LOP)
    {
      if (buf[1] != LOP_QUOTE)
	goto format_error;

      if (bfd_bread (buf, 4, abfd) != 4)
	goto format_error;
    }

  /* We don't care to keep the name length accurate.  It's
     zero-terminated.  */
  secname_length = bfd_get_32 (abfd, buf) * 4;

  /* Check section name length for sanity.  */
  if (secname_length > MAX_SECTION_NAME_SIZE)
    goto format_error;

  /* This should be free'd regardless if a section is created.  */
  secname = bfd_malloc (secname_length + 1);
  secname[secname_length] = 0;

  for (i = 0; i < secname_length / 4; i++)
    {
      if (bfd_bread (secname + i * 4, 4, abfd) != 4)
	goto format_error_free;

      if (secname[i * 4] == LOP)
	{
	  /* A bit of overkill, but we handle char 0x98 in a section name,
	     and recognize misparsing.  */
	  if (secname[i * 4 + 1] != LOP_QUOTE
	      || bfd_bread (secname + i * 4, 4, abfd) != 4)
	    /* Whoops.  We thought this was a name, and now we found a
	       non-lop_quote lopcode before we parsed the whole length of
	       the name.  Signal end-of-file in the same manner.  */
	      goto format_error_free;
	}
    }

  /* Get the section flags.  */
  if (bfd_bread (buf, 4, abfd) != 4
      || (buf[0] == LOP
	  && (buf[1] != LOP_QUOTE || bfd_bread (buf, 4, abfd) != 4)))
    goto format_error_free;

  flags = bfd_get_32 (abfd, buf);

  /* Get the section length.  */
  if (bfd_bread (buf, 4, abfd) != 4
      || (buf[0] == LOP
	  && (buf[1] != LOP_QUOTE || bfd_bread (buf, 4, abfd) != 4)))
    goto format_error_free;

  section_length = (bfd_vma) bfd_get_32 (abfd, buf) << 32;

  /* That's the first, high-part.  Now get the low part.  */

  if (bfd_bread (buf, 4, abfd) != 4
      || (buf[0] == LOP
	  && (buf[1] != LOP_QUOTE || bfd_bread (buf, 4, abfd) != 4)))
    goto format_error_free;

  section_length |= (bfd_vma) bfd_get_32 (abfd, buf);

  /* Check the section length for sanity.  */
  if (section_length > MAX_ARTIFICIAL_SECTION_SIZE)
    goto format_error_free;

  /* Get the section VMA.  */
  if (bfd_bread (buf, 4, abfd) != 4
      || (buf[0] == LOP
	  && (buf[1] != LOP_QUOTE || bfd_bread (buf, 4, abfd) != 4)))
    goto format_error_free;

  section_vma = (bfd_vma) bfd_get_32 (abfd, buf) << 32;

  /* That's the first, high-part.  Now get the low part.  */
  if (bfd_bread (buf, 4, abfd) != 4
      || (buf[0] == LOP
	  && (buf[1] != LOP_QUOTE || bfd_bread (buf, 4, abfd) != 4)))
    goto format_error_free;

  section_vma |= (bfd_vma) bfd_get_32 (abfd, buf);

  sec = mmo_make_section (abfd, secname);
  free (secname);
  if (sec == NULL)
    goto format_error;

  /* We allocate a buffer here for the advertised size, with head room for
     tetrabyte alignment.  */
  loc = bfd_zmalloc (section_length + 3
		     + sizeof (struct mmo_data_list_struct));
  if (loc == NULL)
    goto format_error;

  /* Use a TETRA-rounded size for the allocated buffer; we set the
     "visible" section size below.  */
  loc->size = (section_length + 3) & ~3;

  /* Add in the section flags we found to those bfd entered during this
     process and set the contents.  */
  if (! bfd_set_section_flags (abfd, sec,
			       bfd_sec_flags_from_mmo_flags (flags)
			       | bfd_get_section_flags (abfd, sec)
			       | (section_length != 0 ? SEC_HAS_CONTENTS : 0))
      || ! bfd_set_section_size (abfd, sec,
				 sec->_cooked_size + section_length)
      /* Set VMA only for the first occurrence.  */
      || (! sec->user_set_vma
	  && ! bfd_set_section_vma  (abfd, sec, section_vma)))
    {
      /* If we get an error for any of the calls above, signal more than
	 just a format error for the spec section.  */
      return NULL;
    }

  loc->next = NULL;
  if (mmo_section_data (sec)->tail != NULL)
    mmo_section_data (sec)->tail->next = loc;
  else
    mmo_section_data (sec)->head = loc;
  mmo_section_data (sec)->tail = loc;
  loc->where = section_vma;

  return sec;

 format_error_free:
  free (secname);
 format_error:
  if (bfd_seek (abfd, orig_pos, SEEK_SET) != 0)
    return NULL;

  return mmo_get_generic_spec_data_section (abfd, spec_data_number);
}

/* Read a byte, but read from file in multiples of 32-bit words.  */

static bfd_byte
mmo_get_byte (abfd)
     bfd *abfd;
{
  bfd_byte retval;

  if (abfd->tdata.mmo_data->byte_no == 0)
    {
      if (! abfd->tdata.mmo_data->have_error
	  && bfd_bread (abfd->tdata.mmo_data->buf, 4, abfd) != 4)
	{
	  abfd->tdata.mmo_data->have_error = TRUE;

	  /* A value somewhat safe against tripping on some inconsistency
	     when mopping up after this error.  */
	  return 128;
	}
    }

  retval = abfd->tdata.mmo_data->buf[abfd->tdata.mmo_data->byte_no];
  abfd->tdata.mmo_data->byte_no = (abfd->tdata.mmo_data->byte_no + 1) % 4;

  return retval;
}

/* Write a byte, in multiples of 32-bit words.  */

static void
mmo_write_byte (abfd, value)
     bfd *abfd;
     bfd_byte value;
{
  abfd->tdata.mmo_data->buf[(abfd->tdata.mmo_data->byte_no++ % 4)] = value;
  if ((abfd->tdata.mmo_data->byte_no % 4) == 0)
    {
      if (! abfd->tdata.mmo_data->have_error
	  && bfd_bwrite (abfd->tdata.mmo_data->buf, 4, abfd) != 4)
	abfd->tdata.mmo_data->have_error = TRUE;
    }
}

/* Create a symbol.  */

static bfd_boolean
mmo_create_symbol (abfd, symname, addr, sym_type, serno)
     bfd *abfd;
     const char *symname;
     bfd_vma addr;
     enum mmo_sym_type sym_type;
     unsigned int serno;
{
  struct mmo_symbol *n;

  n = (struct mmo_symbol *) bfd_alloc (abfd, sizeof (struct mmo_symbol));
  if (n == NULL)
    return FALSE;

  n->name = bfd_alloc (abfd, strlen (symname) + 1);
  if (n->name == NULL)
    return FALSE;

  strcpy ((PTR) n->name, symname);

  n->value = addr;
  n->sym_type = sym_type;
  n->serno = serno;

  if (abfd->tdata.mmo_data->symbols == NULL)
    abfd->tdata.mmo_data->symbols = n;
  else
    abfd->tdata.mmo_data->symtail->next = n;
  abfd->tdata.mmo_data->symtail = n;
  n->next = NULL;

  ++abfd->symcount;

  /* Check that :Main equals the last octa of the .MMIX.reg_contents
     section, as it's the one place we're sure to pass when reading a mmo
     object.  For written objects, we do it while setting the symbol
     table.  */
  if (strcmp (symname, MMIX_START_SYMBOL_NAME) == 0
      && bfd_get_start_address (abfd) != addr)
    {
      (*_bfd_error_handler)
	(_("%s: invalid mmo file: initialization value for $255 is not `Main'\n"),
	 bfd_get_filename (abfd));
      bfd_set_error (bfd_error_bad_value);
      return FALSE;
    }

  return TRUE;
}

/* Read in symbols.  */

static bfd_boolean
mmo_get_symbols (abfd)
     bfd *abfd;
{
/*
INODE
Symbol-table, mmo section mapping, File layout, mmo
SUBSECTION
	Symbol table format

	From mmixal.w (or really, the generated mmixal.tex) in
	@url{http://www-cs-faculty.stanford.edu/~knuth/programs/mmix.tar.gz}):
	``Symbols are stored and retrieved by means of a @samp{ternary
	search trie}, following ideas of Bentley and Sedgewick. (See
	ACM--SIAM Symp.@: on Discrete Algorithms @samp{8} (1997), 360--369;
	R.@:Sedgewick, @samp{Algorithms in C} (Reading, Mass.@:
	Addison--Wesley, 1998), @samp{15.4}.)  Each trie node stores a
	character, and there are branches to subtries for the cases where
	a given character is less than, equal to, or greater than the
	character in the trie.  There also is a pointer to a symbol table
	entry if a symbol ends at the current node.''

	So it's a tree encoded as a stream of bytes.  The stream of bytes
	acts on a single virtual global symbol, adding and removing
	characters and signalling complete symbol points.  Here, we read
	the stream and create symbols at the completion points.

	First, there's a control byte <<m>>.  If any of the listed bits
	in <<m>> is nonzero, we execute what stands at the right, in
	the listed order:

| (MMO3_LEFT)
| 0x40 - Traverse left trie.
|        (Read a new command byte and recurse.)
|
| (MMO3_SYMBITS)
| 0x2f - Read the next byte as a character and store it in the
|        current character position; increment character position.
|        Test the bits of <<m>>:
|
|        (MMO3_WCHAR)
|        0x80 - The character is 16-bit (so read another byte,
|               merge into current character.
|
|        (MMO3_TYPEBITS)
|        0xf  - We have a complete symbol; parse the type, value
|               and serial number and do what should be done
|               with a symbol.  The type and length information
|               is in j = (m & 0xf).
|
|               (MMO3_REGQUAL_BITS)
|	        j == 0xf: A register variable.  The following
|                         byte tells which register.
|               j <= 8:   An absolute symbol.  Read j bytes as the
|                         big-endian number the symbol equals.
|                         A j = 2 with two zero bytes denotes an
|                         unknown symbol.
|               j > 8:    As with j <= 8, but add (0x20 << 56)
|                         to the value in the following j - 8
|                         bytes.
|
|               Then comes the serial number, as a variant of
|               uleb128, but better named ubeb128:
|               Read bytes and shift the previous value left 7
|               (multiply by 128).  Add in the new byte, repeat
|               until a byte has bit 7 set.  The serial number
|               is the computed value minus 128.
|
|        (MMO3_MIDDLE)
|        0x20 - Traverse middle trie.  (Read a new command byte
|               and recurse.)  Decrement character position.
|
| (MMO3_RIGHT)
| 0x10 - Traverse right trie.  (Read a new command byte and
|        recurse.)

	Let's look again at the <<lop_stab>> for the trivial file
	(@pxref{File layout}).

| 0x980b0000 - lop_stab for ":Main" = 0, serial 1.
| 0x203a4040
| 0x10404020
| 0x4d206120
| 0x69016e00
| 0x81000000

	This forms the trivial trie (note that the path between ``:'' and
	``M'' is redundant):

| 203a	   ":"
| 40       /
| 40      /
| 10      \
| 40      /
| 40     /
| 204d  "M"
| 2061  "a"
| 2069  "i"
| 016e  "n" is the last character in a full symbol, and
|       with a value represented in one byte.
| 00    The value is 0.
| 81    The serial number is 1.  */

  bfd_byte m = mmo_get_byte (abfd);

  /* Check first if we have a bad hair day.  */
  if (abfd->tdata.mmo_data->have_error)
    return FALSE;

  if (m & MMO3_LEFT)
    /* Traverse left trie. */
    mmo_get_symbols (abfd);

  if (m & MMO3_SYMBITS)
    {
      bfd_byte c = mmo_get_byte (abfd);
      bfd_byte j = m & MMO3_TYPEBITS;
      bfd_vma addr = 0;
      enum mmo_sym_type sym_type;
      unsigned int serno = 0;
      bfd_byte k;

      if (m & MMO3_WCHAR)
	{
	  bfd_byte c2 = mmo_get_byte (abfd);

	  /* A two-byte character.  We can't grok this, but neither can
	     mmotype, for other cases than the second byte being zero.  */

	  if (c != 0)
	    {
	      abfd->tdata.mmo_data->lop_stab_symbol
		[abfd->tdata.mmo_data->symbol_position] = 0;

	      (*_bfd_error_handler)
		(_("%s: unsupported wide character sequence\
 0x%02X 0x%02X after symbol name starting with `%s'\n"),
		 bfd_get_filename (abfd), c, c2,
		 abfd->tdata.mmo_data->lop_stab_symbol);
	      bfd_set_error (bfd_error_bad_value);
	      abfd->tdata.mmo_data->have_error = TRUE;
	      return FALSE;
	    }
	  else
	    c = c2;
	}

      abfd->tdata.mmo_data->lop_stab_symbol[abfd->tdata.mmo_data->symbol_position++] = c;
      abfd->tdata.mmo_data->lop_stab_symbol[abfd->tdata.mmo_data->symbol_position] = 0;

      if (j & MMO3_REGQUAL_BITS)
	{
	  if (j == MMO3_REGQUAL_BITS)
	    {
	      sym_type = mmo_reg_sym;
	      addr = mmo_get_byte (abfd);
	    }
	  else if (j <= 8)
	    {
	      unsigned int i;

	      for (i = 0; i < j; i++)
		addr = (addr << 8) + mmo_get_byte (abfd);

	      if (addr == 0 && j == MMO3_UNDEF)
		sym_type = mmo_undef_sym;
	      else
		sym_type = mmo_abs_sym;
	    }
	  else
	    {
	      unsigned int i;

	      for (i = MMO3_DATA; i < j; i++)
		addr = (addr << 8) + mmo_get_byte (abfd);

	      addr += (bfd_vma) 0x20 << 56;
	      sym_type = mmo_data_sym;
	    }

	  /* Get the serial number.  */
	  do
	    {
	      k = mmo_get_byte (abfd);
	      serno = (serno << 7) + k;
	    }
	  while (k < 128);
	  serno -= 128;

	  /* Got it.  Now enter it.  Skip a leading ":".  */
	  if (! abfd->tdata.mmo_data->have_error
	      && ! mmo_create_symbol (abfd,
				      abfd->tdata.mmo_data->lop_stab_symbol
				      + 1,
				      addr, sym_type, serno))
	    abfd->tdata.mmo_data->have_error = TRUE;
	}

      if (m & MMO3_MIDDLE)
	/* Traverse middle trie. */
	mmo_get_symbols (abfd);

      abfd->tdata.mmo_data->symbol_position--;
    }

  if (m & MMO3_RIGHT)
    /* Traverse right trie.  */
    mmo_get_symbols (abfd);

  return ! abfd->tdata.mmo_data->have_error;
}

/* Get the location of memory area [VMA..VMA + SIZE - 1], which we think
   is in section SEC.  Adjust and reallocate zero-initialized contents.
   If there's new contents, allocate to the next multiple of
   MMO_SEC_CONTENTS_CHUNK_SIZE.  */

static INLINE bfd_byte *
mmo_get_loc (sec, vma, size)
     asection *sec;
     bfd_vma vma;
     int size;
{
  bfd_size_type allocated_size;
  struct mmo_section_data_struct *sdatap = mmo_section_data (sec);
  struct mmo_data_list_struct *datap = sdatap->head;
  struct mmo_data_list_struct *entry;

  /* First search the list to see if we have the requested chunk in one
     piece, or perhaps if we have a suitable chunk with room to fit.  */
  for (; datap != NULL; datap = datap->next)
    {
      if (datap->where <= vma
	  && datap->where + datap->size >= vma + size)
	return datap->data + vma - datap->where;
      else if (datap->where <= vma
	       && datap->where + datap->allocated_size >= vma + size
	       /* Only munch on the "allocated size" if it does not
		  overlap the next chunk.  */
	       && (datap->next == NULL || datap->next->where >= vma + size))
	{
	  /* There was room allocated, but the size wasn't set to include
	     it.  Do that now.  */
	  datap->size += (vma + size) - (datap->where + datap->size);

	  /* Update the section size.  This happens only if we update the
	     32-bit-aligned chunk size.  Callers that have
	     non-32-bit-aligned sections should do all allocation and
	     size-setting by themselves or at least set the section size
	     after the last allocating call to this function.  */
	  if (vma + size > sec->vma + sec->_raw_size)
	    sec->_raw_size += (vma + size) - (sec->vma + sec->_raw_size);

	  return datap->data + vma - datap->where;
	}
    }

  /* Not found; allocate a new block.  First check in case we get a
     request for a size split up over several blocks; we'll have to return
     NULL for those cases, requesting the caller to split up the request.
     Requests with an address aligned on MMO_SEC_CONTENTS_CHUNK_SIZE bytes and
     for no more than MMO_SEC_CONTENTS_CHUNK_SIZE will always get resolved.  */

  for (datap = sdatap->head; datap != NULL; datap = datap->next)
    if ((datap->where <= vma && datap->where + datap->size > vma)
	|| (datap->where < vma + size
	    && datap->where + datap->size >= vma + size))
      return NULL;

  allocated_size
    = (size + MMO_SEC_CONTENTS_CHUNK_SIZE - 1) & ~(MMO_SEC_CONTENTS_CHUNK_SIZE - 1);
  entry = (mmo_data_list_type *)
    bfd_zalloc (sec->owner, sizeof (mmo_data_list_type) + allocated_size);
  if (entry == NULL)
    return NULL;
  entry->where = vma;
  entry->size = size;
  entry->allocated_size = allocated_size;

  datap = sdatap->head;

  /* Sort the records by address.  Optimize for the common case of adding
     a record to the end of the list.  */
  if (sdatap->tail != NULL && entry->where >= sdatap->tail->where)
    {
      sdatap->tail->next = entry;
      entry->next = NULL;
      sdatap->tail = entry;
    }
  else
    {
      mmo_data_list_type **look;
      for (look = &sdatap->head;
	   *look != NULL && (*look)->where < entry->where;
	   look = &(*look)->next)
	;
      entry->next = *look;
      *look = entry;
      if (entry->next == NULL)
	{
	  sdatap->tail = entry;

	  /* We get here for the first time (at other times too) for this
	     section.  Say we have contents.  */
	  if (! bfd_set_section_flags (sec->owner, sec,
				       bfd_get_section_flags (sec->owner, sec)
				       | SEC_HAS_CONTENTS))
	    return NULL;
	}
    }

  /* Update the section size.  This happens only when we add contents and
     re-size as we go.  The section size will then be aligned to 32 bits.  */
  if (vma + size > sec->vma + sec->_raw_size)
    sec->_raw_size += (vma + size) - (sec->vma + sec->_raw_size);
  return entry->data;
}

/* Set sizes once we've read in all sections.  */

static void
mmo_map_set_sizes (abfd, sec, ignored)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     PTR ignored ATTRIBUTE_UNUSED;
{
  sec->_cooked_size = sec->_raw_size;
  sec->lma = sec->vma;
}

/* Read the mmo file and turn it into sections.  */

static bfd_boolean
mmo_scan (abfd)
     bfd *abfd;
{
  unsigned int i;
  unsigned int lineno = 1;
  bfd_boolean error = FALSE;
  bfd_vma vma = 0;
  asection *sec = bfd_make_section_old_way (abfd, MMO_TEXT_SECTION_NAME);
  asection *non_spec_sec = NULL;
  bfd_vma non_spec_vma = 0;
  char *current_filename = NULL;
  bfd_size_type nbytes_read = 0;
  /* Buffer with room to read a 64-bit value.  */
  bfd_byte buf[8];
  long stab_loc = -1;
  char *file_names[256];

  memset (file_names, 0, sizeof (file_names));

  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0)
    goto error_return;

  while ((nbytes_read = bfd_bread (buf, 4, abfd)) == 4)
    {
      if (buf[0] == LOP)
	{
	  unsigned int y = bfd_get_8 (abfd, buf + 2);
	  unsigned int z = bfd_get_8 (abfd, buf + 3);

	  /* Change back to the original section for lopcodes other
	     than LOP_QUOTE that comes after a LOP_SPEC.  */
	  if ((buf[1] != LOP_QUOTE || y != 0 || z != 1)
	      && non_spec_sec != NULL)
	    {
	      sec = non_spec_sec;
	      vma = non_spec_vma;
	      non_spec_sec = NULL;
	    }

	  switch (buf[1])
	    {
	    default:
	      (*_bfd_error_handler)
		(_("%s: invalid mmo file: unsupported lopcode `%d'\n"),
		 bfd_get_filename (abfd), buf[1]);
	      bfd_set_error (bfd_error_bad_value);
	      goto error_return;

	    case LOP_QUOTE:
	      /* Quote the next 32-bit word.  */
	      if (y != 0 || z != 1)
		{
		  (*_bfd_error_handler)
		    (_("%s: invalid mmo file: expected YZ = 1 got YZ = %d for lop_quote\n"),
		     bfd_get_filename (abfd), y*256+z);
		  bfd_set_error (bfd_error_bad_value);
		  goto error_return;
		}
	      if (bfd_bread (buf, 4, abfd) != 4)
		goto error_return;

	      mmo_xore_32 (sec, vma, bfd_get_32 (abfd, buf));
	      vma += 4;
	      vma &= ~3;
	      lineno++;
	      break;

	    case LOP_LOC:
	      /* Set vma (and section).  */
	      vma = (bfd_vma) y << 56;
	      if (z == 1)
		{
		  /* Get a 32-bit value.  */
		  if (bfd_bread (buf, 4, abfd) != 4)
		    goto error_return;

		  vma += bfd_get_32 (abfd, buf);
		}
	      else if (z == 2)
		{
		  /* Get a 64-bit value.  */
		  if (bfd_bread (buf, 8, abfd) != 8)
		    goto error_return;

		  vma += bfd_get_64 (abfd, buf);
		}
	      else
		{
		  (*_bfd_error_handler)
		    (_("%s: invalid mmo file: expected z = 1 or z = 2, got z = %d for lop_loc\n"),
		     bfd_get_filename (abfd), z);
		  bfd_set_error (bfd_error_bad_value);
		  goto error_return;
		}

	      sec = mmo_decide_section (abfd, vma);
	      if (sec == NULL)
		goto error_return;
	      break;

	    case LOP_SKIP:
	      /* Move forward within the same section.  */
	      vma += y * 256 + z;

	      sec = mmo_decide_section (abfd, vma);
	      if (sec == NULL)
		goto error_return;
	      break;

	    case LOP_FIXO:
	      /* A fixup: Store the current vma somewhere.  Position using
		 same format as LOP_LOC.  */
	      {
		bfd_vma p = (bfd_vma) y << 56;
		asection *fixosec;

		if (z == 1)
		  {
		    /* Get a 32-bit value.  */
		    if (bfd_bread (buf, 4, abfd) != 4)
		      goto error_return;

		    p += bfd_get_32 (abfd, buf);
		  }
		else if (z == 2)
		  {
		    /* Get a 64-bit value.  */
		    if (bfd_bread (buf, 8, abfd) != 8)
		      goto error_return;

		    p += bfd_get_64 (abfd, buf);
		  }
		else
		  {
		    (*_bfd_error_handler)
		      (_("%s: invalid mmo file: expected z = 1 or z = 2, got z = %d for lop_fixo\n"),
		       bfd_get_filename (abfd), z);
		    bfd_set_error (bfd_error_bad_value);
		    goto error_return;
		  }

		/* The section where we store this address might be a
		   different one than the current section.  */
		fixosec = mmo_decide_section (abfd, p);
		if (fixosec == NULL)
		  goto error_return;
		mmo_xore_64 (fixosec, p, vma);
	      }
	    break;

	    case LOP_FIXR:
	      /* A fixup: Store YZ of this lopcode into YZ at vma - 4 * yz.  */
	      {
		unsigned int yz = (y * 256 + z);
		bfd_vma p = vma + 2 - 4 * yz;
		asection *fixrsec = mmo_decide_section (abfd, p);
		if (fixrsec == NULL)
		  goto error_return;
		mmo_xore_16 (fixrsec, p, yz);
	      }
	    break;

	    case LOP_FIXRX:
	      /* A fixup, similar to lop_fixr, but taking larger numbers
		 and can change branches into the opposite direction
		 (gasp!).  */
	      {
		bfd_vma delta;
		bfd_vma p;
		asection *fixrsec;

		if (y != 0)
		  {
		    (*_bfd_error_handler)
		      (_("%s: invalid mmo file: expected y = 0, got y = %d for lop_fixrx\n"),
		       bfd_get_filename (abfd), y);
		    bfd_set_error (bfd_error_bad_value);
		    goto error_return;
		  }

		if (z != 16 && z != 24)
		  {
		    (*_bfd_error_handler)
		      (_("%s: invalid mmo file: expected z = 16 or z = 24, got z = %d for lop_fixrx\n"),
		       bfd_get_filename (abfd), z);
		    bfd_set_error (bfd_error_bad_value);
		    goto error_return;
		  }

		/* Get the next 32-bit value.  */
		if (bfd_bread (buf, 4, abfd) != 4)
		  goto error_return;

		delta = bfd_get_32 (abfd, buf);

		/* Do an, ehm, involved calculation for the location of
		   the fixup.  See mmixal documentation for a verbose
		   explanation.  We follow it verbosely here for the
		   readers delight.  */
		if (buf[0] == 0)
		  p = vma - 4 * delta;
		else if (buf[0] == 1)
		  p = vma - 4 * ((delta & 0xffffff) - (1 << z));
		else
		  {
		    (*_bfd_error_handler)
		      (_("%s: invalid mmo file: leading byte of operand word must be 0 or 1, got %d for lop_fixrx\n"),
		       bfd_get_filename (abfd), buf[0]);
		    bfd_set_error (bfd_error_bad_value);
		    goto error_return;
		  }

		fixrsec = mmo_decide_section (abfd, vma);
		if (fixrsec == NULL)
		  goto error_return;
		mmo_xore_32 (fixrsec, p, delta);
	      }
	    break;

	    case LOP_FILE:
	      /* Set current file and perhaps the file name.  Reset line
		 number.  */
	      if (z != 0)
		{
		  char *fname = bfd_malloc (z * 4 + 1);

		  if (fname == NULL)
		    {
		      (*_bfd_error_handler)
			(_("%s: cannot allocate file name for file number %d, %d bytes\n"),
			 bfd_get_filename (abfd), y, z * 4 + 1);
		      bfd_set_error (bfd_error_system_call);
		      goto error_return;
		    }

		  fname[z * 4] = 0;

		  for (i = 0; i < z; i++)
		    {
		      if (bfd_bread (fname + i * 4, 4, abfd) != 4)
			{
			  free (fname);
			  goto error_return;
			}
		    }

		  if (file_names[y] != NULL)
		    {
		      (*_bfd_error_handler)
			(_("%s: invalid mmo file: file number %d `%s',\
 was already entered as `%s'\n"),
			 bfd_get_filename (abfd), y, fname, file_names[y]);
		      bfd_set_error (bfd_error_bad_value);
		      goto error_return;
		    }

		  file_names[y] = fname;
		}

	      if (file_names[y] == NULL)
		{
		  (*_bfd_error_handler)
		    (_("%s: invalid mmo file: file name for number %d\
 was not specified before use\n"),
		     bfd_get_filename (abfd), y);
		  bfd_set_error (bfd_error_bad_value);
		  goto error_return;
		}

	      current_filename = file_names[y];
	      lineno = 0;
	      break;

	    case LOP_LINE:
	      /* Set line number.  */
	      lineno = y * 256 + z;
	      /* FIXME: Create a sequence of mmo-specific line number
		 entries for each section, then translate into canonical
		 format.  */
	      break;

	    case LOP_SPEC:
	      /* Special data follows until the next non-lop_quote
		 lopcode.  */
	      non_spec_sec = sec;
	      non_spec_vma = vma;
	      sec = mmo_get_spec_section (abfd, y * 256 + z);
	      if (sec == NULL)
		goto error_return;

	      vma = sec->vma;
	      break;

	    case LOP_PRE:
	      {
		/* We ignore header information, except we read in the
		   creation time from the first 32-bit word with the time
		   in seconds since era.  */
		if (z >= 1
		    && bfd_bread (abfd->tdata.mmo_data->created, 4,
				 abfd) != 4)
		  goto error_return;

		for (i = 1; i < z; i++)
		  if (bfd_bread (buf, 4, abfd) != 4)
		    goto error_return;
	      }
	      break;

	    case LOP_POST:
	      /* This tells of the contents of registers $Z..$255 at
		 startup.  We make a section out of it, with VMA = Z * 8,
		 but only if Z != 255 or the contents is non-zero.  */
	      {
		asection *rsec;
		bfd_byte *loc;
		bfd_vma first_octa;
		bfd_vma startaddr_octa;

		/* Read first octaword outside loop to simplify logic when
		   excluding the Z == 255, octa == 0 case.  */
		if (bfd_bread (buf, 8, abfd) != 8)
		  goto error_return;

		first_octa = bfd_get_64 (abfd, buf);

		/* Don't emit contents for the trivial case which is
		   always present; $255 pointing to Main.  */
		if (z != 255)
		  {
		    rsec
		      = bfd_make_section_old_way (abfd,
						  MMIX_REG_CONTENTS_SECTION_NAME);
		    rsec->vma = z * 8;
		    loc = mmo_get_loc (rsec, z * 8, (255 - z) * 8);
		    bfd_put_64 (abfd, first_octa, loc);

		    for (i = z + 1; i < 255; i++)
		      {
			if (bfd_bread (loc + (i - z) * 8, 8, abfd) != 8)
			  goto error_return;
		      }

		    /* Read out the last octabyte, and use it to set the
		       start address.  */
		    if (bfd_bread (buf, 8, abfd) != 8)
		      goto error_return;

		    startaddr_octa = bfd_get_64 (abfd, buf);
		  }
		else
		  startaddr_octa = first_octa;

		if (! bfd_set_start_address (abfd, startaddr_octa))
		  {
		    /* Currently this can't fail, but this should handle
		       future failures.  */
		    bfd_set_error (bfd_error_bad_value);
		    goto error_return;
		  }
	      }
	      break;

	    case LOP_STAB:
	      /* We read in the symbols now, not later.  */
	      if (y != 0 || z != 0)
		{
		  (*_bfd_error_handler)
		    (_("%s: invalid mmo file: fields y and z of lop_stab\
 non-zero, y: %d, z: %d\n"),
		     bfd_get_filename (abfd), y, z);
		  bfd_set_error (bfd_error_bad_value);
		  goto error_return;
		}

	      /* Save the location, so we can check that YZ in the LOP_END
		 is correct.  */
	      stab_loc = bfd_tell (abfd);

	      /* It's not said that an MMO can be without symbols (though
		 mmixal will refuse to assemble files without Main), but
		 it seems it would still be a valid mmo-file, so allow it.
		 We detect the absence of a symbol area in that the upper
		 limit is computed (from the lop_end YZ field) as 0.
		 Don't call mmo_get_symbols; it can only detect the end of
		 a valid symbol trie, not the absence of one.  */
	      if (abfd->tdata.mmo_data->max_symbol_length != 0
		  && ! mmo_get_symbols (abfd))
		goto error_return;
	      break;

	    case LOP_END:
	      {
		/* This must be the last 32-bit word in an mmo file.
		   Let's find out.  */
		struct stat statbuf;
		long curpos = bfd_tell (abfd);

		if (bfd_stat (abfd, &statbuf) < 0)
		  goto error_return;

		if (statbuf.st_size != curpos)
		  {
		    (*_bfd_error_handler)
		      (_("%s: invalid mmo file: lop_end not last item in\
 file\n"),
		       bfd_get_filename (abfd));
		    bfd_set_error (bfd_error_bad_value);
		    goto error_return;
		  }

		/* Check that the YZ field is right.  Subtract the size of
		   this LOP_END in the calculation; YZ does not include
		   it.  */
		if ((long) (y * 256 + z) * 4 != (curpos - stab_loc) - 4)
		  {
		    (*_bfd_error_handler)
		      (_("%s: invalid mmo file: YZ of lop_end (%ld)\
 not equal to the number of tetras to the preceding lop_stab (%ld)\n"),
		       bfd_get_filename (abfd), (long) (y * 256 + z),
		       (curpos - stab_loc - 4)/4);
		    bfd_set_error (bfd_error_bad_value);
		    goto error_return;
		  }

		bfd_map_over_sections (abfd, mmo_map_set_sizes, NULL);
		goto done;
	      }
	    }
	}
      else
	{
	  /* This wasn't a lopcode, so store it in the current section.  */
	  mmo_xore_32 (sec, vma & ~3, bfd_get_32 (abfd, buf));
	  vma += 4;
	  vma &= ~3;
	  lineno++;
	}
    }

  /* We know this file is a multiple of four bytes (checked in
     mmo_object_p), so if we got something other than 0, this was a bad
     file (although it's more likely we'll get 0 in that case too).
     If we got end-of-file, then there was no lop_stab, so the file has
     invalid format.  */

  if (nbytes_read != 0)
    bfd_set_error (bfd_error_system_call);
  else
    bfd_set_error (bfd_error_bad_value);

 error_return:
  error = TRUE;
 done:
  /* Mark the .text and .data section with their normal attribute if they
     contain anything.  This is not redundant wrt. mmo_decide_section,
     since that code might never execute, and conversely the alloc+code
     section flags must be set then.  */
  sec = bfd_get_section_by_name (abfd, MMO_TEXT_SECTION_NAME);
  if (sec != NULL
      && (bfd_get_section_flags (abfd, sec) & SEC_HAS_CONTENTS)
      && ! bfd_set_section_flags (abfd, sec,
				  bfd_get_section_flags (abfd, sec)
				  | SEC_ALLOC | SEC_LOAD | SEC_CODE))
    error = TRUE;

  sec = bfd_get_section_by_name (abfd, MMO_DATA_SECTION_NAME);
  if (sec != NULL
      && (bfd_get_section_flags (abfd, sec) & SEC_HAS_CONTENTS)
      && ! bfd_set_section_flags (abfd, sec,
				  bfd_get_section_flags (abfd, sec)
				  | SEC_ALLOC | SEC_LOAD))
    error = TRUE;

  /* Free whatever resources we took.  */
  for (i = 0; i < sizeof (file_names) / sizeof (file_names[0]); i++)
    if (file_names[i])
      free (file_names[i]);
  return ! error;
}

/* A hook to set up object file dependent section information.  For mmo,
   we point out the shape of allocated section contents.  */

static bfd_boolean
mmo_new_section_hook (abfd, newsect)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *newsect;
{
  /* We zero-fill all fields and assume NULL is represented by an all
     zero-bit pattern.  */
  newsect->used_by_bfd =
    (PTR) bfd_zalloc (abfd, sizeof (struct mmo_section_data_struct));

  if (!newsect->used_by_bfd)
    return FALSE;

  /* Always align to at least 32-bit words.  */
  newsect->alignment_power = 2;
  return TRUE;
}

/* We already have section contents loaded for sections that have
   contents.  */

static bfd_boolean
mmo_get_section_contents (abfd, sec, location, offset, bytes_to_do)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec ATTRIBUTE_UNUSED;
     PTR location ATTRIBUTE_UNUSED;
     file_ptr offset ATTRIBUTE_UNUSED;
     bfd_size_type bytes_to_do ATTRIBUTE_UNUSED;
{
  /* Iterate over diminishing chunk sizes, copying contents, like
     mmo_set_section_contents.  */
  while (bytes_to_do)
    {
      /* A minor song-and-dance to make sure we're not bitten by the
	 distant possibility of the cast from bfd_vma to int making the
	 chunk zero-sized.  */
      int chunk_size
	= (int) bytes_to_do != 0 ? bytes_to_do : MMO_SEC_CONTENTS_CHUNK_SIZE;
      bfd_byte *loc;

      do
	loc = mmo_get_loc (sec, sec->vma + offset, chunk_size);
      while (loc == NULL && (chunk_size /= 2) != 0);

      if (chunk_size == 0)
	return FALSE;

      memcpy (location, loc, chunk_size);

      location += chunk_size;
      bytes_to_do -= chunk_size;
      offset += chunk_size;
    }
  return TRUE;
}

/* Return the amount of memory needed to read the symbol table.  */

static long
mmo_get_symtab_upper_bound (abfd)
     bfd *abfd ATTRIBUTE_UNUSED;
{
  return (abfd->symcount + 1) * sizeof (asymbol *);
}

/* Sort mmo symbols by serial number.  */

static int
mmo_sort_mmo_symbols (arg1, arg2)
     const PTR arg1;
     const PTR arg2;
{
  const struct mmo_symbol *sym1 = *(const struct mmo_symbol **) arg1;
  const struct mmo_symbol *sym2 = *(const struct mmo_symbol **) arg2;

  /* Sort by serial number first.  */
  if (sym1->serno < sym2->serno)
    return -1;
  else if (sym1->serno > sym2->serno)
    return 1;

  /* Then sort by address of the table entries.  */
  return ((const char *) arg1 - (const char *) arg2);
}

/* Translate the symbol table.  */

static long
mmo_canonicalize_symtab (abfd, alocation)
     bfd *abfd;
     asymbol **alocation;
{
  unsigned int symcount = bfd_get_symcount (abfd);
  asymbol *csymbols;
  unsigned int i;

  csymbols = abfd->tdata.mmo_data->csymbols;
  if (csymbols == NULL)
    {
      asymbol *c;
      struct mmo_symbol *s;
      struct mmo_symbol **msp;

      /* First we store the symbols into the table we'll return, then we
	 qsort it on the serial number, with secondary on the address of
	 the symbol, to preserve order if there would be non-unique serial
	 numbers.  */
      for (s = abfd->tdata.mmo_data->symbols,
	     msp = (struct mmo_symbol **) alocation;
	   s != NULL;
	   s = s->next, ++msp)
	*msp = s;

      *msp = NULL;

      qsort (alocation, symcount, sizeof (struct mmo_symbol *),
	     mmo_sort_mmo_symbols);

      csymbols = (asymbol *) bfd_alloc (abfd, symcount * sizeof (asymbol));
      if (csymbols == NULL && symcount != 0)
	return FALSE;
      abfd->tdata.mmo_data->csymbols = csymbols;

      for (msp = (struct mmo_symbol **) alocation, c = csymbols;
	   *msp != NULL;
	   msp++, ++c)
	{
	  s = *msp;
	  c->the_bfd = abfd;
	  c->name = s->name;
	  c->value = s->value;
	  c->flags = BSF_GLOBAL;

	  if (s->sym_type == mmo_data_sym)
	    {
	      c->section
		= bfd_get_section_by_name (abfd, MMO_DATA_SECTION_NAME);

	      if (c->section == NULL)
		c->section = bfd_abs_section_ptr;
	      else
		c->value -= c->section->vma;
	    }
	  else if (s->sym_type == mmo_undef_sym)
	    c->section = bfd_und_section_ptr;
	  else if (s->sym_type == mmo_reg_sym)
	    {
	      c->section
		= bfd_make_section_old_way (abfd, MMIX_REG_SECTION_NAME);
	    }
	  else
	    {
	      asection *textsec
		= bfd_get_section_by_name (abfd, MMO_TEXT_SECTION_NAME);

	      if (textsec != NULL
		  && c->value >= textsec->vma
		  && c->value <= textsec->vma + textsec->_cooked_size)
		{
		  c->section = textsec;
		  c->value -= c->section->vma;
		}
	      else
		c->section = bfd_abs_section_ptr;
	    }

	  c->udata.p = NULL;
	}
    }

  /* Last, overwrite the incoming table with the right-type entries.  */
  for (i = 0; i < symcount; i++)
    *alocation++ = csymbols++;
  *alocation = NULL;

  return symcount;
}

/* Get information about a symbol.  */

static void
mmo_get_symbol_info (ignore_abfd, symbol, ret)
     bfd *ignore_abfd ATTRIBUTE_UNUSED;
     asymbol *symbol;
     symbol_info *ret;
{
  bfd_symbol_info (symbol, ret);
}

static void
mmo_print_symbol (abfd, afile, symbol, how)
     bfd *abfd;
     PTR afile;
     asymbol *symbol;
     bfd_print_symbol_type how;
{
  FILE *file = (FILE *) afile;

  switch (how)
    {
    case bfd_print_symbol_name:
      fprintf (file, "%s", symbol->name);
      break;
    default:
      bfd_print_symbol_vandf (abfd, (PTR) file, symbol);

      fprintf (file, " %-5s %s",
	       symbol->section->name,
	       symbol->name);
    }
}

/* We can't map a file directly into executable code, so the
   size of header information is irrelevant.  */

static int
mmo_sizeof_headers (abfd, exec)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_boolean exec ATTRIBUTE_UNUSED;
{
  return 0;
}

/* Write the (section-neutral) file preamble.  */

static bfd_boolean
mmo_internal_write_header (abfd)
     bfd *abfd;
{
  const char lop_pre_bfd[] = { LOP, LOP_PRE, 1, 1};

  if (bfd_bwrite (lop_pre_bfd, 4, abfd) != 4)
    return FALSE;

  /* Copy creation time of original file.  */
  if (bfd_bwrite (abfd->tdata.mmo_data->created, 4, abfd) != 4)
    return FALSE;

  return TRUE;
}

/* Write the LOP_POST record, with global register initializations.
   Z is the Z field of the LOP_POST, corresponding to 255 - number of
   registers at DATA.  The Z = 255 field is filled in with the
   start-address.  */

static bfd_boolean
mmo_internal_write_post (abfd, z, sec)
     bfd *abfd;
     int z;
     asection *sec;
{
  int i;
  bfd_byte buf[8];
  mmo_write_tetra_raw (abfd, (LOP << 24) | (LOP_POST << 16) | z);

  for (i = z; i < 255; i++)
    {
      bfd_byte *data = mmo_get_loc (sec, i * 8, 8);

      if (bfd_bwrite (data, 8, abfd) != 8)
	return FALSE;
    }

  /* For Z == $255, we always emit the start location; supposedly Main,
     but we have it handy at bfd_get_start_address.  If we're called with
     Z == 255, don't assume DATA is valid.  */
  bfd_put_64 (abfd, bfd_get_start_address (abfd), buf);

  return ! abfd->tdata.mmo_data->have_error && bfd_bwrite (buf, 8, abfd) == 8;
}

/* Translate to and from BFD flags.  This is to make sure that we don't
   get bitten by BFD flag number changes.  */

static flagword
mmo_sec_flags_from_bfd_flags (flags)
     flagword flags;
{
  flagword oflags = 0;

  if (flags & SEC_ALLOC)
    oflags |= MMO_SEC_ALLOC;
  if (flags & SEC_LOAD)
    oflags |= MMO_SEC_LOAD;
  if (flags & SEC_RELOC)
    oflags |= MMO_SEC_RELOC;
  if (flags & SEC_READONLY)
    oflags |= MMO_SEC_READONLY;
  if (flags & SEC_CODE)
    oflags |= MMO_SEC_CODE;
  if (flags & SEC_DATA)
    oflags |= MMO_SEC_DATA;
  if (flags & SEC_NEVER_LOAD)
    oflags |= MMO_SEC_NEVER_LOAD;
  if (flags & SEC_IS_COMMON)
    oflags |= MMO_SEC_IS_COMMON;
  if (flags & SEC_DEBUGGING)
    oflags |= MMO_SEC_DEBUGGING;

  return oflags;
}

static flagword
bfd_sec_flags_from_mmo_flags (flags)
     flagword flags;
{
  flagword oflags = 0;

  if (flags & MMO_SEC_ALLOC)
    oflags |= SEC_ALLOC;
  if (flags & MMO_SEC_LOAD)
    oflags |= SEC_LOAD;
  if (flags & MMO_SEC_RELOC)
    oflags |= SEC_RELOC;
  if (flags & MMO_SEC_READONLY)
    oflags |= SEC_READONLY;
  if (flags & MMO_SEC_CODE)
    oflags |= SEC_CODE;
  if (flags & MMO_SEC_DATA)
    oflags |= SEC_DATA;
  if (flags & MMO_SEC_NEVER_LOAD)
    oflags |= SEC_NEVER_LOAD;
  if (flags & MMO_SEC_IS_COMMON)
    oflags |= SEC_IS_COMMON;
  if (flags & MMO_SEC_DEBUGGING)
    oflags |= SEC_DEBUGGING;

  return oflags;
}

/* Write a section.  */

static bfd_boolean
mmo_internal_write_section (abfd, sec)
     bfd *abfd;
     asection *sec;
{
  /* We do it differently depending on what section this is:

   ".text": Output, prepended by information about the first source file
   (not yet implemented.)

   ".data": Output.

   (".MMIX.reg_contents": Not handled here.)

   Anything else: Output inside a lop_spec 80, in the format described
   above.  */

  if (strcmp (sec->name, MMO_TEXT_SECTION_NAME) == 0)
    /* FIXME: Output source file name and line number.  */
    return mmo_write_loc_chunk_list (abfd, mmo_section_data (sec)->head);
  else if (strcmp (sec->name, MMO_DATA_SECTION_NAME) == 0)
    return mmo_write_loc_chunk_list (abfd, mmo_section_data (sec)->head);
  else if (strcmp (sec->name, MMIX_REG_CONTENTS_SECTION_NAME) == 0)
    /* Not handled here.  */
    {
      /* This would normally be an abort call since this can't happen, but
         we don't do that.  */
      bfd_set_error (bfd_error_bad_value);
      return FALSE;
    }
  else if (strncmp (sec->name, MMIX_OTHER_SPEC_SECTION_PREFIX,
		    strlen (MMIX_OTHER_SPEC_SECTION_PREFIX)) == 0)
    {
      int n = atoi (sec->name + strlen (MMIX_OTHER_SPEC_SECTION_PREFIX));
      mmo_write_tetra_raw (abfd, (LOP << 24) | (LOP_SPEC << 16) | n);
      return (! abfd->tdata.mmo_data->have_error
	      && mmo_write_chunk_list (abfd, mmo_section_data (sec)->head));
    }
  /* Ignore sections that are just allocated or empty; we write out
     _contents_ here.  */
  else if ((bfd_get_section_flags (abfd, sec) & SEC_HAS_CONTENTS) != 0
	   && sec->_raw_size != 0)
    {
      /* Keep the document-comment formatted the way it is.  */
/*
INODE
mmo section mapping, , Symbol-table, mmo
SUBSECTION
	mmo section mapping

	The implementation in BFD uses special data type 80 (decimal) to
	encapsulate and describe named sections, containing e.g.@: debug
	information.  If needed, any datum in the encapsulation will be
	quoted using lop_quote.  First comes a 32-bit word holding the
	number of 32-bit words containing the zero-terminated zero-padded
	segment name.  After the name there's a 32-bit word holding flags
	describing the section type.  Then comes a 64-bit big-endian word
	with the section length (in bytes), then another with the section
	start address.  Depending on the type of section, the contents
	might follow, zero-padded to 32-bit boundary.  For a loadable
	section (such as data or code), the contents might follow at some
	later point, not necessarily immediately, as a lop_loc with the
	same start address as in the section description, followed by the
	contents.  This in effect forms a descriptor that must be emitted
	before the actual contents.  Sections described this way must not
	overlap.

	For areas that don't have such descriptors, synthetic sections are
	formed by BFD.  Consecutive contents in the two memory areas
	@samp{0x0000@dots{}00} to @samp{0x01ff@dots{}ff} and
	@samp{0x2000@dots{}00} to @samp{0x20ff@dots{}ff} are entered in
	sections named <<.text>> and <<.data>> respectively.  If an area
	is not otherwise described, but would together with a neighboring
	lower area be less than @samp{0x40000000} bytes long, it is joined
	with the lower area and the gap is zero-filled.  For other cases,
	a new section is formed, named <<.MMIX.sec.@var{n}>>.  Here,
	@var{n} is a number, a running count through the mmo file,
	starting at 0.

EXAMPLE
	A loadable section specified as:

| .section secname,"ax"
| TETRA 1,2,3,4,-1,-2009
| BYTE 80

	and linked to address @samp{0x4}, is represented by the sequence:

| 0x98080050 - lop_spec 80
| 0x00000002 - two 32-bit words for the section name
| 0x7365636e - "secn"
| 0x616d6500 - "ame\0"
| 0x00000033 - flags CODE, READONLY, LOAD, ALLOC
| 0x00000000 - high 32 bits of section length
| 0x0000001c - section length is 28 bytes; 6 * 4 + 1 + alignment to 32 bits
| 0x00000000 - high 32 bits of section address
| 0x00000004 - section address is 4
| 0x98010002 - 64 bits with address of following data
| 0x00000000 - high 32 bits of address
| 0x00000004 - low 32 bits: data starts at address 4
| 0x00000001 - 1
| 0x00000002 - 2
| 0x00000003 - 3
| 0x00000004 - 4
| 0xffffffff - -1
| 0xfffff827 - -2009
| 0x50000000 - 80 as a byte, padded with zeros.

	Note that the lop_spec wrapping does not include the section
	contents.  Compare this to a non-loaded section specified as:

| .section thirdsec
| TETRA 200001,100002
| BYTE 38,40

	This, when linked to address @samp{0x200000000000001c}, is
	represented by:

| 0x98080050 - lop_spec 80
| 0x00000002 - two 32-bit words for the section name
| 0x7365636e - "thir"
| 0x616d6500 - "dsec"
| 0x00000010 - flag READONLY
| 0x00000000 - high 32 bits of section length
| 0x0000000c - section length is 12 bytes; 2 * 4 + 2 + alignment to 32 bits
| 0x20000000 - high 32 bits of address
| 0x0000001c - low 32 bits of address 0x200000000000001c
| 0x00030d41 - 200001
| 0x000186a2 - 100002
| 0x26280000 - 38, 40 as bytes, padded with zeros

	For the latter example, the section contents must not be
	loaded in memory, and is therefore specified as part of the
	special data.  The address is usually unimportant but might
	provide information for e.g.@: the DWARF 2 debugging format.  */

      mmo_write_tetra_raw (abfd, LOP_SPEC_SECTION);
      mmo_write_tetra (abfd, (strlen (sec->name) + 3) / 4);
      mmo_write_chunk (abfd, sec->name, strlen (sec->name));
      mmo_flush_chunk (abfd);
      /* FIXME: We can get debug sections (.debug_line & Co.) with a
	 section flag still having SEC_RELOC set.  Investigate.  This
	 might be true for all alien sections; perhaps mmo.em should clear
	 that flag.  Might be related to weak references.  */
      mmo_write_tetra (abfd,
		       mmo_sec_flags_from_bfd_flags
		       (bfd_get_section_flags (abfd, sec)));
      mmo_write_octa (abfd, sec->_raw_size);
      mmo_write_octa (abfd, bfd_get_section_vma (abfd, sec));

      /* Writing a LOP_LOC ends the LOP_SPEC data, and makes data actually
	 loaded.  */
      if (bfd_get_section_flags (abfd, sec) & SEC_LOAD)
	return (! abfd->tdata.mmo_data->have_error
		&& mmo_write_loc_chunk_list (abfd,
					     mmo_section_data (sec)->head));
      return (! abfd->tdata.mmo_data->have_error
	      && mmo_write_chunk_list (abfd, mmo_section_data (sec)->head));
    }
  return TRUE;
}

/* We save up all data before output.  */

static bfd_boolean
mmo_set_section_contents (abfd, sec, location, offset, bytes_to_do)
     bfd *abfd ATTRIBUTE_UNUSED;
     sec_ptr sec;
     const PTR location;
     file_ptr offset;
     bfd_size_type bytes_to_do;
{
  /* Iterate over diminishing chunk sizes, copying contents.  */
  while (bytes_to_do)
    {
      /* A minor song-and-dance to make sure we're not bitten by the
	 distant possibility of the cast from bfd_vma to int making the
	 chunk zero-sized.  */
      int chunk_size
	= (int) bytes_to_do != 0 ? bytes_to_do : MMO_SEC_CONTENTS_CHUNK_SIZE;
      bfd_byte *loc;

      do
	loc = mmo_get_loc (sec, sec->vma + offset, chunk_size);
      while (loc == NULL && (chunk_size /= 2) != 0);

      if (chunk_size == 0)
	return FALSE;

      memcpy (loc, location, chunk_size);

      location += chunk_size;
      bytes_to_do -= chunk_size;
      offset += chunk_size;
    }
  return TRUE;
}

/* Add a symbol to a trie-tree.  */

static bfd_boolean
mmo_internal_add_3_sym (abfd, rootp, symp)
     bfd *abfd;
     struct mmo_symbol_trie *rootp;
     const struct mmo_symbol *symp;
{
  const char *name = symp->name;
  struct mmo_symbol_trie *trie = rootp;
  struct mmo_symbol_trie **triep = NULL;

  while (*name && trie != NULL)
    {
      if (*name < trie->symchar)
	{
	  triep = &trie->left;
	  trie = trie->left;
	}
      else if (*name > trie->symchar)
	{
	  triep = &trie->right;
	  trie = trie->right;
	}
      else if (*name == trie->symchar)
	{
	  triep = &trie->middle;
	  name++;

	  /* Make sure "trie" points to where we should fill in the
	     current symbol whenever we've iterated through "name".  We
	     would lose the right position if we encounter "foobar" then
	     "foo".  */
	  if (*name)
	    trie = trie->middle;
	}
    }

  while (*name != 0)
    {
      /* Create middle branches for the rest of the characters.  */
      trie = bfd_zalloc (abfd, sizeof (struct mmo_symbol_trie));
      *triep = trie;
      trie->symchar = *name++;
      triep = &trie->middle;
    }

  /* We discover a duplicate symbol rather late in the process, but still;
     we discover it and bail out.  */
  if (trie->sym.name != NULL)
    {
      (*_bfd_error_handler)
	(_("%s: invalid symbol table: duplicate symbol `%s'\n"),
	 bfd_get_filename (abfd), trie->sym.name);
      bfd_set_error (bfd_error_bad_value);
      return FALSE;
    }

  memcpy (&trie->sym, symp, sizeof *symp);
  return TRUE;
}

/* Find out the length of the serialized version of a trie in bytes.  */

static unsigned int
mmo_internal_3_length (abfd, trie)
     bfd *abfd;
     struct mmo_symbol_trie *trie;
{
  /* First, one for the control byte.  */
  unsigned int length = 1;

  if (trie == NULL)
    return 0;

  /* Add in the recursion to the left.  */
  length += mmo_internal_3_length (abfd, trie->left);

  /* Add in the middle trie and the character.  */
  length += 1 + mmo_internal_3_length (abfd, trie->middle);

  /* Add in the recursion to the right.  */
  length += mmo_internal_3_length (abfd, trie->right);

  /* Add in bytes for the symbol (if this is an endnode). */
  if (trie->sym.name != NULL)
    {
      unsigned int serno = trie->sym.serno;

      /* First what it takes to encode the value. */
      if (trie->sym.sym_type == mmo_reg_sym)
	length++;
      else if (trie->sym.sym_type == mmo_undef_sym)
	length += 2;
      else
	{
	  bfd_vma value = trie->sym.value;

	  /* Coded in one to eight following bytes.  */
	  if (trie->sym.sym_type == mmo_data_sym)
	    value -= (bfd_vma) 0x20 << 56;

	  do
	    {
	      value >>= 8;
	      length++;
	    }
	  while (value != 0);
	}

      /* Find out what it takes to encode the serial number.  */
      do
	{
	  serno >>= 7;
	  length++;
	}
      while (serno != 0);
    }

  return length;
}

/* Helper function for outputting the serial number of a symbol, output as
   a variant of leb128 (see dwarf2 documentation) which could be called
   beb128.  Using a helper function and recursion simplifies debugging.  */

static void
mmo_beb128_out (abfd, serno, marker)
     bfd *abfd;
     int serno;
     int marker;
{
  if (serno & ~0x7f)
    mmo_beb128_out (abfd, serno >> 7, 0);
  mmo_write_byte (abfd, marker | (serno & 0x7f));
}

/* Serialize a trie.  */

static void
mmo_internal_3_dump (abfd, trie)
     bfd *abfd;
     struct mmo_symbol_trie *trie;
{
  bfd_byte control = 0;

  if (trie == NULL)
    return;

  if (trie->left)
    control |= MMO3_LEFT;

  if (trie->middle)
    control |= MMO3_MIDDLE;

  if (trie->right)
    control |= MMO3_RIGHT;

  if (trie->sym.name != NULL)
    {
      /* Encode the symbol type and length of value bytes.  */
      if (trie->sym.sym_type == mmo_reg_sym)
	control |= MMO3_REGQUAL_BITS;
      else if (trie->sym.sym_type == mmo_undef_sym)
	control |= MMO3_UNDEF;
      else
	{
	  bfd_vma value = trie->sym.value;

	  /* Coded in 1..8 following bytes.  */
	  if (trie->sym.sym_type == mmo_data_sym)
	    {
	      control |= MMO3_DATA;
	      value -= (bfd_vma) 0x20 << 56;
	    }

	  do
	    {
	      value >>= 8;
	      control++;
	    }
	  while (value != 0);
	}
    }

  /* The control byte is output before recursing.  */
  mmo_write_byte (abfd, control);

  mmo_internal_3_dump (abfd, trie->left);

  if (control & MMO3_SYMBITS)
    {
      mmo_write_byte (abfd, trie->symchar);

      if (trie->sym.name != NULL)
	{
	  if (trie->sym.sym_type == mmo_reg_sym)
	    mmo_write_byte (abfd, trie->sym.value);
	  else if (trie->sym.sym_type == mmo_undef_sym)
	    {
	      mmo_write_byte (abfd, 0);
	      mmo_write_byte (abfd, 0);
	    }
	  else
	    {
	      bfd_vma value = trie->sym.value;

	      bfd_byte byte_n = control & 15;

	      /* Coded in 1..8 following bytes.  Note that the value is
		 shifted out big-endian.  */
	      if (trie->sym.sym_type == mmo_data_sym)
		{
		  value -= (bfd_vma) 0x20 << 56;
		  byte_n -= 8;
		}

	      do
		{
		  mmo_write_byte (abfd, (value >> ((byte_n - 1) * 8)) & 0xff);
		  byte_n--;
		}
	      while (byte_n != 0);
	    }

	  mmo_beb128_out (abfd, trie->sym.serno, 128);
	}
      mmo_internal_3_dump (abfd, trie->middle);
    }
  mmo_internal_3_dump (abfd, trie->right);
}

/* Write symbols in mmo format.  Also write the lop_end terminator.  */

static bfd_boolean
mmo_write_symbols_and_terminator (abfd)
     bfd *abfd;
{
  int count = bfd_get_symcount (abfd);
  asymbol *maintable[2];
  asymbol **table;
  asymbol **orig_table = bfd_get_outsymbols (abfd);
  int serno;
  struct mmo_symbol_trie root;
  int trie_len;
  int i;
  bfd_byte buf[4];

  /* Create a symbol for "Main".  */
  asymbol *fakemain = bfd_make_empty_symbol (abfd);

  fakemain->flags = BSF_GLOBAL;
  fakemain->value = bfd_get_start_address (abfd);
  fakemain->name = MMIX_START_SYMBOL_NAME;
  fakemain->section = bfd_abs_section_ptr;
  maintable[0] = fakemain;
  maintable[1] = NULL;

  memset (&root, 0, sizeof (root));

  /* Make all symbols take a left turn.  */
  root.symchar = 0xff;

  /* There must always be a ":Main", so we'll add one if there are no
     symbols.  Make sure we have room for it.  */
  table = bfd_alloc (abfd, (count + 1) * sizeof (asymbol *));
  if (table == NULL)
    return FALSE;

  memcpy (table, orig_table, count * sizeof (asymbol *));

  /* Move :Main (if there is one) to the first position.  This is
     necessary to get the same layout of the trie-tree when linking as
     when objcopying the result as in the objcopy.exp test "simple objcopy
     of executable".  It also automatically takes care of assigning serial
     number 1 to :Main (as is mandatory).  */
  for (i = 0; i < count; i++)
    if (table[i] != NULL
	&& strcmp (table[i]->name, MMIX_START_SYMBOL_NAME) == 0
	&& (table[i]->flags & (BSF_DEBUGGING|BSF_GLOBAL)) == BSF_GLOBAL)
      {
	asymbol *mainsym = table[i];
	memcpy (table + 1, orig_table, i * sizeof (asymbol *));
	table[0] = mainsym;

	/* Check that the value assigned to :Main is the same as the entry
	   address.  The default linker script asserts this.  This is as
	   good a place as any to check this consistency. */
	if ((mainsym->value
	     + mainsym->section->output_section->vma
	     + mainsym->section->output_offset)
	    != bfd_get_start_address (abfd))
	  {
	    /* Arbitrary buffer to hold the printable representation of a
	       vma.  */
	    char vmas_main[40];
	    char vmas_start[40];
	    bfd_vma vma_start = bfd_get_start_address (abfd);

	    sprintf_vma (vmas_main, mainsym->value);
	    sprintf_vma (vmas_start, vma_start);

	    (*_bfd_error_handler)
	      (_("%s: Bad symbol definition: `Main' set to %s rather\
 than the start address %s\n"),
	       bfd_get_filename (abfd), vmas_main, vmas_start);
	    bfd_set_error (bfd_error_bad_value);
	    return FALSE;
	  }
	break;
      }
  if (i == count && count != 0)
    {
      /* When there are symbols, there must be a :Main.  There was no
	 :Main, so we need to add it manually.  */
      memcpy (table + 1, orig_table, count * sizeof (asymbol *));
      table[0] = fakemain;
      count++;
    }

  for (i = 0, serno = 1; i < count && table[i] != NULL; i++)
    {
      asymbol *s = table[i];

      /* It's not enough to consult bfd_is_local_label, since it does not
	 mean "local" in the sense of linkable-and-observable-after-link.
	 Let's just check the BSF_GLOBAL flag.

	 Also, don't export symbols with characters not in the allowed set.  */
      if ((s->flags & (BSF_DEBUGGING|BSF_GLOBAL)) == BSF_GLOBAL
	  && strspn (s->name,
		     valid_mmo_symbol_character_set) == strlen (s->name))
	{
	  struct mmo_symbol sym;
	  memset (&sym, 0, sizeof (sym));

	  sym.name = s->name;
	  sym.value =
	    s->value
	    + s->section->output_section->vma
	    + s->section->output_offset;

	  if (bfd_is_und_section (s->section))
	    sym.sym_type = mmo_undef_sym;
	  else if (strcmp (s->section->name, MMO_DATA_SECTION_NAME) == 0
		   /* The encoding of data symbols require that the "rest"
		      of the value fits in 6 bytes, so the upper two bytes
		      must be 0x2000.  All other symbols get to be the
		      absolute type.  */
		   && (sym.value >> 48) == 0x2000)
	    sym.sym_type = mmo_data_sym;
	  else if (strcmp (s->section->name, MMIX_REG_SECTION_NAME) == 0)
	    sym.sym_type = mmo_reg_sym;
	  else if (strcmp (s->section->name,
			   MMIX_REG_CONTENTS_SECTION_NAME) == 0)
	    {
	      sym.sym_type = mmo_reg_sym;
	      sym.value /= 8;
	    }
	  else
	    sym.sym_type = mmo_abs_sym;

	  /* FIXME: We assume the order of the received symbols is an
	     ordered mapping of the serial numbers.  This is not
	     necessarily true if we e.g. objcopy a mmo file to another and
	     there are gaps in the numbering.  Not sure if this can
	     happen.  Not sure what to do.  */
	  sym.serno = serno++;

	  if (! mmo_internal_add_3_sym (abfd, &root, &sym))
	    return FALSE;
	}
    }

  /* Change the root node to be a ":"-prefix.  */
  root.symchar = ':';
  root.middle = root.left;
  root.right = NULL;
  root.left = NULL;

  /* We have to find out if we can fit the whole symbol table in the mmo
     symtab.  It would be bad to assume we can always fit it in 262144
     bytes.  If we can't, just leave the Main symbol.  */
  trie_len = (mmo_internal_3_length (abfd, &root) + 3)/4;

  if (trie_len > 0xffff)
    {
      /* Test this code by using a lower limit in the test above and check
	 that the single "Main" symbol is emitted and handled properly.
	 There's no specific test-case.  */
      struct mmo_symbol sym;

      (*_bfd_error_handler)
	(_("%s: warning: symbol table too large for mmo, larger than 65535\
 32-bit words: %d.  Only `Main' will be emitted.\n"),
	 bfd_get_filename (abfd), trie_len);

      memset (&sym, 0, sizeof (sym));
      sym.sym_type = mmo_abs_sym;
      sym.name = MMIX_START_SYMBOL_NAME;
      sym.serno = 1;
      sym.value = bfd_get_start_address (abfd);

      /* Then patch up a symbol table to be just the ":Main" symbol.  */
      memset (&root, 0, sizeof (root));
      root.left = root.middle;
      root.symchar = 0xff;
      root.middle = NULL;
      root.right = NULL;

      if (! mmo_internal_add_3_sym (abfd, &root, &sym))
	return FALSE;

      root.symchar = ':';
      root.middle = root.left;
      root.right = NULL;
      root.left = NULL;

      trie_len = (mmo_internal_3_length (abfd, &root) + 3)/4;
    }

  /* Reset the written-bytes counter.  */
  abfd->tdata.mmo_data->byte_no = 0;

  /* Put out the lop_stab mark.  */
  bfd_put_32 (abfd, (LOP << 24) | (LOP_STAB << 16), buf);
  if (bfd_bwrite (buf, 4, abfd) != 4)
    return FALSE;

  /* Dump out symbols.  */
  mmo_internal_3_dump (abfd, &root);

  if (trie_len != (abfd->tdata.mmo_data->byte_no + 3)/4)
    {
      /* I haven't seen this trig.  It seems no use claiming this case
	 isn't debugged and abort if we get here.  Instead emit a
	 diagnostic and fail "normally".  */
      (*_bfd_error_handler)
	(_("%s: internal error, symbol table changed size from %d to %d\
 words\n"),
	 bfd_get_filename (abfd), trie_len,
	 (abfd->tdata.mmo_data->byte_no + 3)/4);
      bfd_set_error (bfd_error_bad_value);
      return FALSE;
    }

  /* Dump out remaining bytes in the buffer and handle I/O errors by
     propagating errors.  */
  if ((abfd->tdata.mmo_data->byte_no % 4) != 0
      || abfd->tdata.mmo_data->have_error)
    {
      memset (abfd->tdata.mmo_data->buf + (abfd->tdata.mmo_data->byte_no % 4),
	      0, 4 - (abfd->tdata.mmo_data->byte_no % 4));

      if (abfd->tdata.mmo_data->have_error
	  || bfd_bwrite (abfd->tdata.mmo_data->buf, 4, abfd) != 4)
	return FALSE;
    }

  bfd_put_32 (abfd, (LOP << 24) | (LOP_END << 16) | trie_len, buf);
  return bfd_bwrite (buf, 4, abfd) == 4;
}

/* Write section unless it is the register contents section.  For that, we
   instead store the section in the supplied pointer.  This function is
   used through bfd_map_over_sections.  */

static void
mmo_write_section_unless_reg_contents (abfd, sec, p)
     bfd *abfd;
     asection *sec;
     PTR p;
{
  struct mmo_write_sec_info *infop = (struct mmo_write_sec_info *) p;

  if (! infop->retval)
    return;

  if (strcmp (sec->name, MMIX_REG_CONTENTS_SECTION_NAME) == 0)
    {
      infop->reg_section = sec;
      return;
    }

  /* Exclude the convenience register section.  */
  if (strcmp (sec->name, MMIX_REG_SECTION_NAME) == 0)
    {
      if (bfd_get_section_flags (abfd, sec) & SEC_HAS_CONTENTS)
	{
	  /* Make sure it hasn't got contents.  It seems impossible to
	     make it carry contents, so we don't have a test-case for
	     this.  */
	  (*_bfd_error_handler)
	    (_("%s: internal error, internal register section %s had\
 contents\n"),
	     bfd_get_filename (abfd), sec->name);
	  bfd_set_error (bfd_error_bad_value);
	  infop->retval = FALSE;
	  return;
	}

      return;
    }

  infop->retval = mmo_internal_write_section (abfd, sec);
}

/* Do the actual output of a file.  Assumes mmo_set_section_contents is
   already called. */

static bfd_boolean
mmo_write_object_contents (abfd)
     bfd *abfd;
{
  struct mmo_write_sec_info wsecinfo;

  /* First, there are a few words of preamble.  */
  if (! mmo_internal_write_header (abfd))
    return FALSE;

  wsecinfo.reg_section = NULL;
  wsecinfo.retval = TRUE;

  bfd_map_over_sections (abfd, mmo_write_section_unless_reg_contents,
			 (PTR) &wsecinfo);

  if (! wsecinfo.retval)
    return FALSE;

  if (wsecinfo.reg_section != NULL)
    {
      asection *sec = wsecinfo.reg_section;
      unsigned int z = (unsigned int) (sec->vma / 8);

      /* Registers 0..31 must not be global.  Do sanity check on the "vma"
	 of the register contents section and check that it corresponds to
	 the length of the section.  */
      if (z < 32 || z >= 255 || (sec->vma & 7) != 0
	  || sec->vma != 256 * 8 - sec->_raw_size - 8)
	{
	  bfd_set_error (bfd_error_bad_value);

	  if (sec->_raw_size == 0)
	    /* There must always be at least one such register.  */
	    (*_bfd_error_handler)
	      (_("%s: no initialized registers; section length 0\n"),
	       bfd_get_filename (abfd));
	  else if (sec->vma > (256 - 32) * 8)
	    /* Provide better error message for the case of too many
	       global registers.  */
	    (*_bfd_error_handler)
	      (_("%s: too many initialized registers; section length %ld\n"),
	       bfd_get_filename (abfd),
	       (long) sec->_raw_size);
	  else
	    (*_bfd_error_handler)
	      (_("%s: invalid start address for initialized registers of\
 length %ld: 0x%lx%08lx\n"),
	       bfd_get_filename (abfd),
	       (long) sec->_raw_size,
	       (unsigned long) (sec->vma >> 32), (unsigned long) (sec->vma));

	  return FALSE;
	}

      if (! mmo_internal_write_post (abfd, z, sec))
	return FALSE;
    }
  else
    if (! mmo_internal_write_post (abfd, 255, NULL))
      return FALSE;

  return mmo_write_symbols_and_terminator (abfd);
}

/* Return the size of a NULL pointer, so we support linking in an mmo
   object.  */

static long
mmo_get_reloc_upper_bound (abfd, sec)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec ATTRIBUTE_UNUSED;
{
  return sizeof (PTR);
}

/* Similarly canonicalize relocs to empty, filling in the terminating NULL
   pointer.  */

long
mmo_canonicalize_reloc (abfd, section, relptr, symbols)
     bfd *abfd ATTRIBUTE_UNUSED;
     sec_ptr section ATTRIBUTE_UNUSED;
     arelent **relptr;
     asymbol **symbols ATTRIBUTE_UNUSED;
{
  *relptr = NULL;
  return 0;
}

/* If there's anything in particular in a mmo bfd that we want to free,
   make this a real function.  Only do this if you see major memory
   thrashing; zealous free:ing will cause unwanted behavior, especially if
   you "free" memory allocated with "bfd_alloc", or even "bfd_release" a
   block allocated with "bfd_alloc"; they're really allocated from an
   obstack, and we don't know what was allocated there since this
   particular allocation.  */

#define	mmo_close_and_cleanup _bfd_generic_close_and_cleanup
#define mmo_bfd_free_cached_info _bfd_generic_bfd_free_cached_info

/* Perhaps we need to adjust this one; mmo labels (originally) without a
   leading ':' might more appropriately be called local.  */
#define mmo_bfd_is_local_label_name bfd_generic_is_local_label_name

/* Is this one really used or defined by anyone?  */
#define mmo_get_lineno _bfd_nosymbols_get_lineno

/* FIXME: We can do better on this one, if we have a dwarf2 .debug_line
   section or if MMO line numbers are implemented.  */
#define mmo_find_nearest_line _bfd_nosymbols_find_nearest_line
#define mmo_make_empty_symbol _bfd_generic_make_empty_symbol
#define mmo_bfd_make_debug_symbol _bfd_nosymbols_bfd_make_debug_symbol
#define mmo_read_minisymbols _bfd_generic_read_minisymbols
#define mmo_minisymbol_to_symbol _bfd_generic_minisymbol_to_symbol

#define mmo_get_section_contents_in_window \
  _bfd_generic_get_section_contents_in_window
#define mmo_bfd_get_relocated_section_contents \
  bfd_generic_get_relocated_section_contents
#define mmo_bfd_gc_sections bfd_generic_gc_sections
#define mmo_bfd_link_hash_table_create _bfd_generic_link_hash_table_create
#define mmo_bfd_link_hash_table_free _bfd_generic_link_hash_table_free
#define mmo_bfd_link_add_symbols _bfd_generic_link_add_symbols
#define mmo_bfd_link_just_syms _bfd_generic_link_just_syms
#define mmo_bfd_final_link _bfd_generic_final_link
#define mmo_bfd_link_split_section _bfd_generic_link_split_section

/* Strictly speaking, only MMIX uses this restricted format, but let's not
   stop anybody from shooting themselves in the foot.  */
#define mmo_set_arch_mach bfd_default_set_arch_mach
#define mmo_bfd_relax_section bfd_generic_relax_section
#define mmo_bfd_merge_sections bfd_generic_merge_sections
#define mmo_bfd_discard_group bfd_generic_discard_group

/* objcopy will be upset if we return -1 from bfd_get_reloc_upper_bound by
   using BFD_JUMP_TABLE_RELOCS (_bfd_norelocs) rather than 0.  FIXME: Most
   likely a bug in the _bfd_norelocs definition.

   On the other hand, we smuggle in an mmo object (because setting up ELF
   is too cumbersome) when linking (from other formats, presumably ELF) to
   represent the g255 entry.  We need to link that object, so need to say
   it has no relocs.  Upper bound for the size of the relocation table is
   the size of a NULL pointer, and we support "canonicalization" for that
   pointer.  */
#define mmo_bfd_reloc_type_lookup _bfd_norelocs_bfd_reloc_type_lookup

/* We want to copy time of creation, otherwise we'd use
   BFD_JUMP_TABLE_COPY (_bfd_generic).  */
#define mmo_bfd_merge_private_bfd_data _bfd_generic_bfd_merge_private_bfd_data
#define mmo_bfd_copy_private_section_data _bfd_generic_bfd_copy_private_section_data
#define mmo_bfd_copy_private_symbol_data _bfd_generic_bfd_copy_private_symbol_data
#define mmo_bfd_set_private_flags _bfd_generic_bfd_set_private_flags
#define mmo_bfd_print_private_bfd_data _bfd_generic_bfd_print_private_bfd_data

const bfd_target bfd_mmo_vec =
{
  "mmo",			/* name */
  bfd_target_mmo_flavour,
  BFD_ENDIAN_BIG,		/* target byte order */
  BFD_ENDIAN_BIG,		/* target headers byte order */

  /* FIXME: Might need adjustments.  */
  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT),

  /* FIXME: Might need adjustments.  */
  (SEC_CODE | SEC_DATA | SEC_HAS_CONTENTS | SEC_ALLOC | SEC_LOAD
   | SEC_READONLY | SEC_EXCLUDE | SEC_DEBUGGING | SEC_IN_MEMORY),
				/* section flags */
  0,				/* leading underscore */
  ' ',				/* ar_pad_char */
  16,				/* ar_max_namelen */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* data */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* hdrs */

  {
    _bfd_dummy_target,
    mmo_object_p,		/* bfd_check_format */
    _bfd_dummy_target,
    _bfd_dummy_target,
  },
  {
    bfd_false,
    mmo_mkobject,
    bfd_false,
    bfd_false,
  },
  {				/* bfd_write_contents */
    bfd_false,
    mmo_write_object_contents,
    bfd_false,
    bfd_false,
  },

  BFD_JUMP_TABLE_GENERIC (mmo),
  BFD_JUMP_TABLE_COPY (mmo),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (_bfd_noarchive),
  BFD_JUMP_TABLE_SYMBOLS (mmo),
  /* We have to provide a valid method for getting relocs, returning zero,
     so we can't say BFD_JUMP_TABLE_RELOCS (_bfd_norelocs).  */
  BFD_JUMP_TABLE_RELOCS (mmo),
  BFD_JUMP_TABLE_WRITE (mmo),
  BFD_JUMP_TABLE_LINK (mmo),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  NULL,

  NULL
};
