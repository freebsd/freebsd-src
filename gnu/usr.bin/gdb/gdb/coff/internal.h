/* Internal format of COFF object file data structures, for GNU BFD.
   This file is part of BFD, the Binary File Descriptor library.  */

/* First, make "signed char" work, even on old compilers. */
#ifndef signed
#ifndef __STDC__
#define	signed			/**/
#endif
#endif

/********************** FILE HEADER **********************/
struct internal_filehdr
{
  unsigned short f_magic;	/* magic number			*/
  unsigned short f_nscns;	/* number of sections		*/
  long f_timdat;		/* time & date stamp		*/
  bfd_vma f_symptr;		/* file pointer to symtab	*/
  long f_nsyms;			/* number of symtab entries	*/
  unsigned short f_opthdr;	/* sizeof(optional hdr)		*/
  unsigned short f_flags;	/* flags			*/
};

/* Bits for f_flags:
 *	F_RELFLG	relocation info stripped from file
 *	F_EXEC		file is executable (no unresolved external references)
 *	F_LNNO		line numbers stripped from file
 *	F_LSYMS		local symbols stripped from file
 *	F_AR16WR	file is 16-bit little-endian
 *	F_AR32WR	file is 32-bit little-endian
 *	F_AR32W		file is 32-bit big-endian
 *	F_DYNLOAD	rs/6000 aix: dynamically loadable w/imports & exports
 *	F_SHROBJ	rs/6000 aix: file is a shared object
 */

#define	F_RELFLG	(0x0001)
#define	F_EXEC		(0x0002)
#define	F_LNNO		(0x0004)
#define	F_LSYMS		(0x0008)
#define	F_AR16WR	(0x0080)
#define	F_AR32WR	(0x0100)
#define	F_AR32W     	(0x0200)
#define	F_DYNLOAD	(0x1000)
#define	F_SHROBJ	(0x2000)

/********************** AOUT "OPTIONAL HEADER" **********************/
struct internal_aouthdr
{
  short magic;			/* type of file				*/
  short vstamp;			/* version stamp			*/
  bfd_vma tsize;		/* text size in bytes, padded to FW bdry*/
  bfd_vma dsize;		/* initialized data "  "		*/
  bfd_vma bsize;		/* uninitialized data "   "		*/
  bfd_vma entry;		/* entry pt.				*/
  bfd_vma text_start;		/* base of text used for this file */
  bfd_vma data_start;		/* base of data used for this file */

  /* i960 stuff */
  unsigned long tagentries;	/* number of tag entries to follow */

  /* RS/6000 stuff */
  unsigned long o_toc;		/* address of TOC			*/
  short o_snentry;		/* section number for entry point */
  short o_sntext;		/* section number for text	*/
  short o_sndata;		/* section number for data	*/
  short o_sntoc;		/* section number for toc	*/
  short o_snloader;		/* section number for loader section */
  short o_snbss;		/* section number for bss	*/
  short o_algntext;		/* max alignment for text	*/
  short o_algndata;		/* max alignment for data	*/
  short o_modtype;		/* Module type field, 1R,RE,RO	*/
  unsigned long o_maxstack;	/* max stack size allowed.	*/

  /* ECOFF stuff */
  bfd_vma bss_start;		/* Base of bss section.		*/
  bfd_vma gp_value;		/* GP register value.		*/
  unsigned long gprmask;	/* General registers used.	*/
  unsigned long cprmask[4];	/* Coprocessor registers used.	*/
  unsigned long fprmask;	/* Floating pointer registers used.  */

  /* Apollo stuff */
  long o_inlib;
  long o_sri;
  long vid[2];
};

/********************** STORAGE CLASSES **********************/

/* This used to be defined as -1, but now n_sclass is unsigned.  */
#define C_EFCN		0xff	/* physical end of function	*/
#define C_NULL		0
#define C_AUTO		1	/* automatic variable		*/
#define C_EXT		2	/* external symbol		*/
#define C_STAT		3	/* static			*/
#define C_REG		4	/* register variable		*/
#define C_EXTDEF	5	/* external definition		*/
#define C_LABEL		6	/* label			*/
#define C_ULABEL	7	/* undefined label		*/
#define C_MOS		8	/* member of structure		*/
#define C_ARG		9	/* function argument		*/
#define C_STRTAG	10	/* structure tag		*/
#define C_MOU		11	/* member of union		*/
#define C_UNTAG		12	/* union tag			*/
#define C_TPDEF		13	/* type definition		*/
#define C_USTATIC	14	/* undefined static		*/
#define C_ENTAG		15	/* enumeration tag		*/
#define C_MOE		16	/* member of enumeration	*/
#define C_REGPARM	17	/* register parameter		*/
#define C_FIELD		18	/* bit field			*/
#define C_AUTOARG	19	/* auto argument		*/
#define C_LASTENT	20	/* dummy entry (end of block)	*/
#define C_BLOCK		100	/* ".bb" or ".eb"		*/
#define C_FCN		101	/* ".bf" or ".ef"		*/
#define C_EOS		102	/* end of structure		*/
#define C_FILE		103	/* file name			*/
#define C_LINE		104	/* line # reformatted as symbol table entry */
#define C_ALIAS	 	105	/* duplicate tag		*/
#define C_HIDDEN	106	/* ext symbol in dmert public lib */

 /* New storage classes for 80960 */

