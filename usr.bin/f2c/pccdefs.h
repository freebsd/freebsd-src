/* The following numbers are strange, and implementation-dependent */

#define P2BAD -1
#define P2NAME 2
#define P2ICON 4		/* Integer constant */
#define P2PLUS 6
#define P2PLUSEQ 7
#define P2MINUS 8
#define P2NEG 10
#define P2STAR 11
#define P2STAREQ 12
#define P2INDIRECT 13
#define P2BITAND 14
#define P2BITOR 17
#define P2BITXOR 19
#define P2QUEST 21
#define P2COLON 22
#define P2ANDAND 23
#define P2OROR 24
#define P2GOTO 37
#define P2LISTOP 56
#define P2ASSIGN 58
#define P2COMOP 59
#define P2SLASH 60
#define P2MOD 62
#define P2LSHIFT 64
#define P2RSHIFT 66
#define P2CALL 70
#define P2CALL0 72

#define P2NOT 76
#define P2BITNOT 77
#define P2EQ 80
#define P2NE 81
#define P2LE 82
#define P2LT 83
#define P2GE 84
#define P2GT 85
#define P2REG 94
#define P2OREG 95
#define P2CONV 104
#define P2FORCE 108
#define P2CBRANCH 109

/* special operators included only for fortran's use */

#define P2PASS 200
#define P2STMT 201
#define P2SWITCH 202
#define P2LBRACKET 203
#define P2RBRACKET 204
#define P2EOF 205
#define P2ARIF 206
#define P2LABEL 207

#define P2SHORT 3
#define P2INT 4
#define P2LONG 4

#define P2CHAR 2
#define P2REAL 6
#define P2DREAL 7
#define P2PTR 020
#define P2FUNCT 040
