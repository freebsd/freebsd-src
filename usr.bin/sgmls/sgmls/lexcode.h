/* Definitions of lexical codes needed by both lextaba.c and lexrf.c. */

#define FCE    27   /* FRE  Free character in use as an entity reference */
#define FRE     0   /* FREECHAR that is not in a CON delimiter-in-context. */
#define LITC   21   /* LIT LITA PIC or EE in use as a literal terminator */
#define MINLITC 13  /* LIT LITA as literal terminator in minimum data */
#define MSC3   15   /* ]    Also MSC[2]. */
#define NET    17   /* /    When enabled. */
#define ETI    16   /* /    Actually ETAGO[2] */
#define SPCR   19   /* Space in use as SR8. */
#define TGO2   25   /* <    TAGO; also MDO[1], PIO[1] */
#define CDE    11   /* NONSGML   delcdata CDATA/SDATA delimiter */