/* C_LEAFPROC is obsolete.  Use C_LEAFEXT or C_LEAFSTAT */
#define C_LEAFPROC	108	/* Leaf procedure, "call" via BAL */

#define C_SCALL		107	/* Procedure reachable via system call */
#define C_LEAFEXT       108	/* External leaf */
#define C_LEAFSTAT      113	/* Static leaf */
#define C_OPTVAR	109	/* Optimized variable		*/
#define C_DEFINE	110	/* Preprocessor #define		*/
#define C_PRAGMA	111	/* Advice to compiler or linker	*/
#define C_SEGMENT	112	/* 80960 segment name		*/

  /* Storage classes for m88k */
#define C_SHADOW        107     /* shadow symbol                */
#define C_VERSION       108     /* coff version symbol          */

 /* New storage classes for RS/6000 */
#define C_HIDEXT        107	/* Un-named external symbol */
#define C_BINCL         108	/* Marks beginning of include file */
#define C_EINCL         109	/* Marks ending of include file */

 /* storage classes for stab symbols for RS/6000 */
#define C_GSYM          (0x80)
#define C_LSYM          (0x81)
#define C_PSYM          (0x82)
#define C_RSYM          (0x83)
#define C_RPSYM         (0x84)
#define C_STSYM         (0x85)
#define C_TCSYM         (0x86)
#define C_BCOMM         (0x87)
#define C_ECOML         (0x88)
#define C_ECOMM         (0x89)
#define C_DECL          (0x8c)
#define C_ENTRY         (0x8d)
#define C_FUN           (0x8e)
#define C_BSTAT         (0x8f)
#define C_ESTAT         (0x90)

/********************** SECTION HEADER **********************/
struct internal_scnhdr
{
  char s_name[8];		/* section name			*/
  bfd_vma s_paddr;		/* physical address, aliased s_nlib */
  bfd_vma s_vaddr;		/* virtual address		*/
  bfd_vma s_size;		/* section size			*/
  bfd_vma s_scnptr;		/* file ptr to raw data for section */
  bfd_vma s_relptr;		/* file ptr to relocation	*/
  bfd_vma s_lnnoptr;		/* file ptr to line numbers	*/
  unsigned long s_nreloc;	/* number of relocation entries	*/
  unsigned long s_nlnno;	/* number of line number entries*/
  long s_flags;			/* flags			*/
  long s_align;			/* used on I960			*/
};

/*
 * s_flags "type"
 */
#define STYP_REG	 (0x0000)	/* "regular": allocated, relocated, loaded */
#define STYP_DSECT	 (0x0001)	/* "dummy":  relocated only*/
#define STYP_NOLOAD	 (0x0002)	/* "noload": allocated, relocated, not loaded */
#define STYP_GROUP	 (0x0004)	/* "grouped": formed of input sections */
#define STYP_PAD	 (0x0008)	/* "padding": not allocated, not relocated, loaded */
#define STYP_COPY	 (0x0010)	/* "copy": for decision function used by field update;  not allocated, not relocated,
									     loaded; reloc & lineno entries processed normally */
#define STYP_TEXT	 (0x0020)	/* section contains text only */
#define S_SHRSEG	 (0x0020)	/* In 3b Update files (output of ogen), sections which appear in SHARED segments of the Pfile
									     will have the S_SHRSEG flag set by ogen, to inform dufr that updating 1 copy of the proc. will
									     update all process invocations. */
#define STYP_DATA	 (0x0040)	/* section contains data only */
#define STYP_BSS	 (0x0080)	/* section contains bss only */
#define S_NEWFCN	 (0x0100)	/* In a minimal file or an update file, a new function (as compared with a replaced function) */
#define STYP_INFO	 (0x0200)	/* comment: not allocated not relocated, not loaded */
#define STYP_OVER	 (0x0400)	/* overlay: relocated not allocated or loaded */
#define STYP_LIB	 (0x0800)	/* for .lib: same as INFO */
#define STYP_MERGE	 (0x2000)	/* merge section -- combines with text, data or bss sections only */
#define STYP_REVERSE_PAD (0x4000)	/* section will be padded with no-op instructions wherever padding is necessary and there is a
					
									     word of contiguous bytes
									     beginning on a word boundary. */

