/* `a.out.adobe' differences from standard a.out files */

#ifndef __A_OUT_ADOBE_H__
#define __A_OUT_ADOBE_H__

#define	BYTES_IN_WORD	4

/* Struct external_exec is the same.  */

/* This is the layout on disk of the 32-bit or 64-bit exec header. */

struct external_exec 
{
  bfd_byte e_info[4];		/* magic number and stuff		*/
  bfd_byte e_text[BYTES_IN_WORD]; /* length of text section in bytes	*/
  bfd_byte e_data[BYTES_IN_WORD]; /* length of data section in bytes	*/
  bfd_byte e_bss[BYTES_IN_WORD]; /* length of bss area in bytes 		*/
  bfd_byte e_syms[BYTES_IN_WORD]; /* length of symbol table in bytes 	*/
  bfd_byte e_entry[BYTES_IN_WORD]; /* start address 			*/
  bfd_byte e_trsize[BYTES_IN_WORD]; /* length of text relocation info	*/
  bfd_byte e_drsize[BYTES_IN_WORD]; /* length of data relocation info 	*/
};

#define	EXEC_BYTES_SIZE	(4 + BYTES_IN_WORD * 7)

/* Magic numbers for a.out files */

#undef	ZMAGIC
#define	ZMAGIC	0xAD0BE		/* Cute, eh?  */
#undef	OMAGIC
#undef	NMAGIC

#define N_BADMAG(x)	  ((x).a_info != ZMAGIC)

/* By default, segment size is constant.  But some machines override this
   to be a function of the a.out header (e.g. machine type).  */
#ifndef	N_SEGSIZE
#define	N_SEGSIZE(x)	SEGMENT_SIZE
#endif
#undef N_SEGSIZE   /* FIXMEXXXX */

/* Segment information for the a.out.Adobe format is specified after the
   file header.  It contains N segment descriptors, followed by one with
   a type of zero.  

   The actual text of the segments starts at N_TXTOFF in the file,
   regardless of how many or how few segment headers there are.  */

struct external_segdesc {
	unsigned char e_type[1];
	unsigned char e_size[3];
	unsigned char e_virtbase[4];
	unsigned char e_filebase[4];
};

struct internal_segdesc {
	unsigned int	a_type:8;	/* Segment type N_TEXT, N_DATA, 0 */
	unsigned int 	a_size:24;	/* Segment size */
	bfd_vma		a_virtbase;	/* Virtual address */
	unsigned int	a_filebase;	/* Base address in object file */
};

#define N_TXTADDR(x) \

/* This is documented to be at 1024, but appears to really be at 2048.
   FIXME?!  */
#define N_TXTOFF(x)	2048

#define	N_TXTSIZE(x) ((x).a_text)

#define N_DATADDR(x)

#define N_BSSADDR(x)

/* Offsets of the various portions of the file after the text segment.  */

#define N_DATOFF(x)	( N_TXTOFF(x) + N_TXTSIZE(x) )
#define N_TRELOFF(x)	( N_DATOFF(x) + (x).a_data )
#define N_DRELOFF(x)	( N_TRELOFF(x) + (x).a_trsize )
#define N_SYMOFF(x)	( N_DRELOFF(x) + (x).a_drsize )
#define N_STROFF(x)	( N_SYMOFF(x) + (x).a_syms )

/* Symbols */
struct external_nlist {
  bfd_byte e_strx[BYTES_IN_WORD];	/* index into string table of name */
  bfd_byte e_type[1];			/* type of symbol */
  bfd_byte e_other[1];			/* misc info (usually empty) */
  bfd_byte e_desc[2];			/* description field */
  bfd_byte e_value[BYTES_IN_WORD];	/* value of symbol */
};

#define EXTERNAL_NLIST_SIZE (BYTES_IN_WORD+4+BYTES_IN_WORD)

struct internal_nlist {
  unsigned long n_strx;			/* index into string table of name */
  unsigned char n_type;			/* type of symbol */
  unsigned char n_other;		/* misc info (usually empty) */
  unsigned short n_desc;		/* description field */
  bfd_vma n_value;			/* value of symbol */
};

/* The n_type field is the symbol type, containing:  */

