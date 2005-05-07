/* @(#)printmsg.c	2.1 88/08/11 4.0 RPCSRC */
/* $FreeBSD: src/share/examples/sunrpc/msg/printmsg.c,v 1.3 2000/12/09 09:35:40 obrien Exp $ */
/*
 * printmsg.c: print a message on the console
 */
#include <paths.h>
#include <stdio.h>

main(argc, argv)
	int argc;
	char *argv[];
{
	char *message;

	if (argc < 2) {
		fprintf(stderr, "usage: %s <message>\n", argv[0]);
		exit(1);
	}
	message = argv[1];

	if (!printmessage(message)) {
		fprintf(stderr, "%s: sorry, couldn't print your message\n",
			argv[0]);
		exit(1);
	}
	printf("Message delivered!\n");
}

/*
 * Print a message to the console.
 * Return a boolean indicating whether the message was actually printed.
 */
printmessage(msg)
	char *msg;
{
	FILE *f;

	f = fopen(_PATH_CONSOLE, "w");
	if (f == NULL) {
		return (0);
	}
	fprintf(f, "%s\n", msg);
	fclose(f);
	return(1);
}
