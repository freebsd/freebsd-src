/* SGML Character Use: ISO Latin 1.
*/
#define EOFCHAR   '\032'      /* FUNCTION: EE (entity end: files). */
#define EOBCHAR   '\034'      /* NONCHAR: EOB (file entity: end of buffer. */
#define RSCHAR    '\012'      /* FUNCTION: RS (record start). */
#define RECHAR    '\015'      /* FUNCTION: RE (record end). */
#define TABCHAR   '\011'      /* FUNCTION: TAB (horizontal tab). */
#define SPCCHAR   '\040'      /* FUNCTION: SPACE (horizontal space). */
#define GENRECHAR '\010'      /* NONCHAR: Generated RE. */
#define DELCDATA  '\035'      /* NONCHAR: Delimiter for CDATA entity in
				 attribute value. */
#define DELSDATA  '\036'      /* NONCHAR: Delimiter for SDATA entity in
				 attribute value. */
#define DELNONCH  '\037'      /* NONCHAR: non-SGML character prefix. */

/* These two macros are used to handle non-SGML characters. A non-SGML
by character is represented by a DELNONCH character followed by
SHIFTNON(original_character).  SHIFTNON must transform any character
in the set 0, EOFCHAR, EOBCHAR, GENRECHAR, DELCDATA, DELSDATA,
DELNONCH into a character that is not one of the set 0, EOFCHAR,
EOBCHAR.  Furthermore UNSHIFTNON(SHIFTNON(c)) must be equal to c for
every character c in the former set. */
/* This is a simple definition that works for ASCII-like character sets. */
#define SHIFTNON(ch) ((UNCH)(ch) | 0100)
#define UNSHIFTNON(ch) ((UNCH)(ch) & ~0100)

/* A canonical NONSGML character. The character number that is shunned
in the reference concrete syntax and is not the number of a
significant (in the reference concrete syntax) character nor one of
the above characters nor 0. */
#define CANON_NONSGML 255

/* A canonical DATACHAR character. The character number that is not
shunned in the reference concrete syntax and is not the number of a
significant (in the reference concrete syntax) SGML character nor one
of the above characters. */
#define CANON_DATACHAR 254

/* Components for a formal public identifier for the whole of the
system character set.  Protect with ifndef so that it can be overriden
in config.h. */

#ifndef SYSTEM_CHARSET_DESIGNATING_SEQUENCE
#define SYSTEM_CHARSET_DESIGNATING_SEQUENCE "ESC 2/13 4/1"
#endif
#ifndef SYSTEM_CHARSET_OWNER
#define SYSTEM_CHARSET_OWNER "ISO Registration Number 100"
#endif
#ifndef SYSTEM_CHARSET_DESCRIPTION
#define SYSTEM_CHARSET_DESCRIPTION "ECMA-94 Right Part of Latin Alphabet Nr. 1"
#endif
