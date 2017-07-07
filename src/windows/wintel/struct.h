#include "winsock.h"
#ifdef KRB4
   #include "kstream.h"
#endif
#ifdef KRB5
   #include "k5stream.h"
#endif

#define HCONNECTION HGLOBAL

typedef struct CONNECTION {
	SCREEN *pScreen;     /* handle to screen associated with connection */
	kstream ks;
	SOCKET socket;
	int pnum;	     /* port number associated with connection */
	int telstate;	     /* telnet state for this connection */
	int substate;	     /* telnet subnegotiation state */
	int termsent;
	int echo;
	int ugoahead;
	int igoahead;
	int timing;
	int backspace;
	int ctrl_backspace;
	int termstate;	     /* terminal type for this connection */
	int width;
	int height;
	BOOL bResizeable;
} CONNECTION;
