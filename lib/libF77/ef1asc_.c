/* EFL support routine to copy string b to string a */

#include "f2c.h"


#define M	( (long) (sizeof(long) - 1) )
#define EVEN(x)	( ( (x)+ M) & (~M) )

#ifdef KR_headers
extern VOID s_copy();
ef1asc_(a, la, b, lb) ftnint *a, *b; ftnlen *la, *lb;
#else
extern void s_copy(char*,char*,ftnlen,ftnlen);
int ef1asc_(ftnint *a, ftnlen *la, ftnint *b, ftnlen *lb)
#endif
{
s_copy( (char *)a, (char *)b, EVEN(*la), *lb );
#ifdef __cplusplus
return 0;
#endif
}
