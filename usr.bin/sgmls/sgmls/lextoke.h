/* LEXTOKE.H: Symbols for tokenization lexical classes.
*/
#define INV     0   /* Invalid Chars      Not allowed in an SGML name. */
#define REC     1   /* Record Boundary    RS and RE. */
#define SEP     2   /* Separator          TAB. */
#define SP      3   /* SPACE */
#define NMC     4   /* NAMECHAR  . _      Period, underscore (plus NMS, NUM). */
#define NMS     5   /* NAMESTRT           Lower and uppercase letters */
#define NU      6   /* NUMERAL            Numerals */
#define EOB     7   /* NONCHAR   28       End disk buffer. */
