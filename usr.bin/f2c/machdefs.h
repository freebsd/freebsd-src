#define TYLENG	TYLONG		/* char string length field */

#define TYINT	TYLONG
#define SZADDR	4
#define SZSHORT	2
#define SZINT	4

#define SZLONG	4
#define SZLENG	SZLONG

#define SZDREAL 8

/* Alignment restrictions */

#define ALIADDR SZADDR
#define ALISHORT SZSHORT
#define ALILONG 4
#define ALIDOUBLE 8
#define ALIINT	ALILONG
#define ALILENG	ALILONG

#define BLANKCOMMON "_BLNK__"		/* Name for the unnamed
					   common block; this is unique
					   because of underscores */

#define LABELFMT "%s:\n"

#define MAXREGVAR 4
#define TYIREG TYLONG
#define MSKIREG  (M(TYSHORT)|M(TYLONG))	/* allowed types of DO indicies
					   which can be put in registers */
