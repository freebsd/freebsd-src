#include "f2c.h"

#ifdef KR_headers
typedef int (*sig_type)();
extern sig_type signal();

ftnint signal_(sigp, proc) integer *sigp; sig_type proc;
#else
#include "signal.h"
typedef void (*sig_type)(int);

ftnint signal_(integer *sigp, sig_type proc)
#endif
{
	int sig;
	sig = (int)*sigp;

	return (ftnint)signal(sig, proc);
	}
