/* This will develop into the inted-like program which
   we may want to use for a server on Win95/NT.  Right now
   it is just a test program ("telnet foo 2401" and you'll
   get a message).  */

#include <winsock.h>
#include <stdio.h>

int
main ()
{
    struct sockaddr_in sa;
    SOCKET t;
    SOCKET s;
    WSADATA data;

    if (WSAStartup (MAKEWORD (1, 1), &data))
    {
	fprintf (stderr, "cvs: unable to initialize winsock\n");
	exit (1);
    }

    t = socket (PF_INET, SOCK_STREAM, 0);
    if (t == INVALID_SOCKET)
    {
	printf ("Error in socket(): %d\n", WSAGetLastError ());
	exit (1);
    }
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = INADDR_ANY;
    sa.sin_port = htons (2401);
    if (bind (t, (struct sockaddr *) &sa, sizeof (sa)) != 0)
    {
	printf ("Cannot bind(): %d\n", WSAGetLastError ());
	exit (1);
    }
    if (listen (t, 1) != 0)
    {
	printf ("Cannot listen(): %d\n", WSAGetLastError ());
	exit (1);
    }
    while (1)
    {
	int sasize = sizeof (sa);
	s = accept (t, (struct sockaddr *) &sa, &sasize);
	if (s == INVALID_SOCKET)
	{
	    printf ("Cannot accept(): %d\n", WSAGetLastError ());
	    exit (1);
	}
	if (send (s, "hello, world\n", 13, 0) == SOCKET_ERROR)
	{
	    /* Note that we do not detect the case in which we sent
	       less than the requested number of bytes.  */
	    printf ("Cannot send(): %d\n", WSAGetLastError ());
	    exit (1);
	}
	if (closesocket (s) != 0)
	{
	    printf ("Cannot closesocket(): %d\n", WSAGetLastError ());
	    exit (1);
	}
    }
    return 0;
}