#define N_UNDF	0	/* Undefined symbol */
#define N_ABS 	2	/* Absolute symbol -- defined at particular addr */
#define N_TEXT 	4	/* Text sym -- defined at offset in text seg */
#define N_DATA 	6	/* Data sym -- defined at offset in data seg */
#define N_BSS 	8	/* BSS  sym -- defined at offset in zero'd seg */
#define	N_COMM	0x12	/* Common symbol (visible after shared lib dynlink) */
#define N_FN	0x1f	/* File name of .o file */
#define	N_FN_SEQ 0x0C	/* N_FN from Sequent compilers (sigh) */
/* Note: N_EXT can only be usefully OR-ed with N_UNDF, N_ABS, N_TEXT,
   N_DATA, or N_BSS.  When the low-order bit of other types is set,
   (e.g. N_WARNING versus N_FN), they are two different types.  */
#define N_EXT 	1	/* External symbol (as opposed to local-to-this-file) */
#define N_TYPE  0x1e
#define N_STAB 	0xe0	/* If any of these bits are on, it's a debug symbol */

#define N_INDR 0x0a

/* The following symbols refer to set elements.
   All the N_SET[ATDB] symbols with the same name form one set.
   Space is allocated for the set in the text section, and each set
   elements value is stored into one word of the space.
   The first word of the space is the length of the set (number of elements).

   The address of the set is made into an N_SETV symbol
   whose name is the same as the name of the set.
   This symbol acts like a N_DATA global symbol
   in that it can satisfy undefined external references.  */

/* These appear as input to LD, in a .o file.  */
#define	N_SETA	0x14		/* Absolute set element symbol */
#define	N_SETT	0x16		/* Text set element symbol */
#define	N_SETD	0x18		/* Data set element symbol */
#define	N_SETB	0x1A		/* Bss set element symbol */

/* This is output from LD.  */
#define N_SETV	0x1C		/* Pointer to set vector in data area.  */

/* Warning symbol. The text gives a warning message, the next symbol
   in the table will be undefined. When the symbol is referenced, the
   message is printed.  */

#define	N_WARNING 0x1e

/* Relocations 

  There	are two types of relocation flavours for a.out systems,
  standard and extended. The standard form is used on systems where the
  instruction has room for all the bits of an offset to the operand, whilst
  the extended form is used when an address operand has to be split over n
  instructions. Eg, on the 68k, each move instruction can reference
  the target with a displacement of 16 or 32 bits. On the sparc, move
  instructions use an offset of 14 bits, so the offset is stored in
  the reloc field, and the data in the section is ignored.
*/

/* This structure describes a single relocation to be performed.
   The text-relocation section of the file is a vector of these structures,
   all of which apply to the text section.
   Likewise, the data-relocation section applies to the data section.  */

struct reloc_std_external {
  bfd_byte r_address[BYTES_IN_WORD];	/* offset of of data to relocate */
  bfd_byte r_index[3];	/* symbol table index of symbol 	*/
  bfd_byte r_type[1];	/* relocation type			*/
};

#define	RELOC_STD_BITS_PCREL_BIG	0x80
#define	RELOC_STD_BITS_PCREL_LITTLE	0x01

#define	RELOC_STD_BITS_LENGTH_BIG	0x60
#define	RELOC_STD_BITS_LENGTH_SH_BIG	5	/* To shift to units place */
#define	RELOC_STD_BITS_LENGTH_LITTLE	0x06
#define	RELOC_STD_BITS_LENGTH_SH_LITTLE	1

#define	RELOC_STD_BITS_EXTERN_BIG	0x10
#define	RELOC_STD_BITS_EXTERN_LITTLE	0x08

#define	RELOC_STD_BITS_BASEREL_BIG	0x08
#define	RELOC_STD_BITS_BASEREL_LITTLE	0x08

#define	RELOC_STD_BITS_JMPTABLE_BIG	0x04
#define	RELOC_STD_BITS_JMPTABLE_LITTLE	0x04

#define	RELOC_STD_BITS_RELATIVE_BIG	0x02
#define	RELOC_STD_BITS_RELATIVE_LITTLE	0x02

#define	RELOC_STD_SIZE	(BYTES_IN_WORD + 3 + 1)		/* Bytes per relocation entry */

