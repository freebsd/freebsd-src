/* SGML Character Use: EBCDIC
*/

#define EOFCHAR   '\077'      /* FUNCTION: EE (entity end: files). */
#define EOBCHAR   '\034'      /* NONCHAR: EOB (file entity: end of buffer. */
#define RSCHAR    '\045'      /* FUNCTION: RS (record start). */
#define RECHAR    '\015'      /* FUNCTION: RE (record end). */
#define TABCHAR   '\005'      /* FUNCTION: TAB (horizontal tab). */
#define SPCCHAR   '\100'      /* FUNCTION: SPACE (horizontal space). */
#define GENRECHAR '\026'      /* NONCHAR: Generated RE. */
#define DELCDATA  '\035'      /* NONCHAR: Delimiter for CDATA entity in
				 attribute value. */
#define DELSDATA  '\036'      /* NONCHAR: Delimiter for SDATA entity in
				 attribute value. */
#define DELNONCH  '\037'      /* NONCHAR: non-SGML character prefix. */

/* This should work for EBCDIC.  See comment in latin1.h. */
#define SHIFTNON(ch) ((UNCH)(ch) | 0200)
#define UNSHIFTNON(ch) ((UNCH)(ch) & ~0200)

/* See comment in latin1.h. */
#define CANON_NONSGML 255

/* See comment in latin1.h. */
#define CANON_DATACHAR 254