#define STYP_LIT	0x8020	/* Literal data (like STYP_TEXT) */
/********************** LINE NUMBERS **********************/

/* 1 line number entry for every "breakpointable" source line in a section.
 * Line numbers are grouped on a per function basis; first entry in a function
 * grouping will have l_lnno = 0 and in place of physical address will be the
 * symbol table index of the function name.
 */

struct internal_lineno
{
  union
  {
    long l_symndx;		/* function name symbol index, iff l_lnno == 0*/
    long l_paddr;		/* (physical) address of line number	*/
  }     l_addr;
  unsigned long l_lnno;		/* line number		*/
};

/********************** SYMBOLS **********************/

#define SYMNMLEN	8	/* # characters in a symbol name	*/
#define FILNMLEN	14	/* # characters in a file name		*/
#define DIMNUM		4	/* # array dimensions in auxiliary entry */

struct internal_syment
{
  union
  {
    char _n_name[SYMNMLEN];	/* old COFF version	*/
    struct
    {
      long _n_zeroes;		/* new == 0		*/
      long _n_offset;		/* offset into string table */
    }      _n_n;
    char *_n_nptr[2];		/* allows for overlaying	*/
  }     _n;
  long n_value;			/* value of symbol		*/
  short n_scnum;		/* section number		*/
  unsigned short n_flags;	/* copy of flags from filhdr	*/
  unsigned short n_type;	/* type and derived type	*/
  unsigned char n_sclass;		/* storage class		*/
  char n_numaux;		/* number of aux. entries	*/
};

#define n_name		_n._n_name
#define n_zeroes	_n._n_n._n_zeroes
#define n_offset	_n._n_n._n_offset


/* Relocatable symbols have number of the section in which they are defined,
   or one of the following: */

#define N_UNDEF	((short)0)	/* undefined symbol */
#define N_ABS	((short)-1)	/* value of symbol is absolute */
#define N_DEBUG	((short)-2)	/* debugging symbol -- value is meaningless */
#define N_TV	((short)-3)	/* indicates symbol needs preload transfer vector */
#define P_TV	((short)-4)	/* indicates symbol needs postload transfer vector*/

/*
 * Type of a symbol, in low N bits of the word
 */
#define T_NULL		0
#define T_VOID		1	/* function argument (only used by compiler) */
#define T_CHAR		2	/* character		*/
#define T_SHORT		3	/* short integer	*/
#define T_INT		4	/* integer		*/
#define T_LONG		5	/* long integer		*/
#define T_FLOAT		6	/* floating point	*/
#define T_DOUBLE	7	/* double word		*/
#define T_STRUCT	8	/* structure 		*/
#define T_UNION		9	/* union 		*/
#define T_ENUM		10	/* enumeration 		*/
#define T_MOE		11	/* member of enumeration*/
#define T_UCHAR		12	/* unsigned character	*/
#define T_USHORT	13	/* unsigned short	*/
#define T_UINT		14	/* unsigned integer	*/
#define T_ULONG		15	/* unsigned long	*/
#define T_LNGDBL	16	/* long double		*/

/*
 * derived types, in n_type
*/
#define DT_NON		(0)	/* no derived type */
#define DT_PTR		(1)	/* pointer */
#define DT_FCN		(2)	/* function */
#define DT_ARY		(3)	/* array */

#define BTYPE(x)	((x) & N_BTMASK)

#define ISPTR(x)	(((x) & N_TMASK) == (DT_PTR << N_BTSHFT))
#define ISFCN(x)	(((x) & N_TMASK) == (DT_FCN << N_BTSHFT))
#define ISARY(x)	(((x) & N_TMASK) == (DT_ARY << N_BTSHFT))
#define ISTAG(x)	((x)==C_STRTAG||(x)==C_UNTAG||(x)==C_ENTAG)
#define DECREF(x) ((((x)>>N_TSHIFT)&~N_BTMASK)|((x)&N_BTMASK))


union internal_auxent
{
  struct
  {

    union
    {
      long l;			/* str, un, or enum tag indx */
      struct coff_ptr_struct *p;
    }     x_tagndx;

    union
    {
      struct
      {
	unsigned short x_lnno;	/* declaration line number */
	unsigned short x_size;	/* str/union/array size */
      }      x_lnsz;
      long x_fsize;		/* size of function */
    }     x_misc;

