#define NOT 0
#define ISBLOCK 1
#define ISCHAR 2
#define ISDIR 3
#define ISEXIST 4
#define ISFILE 5
#define ISSETGID 6
#define ISSTICKY 7
#define STRLEN 8
#define ISFIFO 9
#define ISREAD 10
#define ISSIZE 11
#define ISTTY 12
#define ISSETUID 13
#define ISWRITE 14
#define ISEXEC 15
#define NULSTR 16
#define OR1 17
#define OR2 18
#define AND1 19
#define AND2 20
#define STREQ 21
#define STRNE 22
#define EQ 23
#define NE 24
#define GT 25
#define LT 26
#define LE 27
#define GE 28

#define FIRST_BINARY_OP 17

#define OP_INT 1		/* arguments to operator are integer */
#define OP_STRING 2		/* arguments to operator are string */
#define OP_FILE 3		/* argument is a file name */

extern char *const unary_op[];
extern char *const binary_op[];
extern char *const andor_op[];
extern const char op_priority[];
extern const char op_argflag[];
