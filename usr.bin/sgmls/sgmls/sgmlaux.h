/* This file controls the interface between the parser core and the auxiliary
functions in entgen.c, sgmlio.c, and sgmlmsg.c */

#include "std.h"
#include "entity.h"
#include "sgmldecl.h"

/* Error types (ERRTYPE) for calls to error-handling services
   performed for SGML by the text processor (SGMLIO).
   NOTE: Strings in these blocks have no lengths, but cannot exceed
   NAMELEN (plus 1 more byte for the zero terminator).
*/
#define FILERR    0           /* Error: file access. */
#define DOCERR    1           /* Error: in document markup. */
#define MDERR     2           /* Error: in markup declaration with subdcl. */
#define MDERR2    3           /* Error: in markup declaration with no subdcl. */
#define EXITERR   4           /* Error: terminal error in document markup. */
/* Quantities affecting error messages and their arguments.
*/
#define MAXARGS    2          /* Maximum number of arguments in a msg. */

/* NOTE: Error handler must return, or next call to SGML must be RSET or END,
         except for EXITERR errors which must not return.
*/
struct error {                /* IPB for error messages. */
     UNS errtype;             /* Type of error: DOC, MD, MD2, FIL. */
     UNS errnum;              /* Error number. */
     UNS errsp;               /* Special parameter index in message file. */
     int sverrno;	      /* Saved value of errno. */
     int parmno;              /* MDERROR: declaration parameter number. */
     UNCH *subdcl;	      /* MDERROR: subject of declaration. */
     UNIV eparm[MAXARGS];     /* Ptrs to arguments (no length, but EOS). */
};

struct location {
     int filesw;
     unsigned long rcnt;
     int ccnt;
     UNCH curchar;
     UNCH nextchar;
     UNCH *ename;
     UNIV fcb;
};

int ioopen P((UNIV, UNIV*));
VOID ioclose P((UNIV));
int ioread P((UNIV, UNCH *, int *));
VOID iopend P((UNIV, int, UNCH *));
int iocont P((UNIV));
VOID ioinit P((struct switches *));
char *ioflid P((UNIV));

UNIV entgen P((struct fpi *));

VOID msgprint P((struct error *));
VOID msginit P((struct switches *));
UNIV msgsave P((struct error *));
VOID msgsprint P((UNIV));
VOID msgsfree P((UNIV));
int msgcnterr P((void));


int inprolog P((void));
UNCH *getgi P((int));

int getlocation P((int, struct location *));
UNIV rmalloc P((unsigned int));
UNIV rrealloc P((UNIV, UNS));
VOID frem P((UNIV));
VOID exiterr P((unsigned int,struct parse *));