    union
    {
      struct
      {				/* if ISFCN, tag, or .bb */
	long x_lnnoptr;		/* ptr to fcn line # */
	union
	{			/* entry ndx past block end */
	  long l;
	  struct coff_ptr_struct *p;
	}     x_endndx;
      }      x_fcn;

      struct
      {				/* if ISARY, up to 4 dimen. */
	unsigned short x_dimen[DIMNUM];
      }      x_ary;
    }     x_fcnary;

    unsigned short x_tvndx;	/* tv index */
  }      x_sym;

  union
  {
    char x_fname[FILNMLEN];
    struct
    {
      long x_zeroes;
      long x_offset;
    }      x_n;
  }     x_file;

  struct
  {
    long x_scnlen;		/* section length */
    unsigned short x_nreloc;	/* # relocation entries */
    unsigned short x_nlinno;	/* # line numbers */
  }      x_scn;

  struct
  {
    long x_tvfill;		/* tv fill value */
    unsigned short x_tvlen;	/* length of .tv */
    unsigned short x_tvran[2];	/* tv range */
  }      x_tv;			/* info about .tv section (in auxent of symbol .tv)) */

  /******************************************
   * RS/6000-specific auxent - last auxent for every external symbol
   ******************************************/
  struct
  {
    long x_scnlen;		/* csect length */
    long x_parmhash;		/* parm type hash index */
    unsigned short x_snhash;	/* sect num with parm hash */
    unsigned char x_smtyp;	/* symbol align and type */
    /* 0-4 - Log 2 of alignment */
    /* 5-7 - symbol type */
    unsigned char x_smclas;	/* storage mapping class */
    long x_stab;		/* dbx stab info index */
    unsigned short x_snstab;	/* sect num with dbx stab */
  }      x_csect;		/* csect definition information */

/* x_smtyp values:  */

#define	SMTYP_ALIGN(x)	((x) >> 3)	/* log2 of alignment */
#define	SMTYP_SMTYP(x)	((x) & 0x7)	/* symbol type */
/* Symbol type values:  */
#define	XTY_ER	0		/* External reference */
#define	XTY_SD	1		/* Csect definition */
#define	XTY_LD	2		/* Label definition */
#define XTY_CM	3		/* .BSS */
#define	XTY_EM	4		/* Error message */
#define	XTY_US	5		/* "Reserved for internal use" */

/* x_smclas values:  */

#define	XMC_PR	0		/* Read-only program code */
#define	XMC_RO	1		/* Read-only constant */
#define	XMC_DB	2		/* Read-only debug dictionary table */
#define	XMC_TC	3		/* Read-write general TOC entry */
#define	XMC_UA	4		/* Read-write unclassified */
#define	XMC_RW	5		/* Read-write data */
#define	XMC_GL	6		/* Read-only global linkage */
#define	XMC_XO	7		/* Read-only extended operation (simulated insn) */
#define	XMC_SV	8		/* Read-only supervisor call */
#define	XMC_BS	9		/* Read-write BSS */
#define	XMC_DS	10		/* Read-write descriptor csect */
#define	XMC_UC	11		/* Read-write unnamed Fortran common */
#define	XMC_TI	12		/* Read-only traceback index csect */
#define	XMC_TB	13		/* Read-only traceback table csect */
/* 		14	??? */
#define	XMC_TC0	15		/* Read-write TOC anchor for TOC addressability */


  /******************************************
   *  I960-specific *2nd* aux. entry formats
   ******************************************/
  struct
  {
    /* This is a very old typo that keeps getting propagated. */
#define x_stdindx x_stindx
    long x_stindx;		/* sys. table entry */
  }      x_sc;			/* system call entry */

  struct
  {
    unsigned long x_balntry;	/* BAL entry point */
  }      x_bal;			/* BAL-callable function */

  struct
  {
    unsigned long x_timestamp;	/* time stamp */
    char x_idstring[20];	/* producer identity string */
  }      x_ident;		/* Producer ident info */

};

/********************** RELOCATION DIRECTIVES **********************/

struct internal_reloc
{
  bfd_vma r_vaddr;		/* Virtual address of reference */
  long r_symndx;		/* Index into symbol table	*/
  unsigned short r_type;	/* Relocation type		*/
  unsigned char r_size;		/* Used by RS/6000 and ECOFF	*/
  unsigned char r_extern;	/* Used by ECOFF		*/
  unsigned long r_offset;	/* Used by RS/6000 and ECOFF	*/
};

