/*
Copyright (C) 2004 Michael J. Silbersack. All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.
*/

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

/*
 * $FreeBSD$
 * The goal of this program is to see if fstat reports the correct
 * data count for a pipe.  Prior to revision 1.172 of sys_pipe.c,
 * 0 would be returned once the pipe entered direct write mode.
 *
 * Linux (2.6) always returns zero, so it's not a valuable platform
 * for comparison.
 */

int main (void)
{
char buffer[32768], buffer2[32768];
int desc[2];
int error, successes = 0;
struct stat status;
pid_t new_pid;

error = pipe(desc);

if (error)
	err(0, "Couldn't allocate fds\n");

new_pid = fork();

if (new_pid == 0) {
	write(desc[1], &buffer, 145);
	usleep(1000000);
	write(desc[1], &buffer, 2048);
	usleep(1000000);
	write(desc[1], &buffer, 4096);
	usleep(1000000);
	write(desc[1], &buffer, 8191);
	usleep(1000000);
	write(desc[1], &buffer, 8192);
	usleep(1000000);
} else {
	while (successes < 5) {
		usleep(3000);
		fstat(desc[0], &status);
		error = read(desc[0], &buffer2, 32768);
		if (status.st_size != error)
			err(0, "FAILURE: stat size %d read size %d\n", (int)status.st_size, error);
		if (error > 0) {
			printf("SUCCESS at stat size %d read size %d\n", (int)status.st_size, error);
			successes++;
			/* Sleep to avoid the natural race in reading st_size. */
			usleep(1000000);
		}
	}
}

}
