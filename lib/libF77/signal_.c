#include "f2c.h"

#ifdef KR_headers
typedef VOID (*sig_pf)();
extern sig_pf signal();
#define signal1 signal

ftnint signal_(sigp, proc) integer *sigp; sig_pf proc;
#else
#include "signal1.h"

ftnint signal_(integer *sigp, sig_pf proc)
#endif
{
	int sig;
	sig = (int)*sigp;

	return (ftnint)signal(sig, proc);
	}
