#include "stdio.h"
#include "f2c.h"
#define PAUSESIG 15

#ifdef KR_headers
#define Void /* void */
#define Int /* int */
#else
#define Void void
#define Int int
#undef abs
#include "stdlib.h"
#include "signal.h"
#ifdef __cplusplus
extern "C" {
#endif
extern int getpid(void), isatty(int), pause(void);
#endif

extern VOID f_exit(Void);

static VOID waitpause(Int n)
{
return;
}

#ifdef KR_headers
int s_paus(s, n) char *s; ftnlen n;
#else
int s_paus(char *s, ftnlen n)
#endif
{
int i;

fprintf(stderr, "PAUSE ");
if(n > 0)
	for(i = 0; i<n ; ++i)
		putc(*s++, stderr);
fprintf(stderr, " statement executed\n");
if( isatty(fileno(stdin)) )
	{
	fprintf(stderr, "To resume execution, type go.  Any other input will terminate job.\n");
	fflush(stderr);
	if( getchar()!='g' || getchar()!='o' || getchar()!='\n' )
		{
		fprintf(stderr, "STOP\n");
		f_exit();
		exit(0);
		}
	}
else
	{
	fprintf(stderr, "To resume execution, execute a   kill -%d %d   command\n",
		PAUSESIG, getpid() );
	signal(PAUSESIG, waitpause);
	fflush(stderr);
	pause();
	}
fprintf(stderr, "Execution resumes after PAUSE.\n");
#ifdef __cplusplus
return 0; /* NOT REACHED */
}
#endif
}
