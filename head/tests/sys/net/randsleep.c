#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define RANDOM_MAX ((1<<31) - 1)

int main(int argc, char** argv){
	useconds_t max_usecs, usecs;
	double frac;

	if (argc != 2) {
		printf("Usage: randsleep <max_microseconds>\n");
		exit(2);
	}

	errno = 0;
	max_usecs = (useconds_t)strtol(argv[1], NULL, 0);
	if (errno != 0) {
		perror("strtol");
		exit(1);
	}
	srandomdev();
	frac = (double)random() / (double)RANDOM_MAX;
	usecs = (useconds_t)((double)max_usecs * frac);
	usleep(usecs);

	return (0);
}
