#include "f2c.h"
#include "signal1.h"

 ftnint
#ifdef KR_headers
signal_(sigp, proc) integer *sigp; sig_pf proc;
#else
signal_(integer *sigp, sig_pf proc)
#endif
{
	int sig;
	sig = (int)*sigp;

	return (ftnint)signal(sig, proc);
	}
