/* Parameters about the a.out format, based on the host system on which
   the program is compiled.  */

/* Address of data segment in memory after it is loaded.
   It is up to you to define SEGMENT_SIZE
   on machines not listed here.  */
#ifndef SEGMENT_SIZE
#if defined(hp300) || defined(pyr)
#define SEGMENT_SIZE page_size
#endif
#ifdef	sony
#define	SEGMENT_SIZE	0x1000
#endif	/* Sony.  */
#ifdef is68k
#define SEGMENT_SIZE 0x20000
#endif
#if defined(m68k) && defined(PORTAR)
#define TARGET_PAGE_SIZE 0x400
#define SEGMENT_SIZE TARGET_PAGE_SIZE
#endif
#endif /*!defined(SEGMENT_SIZE)*/

