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
 * This program simply tests writing through the reverse direction of
 * a pipe.  Nothing too fancy, it's only needed because most pipe-using
 * programs never touch the reverse direction (it doesn't exist on
 * Linux.)
 */

int main (void)
{
char buffer[65535], buffer2[65535];
int desc[2];
int buggy, error, i, successes, total;
struct stat status;
pid_t new_pid;

buggy = 0;

error = pipe(desc);

if (error)
	err(0, "Couldn't allocate fds\n");

buffer[0] = 'A';

for (i = 0; i < 65535; i++) {
	buffer[i] = buffer[i - 1] + 1;
	if (buffer[i] > 'Z')
		buffer[i] = 'A';
	}

new_pid = fork();

if (new_pid == 0) {
	error = write(desc[0], &buffer, 4096);
	total += error;
	error = write(desc[0], &buffer[total], 4096);
	total += error;
	error = write(desc[0], &buffer[total], 4096);
	total += error;
	error = write(desc[0], &buffer[total], 4096);
	total += error;
	error = write(desc[0], &buffer[total], 4096);
	total += error;
	error = write(desc[0], &buffer[total], 4096);
	total += error;
	error = write(desc[0], &buffer[total], 4096);
	total += error;
	error = write(desc[0], &buffer[total], 4096);
	total += error;
	printf("Wrote %d bytes, sleeping\n", total);
	usleep(1000000);
	error = write(desc[0], &buffer[total], 4096);
	total += error;
	error = write(desc[0], &buffer[total], 4096);
	total += error;
	printf("Wrote another 8192 bytes, %d total, done\n", total);
} else {
	usleep(500000);
	error = read(desc[1], &buffer2, 32768);
	total += error;
	printf("Read %d bytes, going back to sleep\n", error);
	usleep(1000000);
	error = read(desc[1], &buffer2[total], 8192);
	total += error;
	printf("Read %d bytes, done\n", error);

	for (i = 0; i < total; i++) {
		if (buffer[i] != buffer2[i]) {
			buggy = 1;
			printf("Location %d input: %hhx output: %hhx\n",
					i, buffer[i], buffer2[i]);
		}
	}

if ((buggy == 1) || (total != 40960))
	printf("FAILURE\n");
else
	printf("SUCCESS\n");

}

}