struct reloc_std_internal
{
  bfd_vma r_address;		/* Address (within segment) to be relocated.  */
  /* The meaning of r_symbolnum depends on r_extern.  */
  unsigned int r_symbolnum:24;
  /* Nonzero means value is a pc-relative offset
     and it should be relocated for changes in its own address
     as well as for changes in the symbol or section specified.  */
  unsigned int r_pcrel:1;
  /* Length (as exponent of 2) of the field to be relocated.
     Thus, a value of 2 indicates 1<<2 bytes.  */
  unsigned int r_length:2;
  /* 1 => relocate with value of symbol.
     r_symbolnum is the index of the symbol
     in files the symbol table.
     0 => relocate with the address of a segment.
     r_symbolnum is N_TEXT, N_DATA, N_BSS or N_ABS
     (the N_EXT bit may be set also, but signifies nothing).  */
  unsigned int r_extern:1;
  /* The next three bits are for SunOS shared libraries, and seem to
     be undocumented.  */
  unsigned int r_baserel:1;	/* Linkage table relative */
  unsigned int r_jmptable:1;	/* pc-relative to jump table */
  unsigned int r_relative:1;	/* "relative relocation" */
  /* unused */
  unsigned int r_pad:1;		/* Padding -- set to zero */
};


/* EXTENDED RELOCS  */

struct reloc_ext_external {
  bfd_byte r_address[BYTES_IN_WORD];	/* offset of of data to relocate 	*/
  bfd_byte r_index[3];	/* symbol table index of symbol 	*/
  bfd_byte r_type[1];	/* relocation type			*/
  bfd_byte r_addend[BYTES_IN_WORD];	/* datum addend				*/
};

#define	RELOC_EXT_BITS_EXTERN_BIG	0x80
#define	RELOC_EXT_BITS_EXTERN_LITTLE	0x01

#define	RELOC_EXT_BITS_TYPE_BIG		0x1F
#define	RELOC_EXT_BITS_TYPE_SH_BIG	0
#define	RELOC_EXT_BITS_TYPE_LITTLE	0xF8
#define	RELOC_EXT_BITS_TYPE_SH_LITTLE	3

/* Bytes per relocation entry */
#define	RELOC_EXT_SIZE	(BYTES_IN_WORD + 3 + 1 + BYTES_IN_WORD)

enum reloc_type
{
  /* simple relocations */
  RELOC_8,			/* data[0:7] = addend + sv 		*/
  RELOC_16,			/* data[0:15] = addend + sv 		*/
  RELOC_32,			/* data[0:31] = addend + sv 		*/
  /* pc-rel displacement */
  RELOC_DISP8,			/* data[0:7] = addend - pc + sv 	*/
  RELOC_DISP16,			/* data[0:15] = addend - pc + sv 	*/
  RELOC_DISP32,			/* data[0:31] = addend - pc + sv 	*/
  /* Special */
  RELOC_WDISP30,		/* data[0:29] = (addend + sv - pc)>>2 	*/
  RELOC_WDISP22,		/* data[0:21] = (addend + sv - pc)>>2 	*/
  RELOC_HI22,			/* data[0:21] = (addend + sv)>>10 	*/
  RELOC_22,			/* data[0:21] = (addend + sv) 		*/
  RELOC_13,			/* data[0:12] = (addend + sv)		*/
  RELOC_LO10,			/* data[0:9] = (addend + sv)		*/
  RELOC_SFA_BASE,		
  RELOC_SFA_OFF13,
  /* P.I.C. (base-relative) */
  RELOC_BASE10,  		/* Not sure - maybe we can do this the */
  RELOC_BASE13,			/* right way now */
  RELOC_BASE22,
  /* for some sort of pc-rel P.I.C. (?) */
  RELOC_PC10,
  RELOC_PC22,
  /* P.I.C. jump table */
  RELOC_JMP_TBL,
  /* reputedly for shared libraries somehow */
  RELOC_SEGOFF16,
  RELOC_GLOB_DAT,
  RELOC_JMP_SLOT,
  RELOC_RELATIVE,

  RELOC_11,	
  RELOC_WDISP2_14,
  RELOC_WDISP19,
  RELOC_HHI22,			/* data[0:21] = (addend + sv) >> 42     */
  RELOC_HLO10,			/* data[0:9] = (addend + sv) >> 32      */
  
  /* 29K relocation types */
  RELOC_JUMPTARG,
  RELOC_CONST,
  RELOC_CONSTH,
  
  NO_RELOC
  };


struct reloc_internal {
  bfd_vma r_address;		/* offset of of data to relocate 	*/
  long	r_index;		/* symbol table index of symbol 	*/
  enum reloc_type r_type;	/* relocation type			*/
  bfd_vma r_addend;		/* datum addend				*/
};

#endif				/* __A_OUT_ADOBE_H__ */