#define R_RELBYTE	017
#define R_RELWORD	020
#define R_PCRBYTE	022
#define R_PCRWORD	023
#define R_PCRLONG	024

#define	R_DIR16		01
#define R_DIR32		06
#define	R_PCLONG	020
#define R_RELBYTE	017
#define R_RELWORD	020



#define R_PCR16L 128
#define R_PCR26L 129
#define R_VRT16  130
#define R_HVRT16 131
#define R_LVRT16 132
#define R_VRT32  133
#define R_RELLONG	(0x11)	/* Direct 32-bit relocation */
#define R_IPRSHORT	(0x18)
#define R_IPRMED 	(0x19)	/* 24-bit ip-relative relocation */
#define R_IPRLONG	(0x1a)
#define R_OPTCALL	(0x1b)	/* 32-bit optimizable call (leafproc/sysproc) */
#define R_OPTCALLX	(0x1c)	/* 64-bit optimizable call (leafproc/sysproc) */
#define R_GETSEG	(0x1d)
#define R_GETPA		(0x1e)
#define R_TAGWORD	(0x1f)
#define R_JUMPTARG	0x20	/* strange 29k 00xx00xx reloc */


#define R_MOVB1    	0x41	/* Special h8 16bit or 8 bit reloc for mov.b 	*/
#define R_MOVB2 	0x42	/* Special h8 opcode for 8bit which could be 16 */
#define R_JMP1     	0x43	/* Special h8 16bit jmp which could be pcrel 	*/
#define R_JMP2 		0x44	/* a branch which used to be a jmp 		*/
#define R_RELLONG_NEG  	0x45

#define R_JMPL1     	0x46	/* Special h8 24bit jmp which could be pcrel 	*/
#define R_JMPL_B8	0x47	/* a 8 bit pcrel which used to be a jmp  */

#define R_MOVLB1    	0x48	/* Special h8 24bit or 8 bit reloc for mov.b 	*/
#define R_MOVLB2 	0x49	/* Special h8 opcode for 8bit which could be 24 */

/* Z8k modes */
#define R_IMM16   0x01		/* 16 bit abs */
#define R_JR	  0x02		/* jr  8 bit disp */
#define R_IMM4L   0x23		/* low nibble */
#define R_IMM8    0x22		/* 8 bit abs */
#define R_IMM32   R_RELLONG	/* 32 bit abs */
#define R_CALL    R_DA		/* Absolute address which could be a callr */
#define R_JP	  R_DA		/* Absolute address which could be a jp */
#define R_REL16   0x04		/* 16 bit PC rel */
#define R_CALLR	  0x05		/* callr 12 bit disp */
#define R_SEG     0x10		/* set if in segmented mode */
#define R_IMM4H   0x24		/* high nibble */


/* H8500 modes */

#define R_H8500_IMM8  	1		/*  8 bit immediate 	*/
#define R_H8500_IMM16 	2		/* 16 bit immediate	*/
#define R_H8500_PCREL8 	3		/*  8 bit pcrel 	*/
#define R_H8500_PCREL16 4		/* 16 bit pcrel 	*/
#define R_H8500_HIGH8  	5		/* high 8 bits of 24 bit address */
#define R_H8500_LOW16 	7		/* low 16 bits of 24 bit immediate */
#define R_H8500_IMM24	6		/* 24 bit immediate */
#define R_H8500_IMM32   8               /* 32 bit immediate */
#define R_H8500_HIGH16  9		/* high 16 bits of 32 bit immediate */

/* SH modes */

#define R_SH_PCREL8 	3		/*  8 bit pcrel 	*/
#define R_SH_PCREL16 	4		/* 16 bit pcrel 	*/
#define R_SH_HIGH8  	5		/* high 8 bits of 24 bit address */
#define R_SH_LOW16 	7		/* low 16 bits of 24 bit immediate */
#define R_SH_IMM24	6		/* 24 bit immediate */
#define R_SH_PCDISP8BY4	9  		/* PC rel 8 bits *4 +ve */
#define R_SH_PCDISP8BY2	10  		/* PC rel 8 bits *2 +ve */
#define R_SH_PCDISP8    11  		/* 8 bit branch */
#define R_SH_PCDISP     12  		/* 12 bit branch */
#define R_SH_IMM32      14    		/* 32 bit immediate */
#define R_SH_IMM8   	16
#define R_SH_IMM8BY2    17
#define R_SH_IMM8BY4    18
#define R_SH_IMM4   	19
#define R_SH_IMM4BY2    20
#define R_SH_IMM4BY4    21
#define R_SH_PCRELIMM8BY2   22
#define R_SH_PCRELIMM8BY4   23



