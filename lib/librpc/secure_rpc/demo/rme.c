#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = 	"@(#)rme.c	2.3 88/09/09 4.0 RPCSRC";
#endif
/*
 * rme.c: secure identity verifier and reporter: client side
 */
#include <rpc/rpc.h>
#include <stdio.h>
#include "whoami.h"

/*
 * Before running this program, the user must have a key in the publickey
 * database, and must have logged in with a password (or used keylogin).
 * The user's machine and the server's machine must both be running keyserv.
 */

main(argc, argv)
	int argc;
	char *argv[];
{
	CLIENT *cl;
	char *server;
	remote_identity *remote_me;
	name *servername;
	void *nullp;

	if (argc != 2) {
		fprintf(stderr, "usage: %s host\n", argv[0]);
		exit(1);
	}

	/*
	 * Remember what our command line argument refers to
	 */
	server = argv[1];

	/*
	 * Create client "handle" used for calling WHOAMI on the
	 * server designated on the command line. We tell the rpc package
	 * to use the "udp" protocol when contacting the server.
	 */
	cl = clnt_create(server, WHOAMI, WHOAMI_V1, "udp");
	if (cl == NULL) {
		/*
		 * Couldn't establish connection with server.
		 * Print error message and die.
		 */
		clnt_pcreateerror(server);
		exit(1);
	}
    /*
     * Get network identifier for server machine.
     */
    servername = whoami_whoru_1(nullp, cl);
    if (servername == NULL)
    {
        fprintf(stderr, "Trouble communicating with %s\n",
            clnt_sperror(cl, server));
        exit(1);
    }
    else if (*servername[0] == '\0')
    {
        fprintf(stderr, "Could not determine netname of WHOAMI server.\n");
        exit(1);
    }
    printf("Server's netname is: %s\n", *servername);

    /*
     * A wide window and no synchronization is used.  Client and server
     * clock must be with five minutes of each other.
     */
    if ((cl->cl_auth = authdes_create(*servername, 300, NULL, NULL)) == NULL)
    {
        fprintf(stderr, "Could not establish DES credentials of netname %s\n",
            servername);
        exit(1);
    }

    /*
     *  Find out who I am, in the server's point of view.
     */
    remote_me = whoami_iask_1(nullp, cl);
    if (remote_me == NULL)
    {
        fprintf(stderr, "Trouble getting my identity from %s\n",
            clnt_sperror(cl, server));
        exit(1);
    }
    /*
     * Print out my identity.
     */
    printf("My remote user name: %s\n", remote_me->remote_username);
    printf("My remote real name: %s\n", remote_me->remote_realname);

    exit(0);
}
