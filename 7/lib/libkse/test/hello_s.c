/****************************************************************************
 *
 * Simple sequence mode test.
 *
 * $FreeBSD$
 *
 ****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <pthread.h>

void *
entry(void * a_arg)
{
	fprintf(stderr, "ok 1\n");
	fprintf(stderr, "ok \n");
	fprintf(stderr, "ok 3\n");

	return NULL;
}

int
main()
{
	pthread_t thread;
	int error;

	fprintf(stderr, "1..3\n");
	
	fprintf(stderr, "Some random text\n");
	
	error = pthread_create(&thread, NULL, entry, NULL);
	fprintf(stderr, "More unimportant text\n");
	if (error)
		fprintf(stderr,"Error in pthread_create(): %s\n",
			strerror(error));

	error = pthread_join(thread, NULL);
	if (error)
		fprintf(stderr,	"Error in pthread_join(): %s\n",
			strerror(error));

	fprintf(stderr, "Hello world\n");

	return 0;
}
