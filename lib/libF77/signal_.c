#include "f2c.h"

#ifdef KR_headers
typedef VOID (*sig_type)();
extern sig_type signal();
typedef int (*sig_proc)();

ftnint signal_(sigp, proc) integer *sigp; sig_type proc;
#else
#include "signal.h"
typedef void (*sig_type)(int);
typedef int (*sig_proc)(int);

ftnint signal_(integer *sigp, sig_proc proc)
#endif
{
	int sig;
	sig = (int)*sigp;

	return (ftnint)signal(sig, (sig_type)proc);
	}
