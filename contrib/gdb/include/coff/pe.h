/* PE COFF header information */

#ifndef _PE_H
#define _PE_H

/* NT specific file attributes */
#define IMAGE_FILE_RELOCS_STRIPPED           0x0001
#define IMAGE_FILE_EXECUTABLE_IMAGE          0x0002
#define IMAGE_FILE_LINE_NUMS_STRIPPED        0x0004
#define IMAGE_FILE_LOCAL_SYMS_STRIPPED       0x0008
#define IMAGE_FILE_BYTES_REVERSED_LO         0x0080
#define IMAGE_FILE_32BIT_MACHINE             0x0100
#define IMAGE_FILE_DEBUG_STRIPPED            0x0200
#define IMAGE_FILE_SYSTEM                    0x1000
#define IMAGE_FILE_DLL                       0x2000
#define IMAGE_FILE_BYTES_REVERSED_HI         0x8000

/* additional flags to be set for section headers to allow the NT loader to
   read and write to the section data (to replace the addresses of data in
   dlls for one thing); also to execute the section in .text's case */
#define IMAGE_SCN_MEM_DISCARDABLE 0x02000000
#define IMAGE_SCN_MEM_EXECUTE     0x20000000
#define IMAGE_SCN_MEM_READ        0x40000000
#define IMAGE_SCN_MEM_WRITE       0x80000000

/*
 * Section characteristics added for ppc-nt
 */

#define IMAGE_SCN_TYPE_NO_PAD                0x00000008  /* Reserved. */

#define IMAGE_SCN_CNT_CODE                   0x00000020  /* Section contains code. */
#define IMAGE_SCN_CNT_INITIALIZED_DATA       0x00000040  /* Section contains initialized data. */
#define IMAGE_SCN_CNT_UNINITIALIZED_DATA     0x00000080  /* Section contains uninitialized data. */

#define IMAGE_SCN_LNK_OTHER                  0x00000100  /* Reserved. */
#define IMAGE_SCN_LNK_INFO                   0x00000200  /* Section contains comments or some other type of information. */
#define IMAGE_SCN_LNK_REMOVE                 0x00000800  /* Section contents will not become part of image. */
#define IMAGE_SCN_LNK_COMDAT                 0x00001000  /* Section contents comdat. */

#define IMAGE_SCN_MEM_FARDATA                0x00008000

#define IMAGE_SCN_MEM_PURGEABLE              0x00020000
#define IMAGE_SCN_MEM_16BIT                  0x00020000
#define IMAGE_SCN_MEM_LOCKED                 0x00040000
#define IMAGE_SCN_MEM_PRELOAD                0x00080000

#define IMAGE_SCN_ALIGN_1BYTES               0x00100000
#define IMAGE_SCN_ALIGN_2BYTES               0x00200000
#define IMAGE_SCN_ALIGN_4BYTES               0x00300000
#define IMAGE_SCN_ALIGN_8BYTES               0x00400000
#define IMAGE_SCN_ALIGN_16BYTES              0x00500000  /* Default alignment if no others are specified. */
#define IMAGE_SCN_ALIGN_32BYTES              0x00600000
#define IMAGE_SCN_ALIGN_64BYTES              0x00700000


#define IMAGE_SCN_LNK_NRELOC_OVFL            0x01000000  /* Section contains extended relocations. */
#define IMAGE_SCN_MEM_NOT_CACHED             0x04000000  /* Section is not cachable.               */
#define IMAGE_SCN_MEM_NOT_PAGED              0x08000000  /* Section is not pageable.               */
#define IMAGE_SCN_MEM_SHARED                 0x10000000  /* Section is shareable.                  */


/* Magic values that are true for all dos/nt implementations */
#define DOSMAGIC       0x5a4d  
#define NT_SIGNATURE   0x00004550

  /* NT allows long filenames, we want to accommodate this.  This may break
     some of the bfd functions */
#undef  FILNMLEN
#define FILNMLEN	18	/* # characters in a file name		*/


#ifdef COFF_IMAGE_WITH_PE
/* The filehdr is only weired in images */

#undef FILHDR
struct external_PE_filehdr
{
  /* DOS header fields */
  char e_magic[2];		/* Magic number, 0x5a4d */
  char e_cblp[2];		/* Bytes on last page of file, 0x90 */
  char e_cp[2];			/* Pages in file, 0x3 */
  char e_crlc[2];		/* Relocations, 0x0 */
  char e_cparhdr[2];		/* Size of header in paragraphs, 0x4 */
  char e_minalloc[2];		/* Minimum extra paragraphs needed, 0x0 */
  char e_maxalloc[2];		/* Maximum extra paragraphs needed, 0xFFFF */
  char e_ss[2];			/* Initial (relative) SS value, 0x0 */
  char e_sp[2];			/* Initial SP value, 0xb8 */
  char e_csum[2];		/* Checksum, 0x0 */
  char e_ip[2];			/* Initial IP value, 0x0 */
  char e_cs[2];			/* Initial (relative) CS value, 0x0 */
  char e_lfarlc[2];		/* File address of relocation table, 0x40 */
  char e_ovno[2];		/* Overlay number, 0x0 */
  char e_res[4][2];		/* Reserved words, all 0x0 */
  char e_oemid[2];		/* OEM identifier (for e_oeminfo), 0x0 */
  char e_oeminfo[2];		/* OEM information; e_oemid specific, 0x0 */
  char e_res2[10][2];		/* Reserved words, all 0x0 */
  char e_lfanew[4];		/* File address of new exe header, 0x80 */
  char dos_message[16][4];	/* other stuff, always follow DOS header */
  char nt_signature[4];		/* required NT signature, 0x4550 */ 

  /* From standard header */  


  char f_magic[2];		/* magic number			*/
  char f_nscns[2];		/* number of sections		*/
  char f_timdat[4];		/* time & date stamp		*/
  char f_symptr[4];		/* file pointer to symtab	*/
  char f_nsyms[4];		/* number of symtab entries	*/
  char f_opthdr[2];		/* sizeof(optional hdr)		*/
  char f_flags[2];		/* flags			*/

};


#define FILHDR struct external_PE_filehdr


#endif

typedef struct 
{
  AOUTHDR standard;

  /* NT extra fields; see internal.h for descriptions */
  char  ImageBase[4];
  char  SectionAlignment[4];
  char  FileAlignment[4];
  char  MajorOperatingSystemVersion[2];
  char  MinorOperatingSystemVersion[2];
  char  MajorImageVersion[2];
  char  MinorImageVersion[2];
  char  MajorSubsystemVersion[2];
  char  MinorSubsystemVersion[2];
  char  Reserved1[4];
  char  SizeOfImage[4];
  char  SizeOfHeaders[4];
  char  CheckSum[4];
  char  Subsystem[2];
  char  DllCharacteristics[2];
  char  SizeOfStackReserve[4];
  char  SizeOfStackCommit[4];
  char  SizeOfHeapReserve[4];
  char  SizeOfHeapCommit[4];
  char  LoaderFlags[4];
  char  NumberOfRvaAndSizes[4];
  /* IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES]; */
  char  DataDirectory[16][2][4]; /* 16 entries, 2 elements/entry, 4 chars */

} PEAOUTHDR;


#undef AOUTSZ
#define AOUTSZ sizeof(PEAOUTHDR)

#undef  E_FILNMLEN
#define E_FILNMLEN	18	/* # characters in a file name		*/
#endif



