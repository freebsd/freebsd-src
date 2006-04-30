/*-
 * Copyright (C) 2005 IronPort Systems, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Note: it is a good idea to run this against a physical drive to
 * exercise the physio fast path (ie. lio_kqueue /dev/<something safe>)
 * This will ensure op's counting is correct.  It is currently broken.
 *
 * Also note that LIO & kqueue is not implemented in FreeBSD yet, LIO
 * is also broken with respect to op's and some paths.
 *
 * A patch to make this work is at:
 * 	http://www.ambrisko.com/doug/listio_kqueue/listio_kqueue.patch
 */

#include <aio.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>

#define PATH_TEMPLATE   "/tmp/aio.XXXXXXXXXX"

#define LIO_MAX 5
#define MAX LIO_MAX * 16
#define MAX_RUNS 300

main(int argc, char *argv[]){
	int fd;
	struct aiocb *iocb[MAX], *kq_iocb;
	struct aiocb **lio[LIO_MAX], **lio_element, **kq_lio;
	int i, result, run, error, j, k;
	char buffer[32768];
	int kq = kqueue();
	struct kevent ke, kq_returned;
	struct timespec ts;
	struct sigevent sig;
	time_t time1, time2;
	char *file, pathname[sizeof(PATH_TEMPLATE)-1];
	int tmp_file = 0, failed = 0;
	
	if (kq < 0) {
		perror("No kqeueue\n");
		exit(1);
	}

	if (argc == 1) {
		strcpy(pathname, PATH_TEMPLATE);
		fd = mkstemp(pathname);
		file = pathname;
		tmp_file = 1;
	} else {
		file = argv[1];
		fd = open(file, O_RDWR|O_CREAT, 0666);
        }
	if (fd < 0){
		fprintf(stderr, "Can't open %s\n", argv[1]);
		perror("");
		exit(1);
	}

#ifdef DEBUG
	printf("Hello kq %d fd %d\n", kq, fd);
#endif
	
	for (run = 0; run < MAX_RUNS; run++){
#ifdef DEBUG
		printf("Run %d\n", run);
#endif
		for (j = 0; j < LIO_MAX; j++) {
			lio[j] = (struct aiocb **)
				malloc(sizeof(struct aiocb *) * MAX/LIO_MAX);
			for(i = 0; i < MAX / LIO_MAX; i++) {
				k = (MAX / LIO_MAX * j) + i;
				lio_element = lio[j];
				lio[j][i] = iocb[k] = (struct aiocb *)
					malloc(sizeof(struct aiocb));
				bzero(iocb[k], sizeof(struct aiocb));
				iocb[k]->aio_nbytes = sizeof(buffer);
				iocb[k]->aio_buf = buffer;
				iocb[k]->aio_fildes = fd;
				iocb[k]->aio_offset 
				  = iocb[k]->aio_nbytes * k * (run + 1);

#ifdef DEBUG
				printf("hello iocb[k] %d\n",
				       iocb[k]->aio_offset);
#endif
				iocb[k]->aio_lio_opcode = LIO_WRITE;
			}
			sig.sigev_notify_kqueue = kq;
			sig.sigev_value.sigval_ptr = lio[j];
			sig.sigev_notify = SIGEV_KEVENT;
			time(&time1);
			result = lio_listio(LIO_NOWAIT, lio[j],
					    MAX / LIO_MAX, &sig);
			error = errno;
			time(&time2);
#ifdef DEBUG
			printf("Time %d %d %d result -> %d\n", 
			    time1, time2, time2-time1, result);
#endif
			if (result != 0) {
			        errno = error;
				perror("list_listio");
				printf("FAIL: Result %d iteration %d\n",result, j);
				exit(1);
			}
#ifdef DEBUG
			printf("write %d is at %p\n", j, lio[j]);
#endif
		}
		
		for(i = 0; i < LIO_MAX; i++) {
			for(j = LIO_MAX - 1; j >=0; j--) {
				if (lio[j])
					break;
			}
			
			for(;;) {
				bzero(&ke, sizeof(ke));
				bzero(&kq_returned, sizeof(ke));
				ts.tv_sec = 0;
				ts.tv_nsec = 1;
#ifdef DEBUG
				printf("FOO lio %d -> %p\n", j, lio[j]);
#endif
				EV_SET(&ke, (uintptr_t)lio[j], 
				       EVFILT_LIO, EV_ONESHOT, 0, 0, iocb[j]);
				result = kevent(kq, NULL, 0, 
						&kq_returned, 1, &ts);
				error = errno;
				if (result < 0) {
					perror("kevent error: ");
				}
				kq_lio = kq_returned.udata;
#ifdef DEBUG
				printf("kevent %d %d errno %d return.ident %p "
				       "return.data %p return.udata %p %p\n", 
				       i, result, error, 
				       kq_returned.ident, kq_returned.data, 
				       kq_returned.udata, 
				       lio[j]);
#endif
				
				if(kq_lio)
					break;
#ifdef DEBUG
				printf("Try again\n");
#endif
			}			
			
#ifdef DEBUG
			printf("lio %p\n", lio);
#endif
			
			for (j = 0; j < LIO_MAX; j++) {
				if (lio[j] == kq_lio) {
					break;
				}
			}
			if (j == LIO_MAX) {
			  printf("FAIL:\n");
			  exit(1);
			}

#ifdef DEBUG
			printf("Error Result for %d is %d\n", j, result);
#endif
			if (result < 0) {
				printf("FAIL: run %d, operation %d result %d \n", run, LIO_MAX - i -1, result);
				failed = 1;
			} else {
				printf("PASS: run %d, operation %d result %d \n", run, LIO_MAX - i -1, result);
			}
			for(k = 0; k < MAX / LIO_MAX; k++){
				result = aio_return(kq_lio[k]);			
#ifdef DEBUG
				printf("Return Resulto for %d %d is %d\n", j, k, result);
#endif
				if (result != sizeof(buffer)) {
					printf("FAIL: run %d, operation %d sub-opt %d  result %d (errno=%d) should be %d\n",
					   run, LIO_MAX - i -1, k, result, errno, sizeof(buffer));
				} else {
					printf("PASS: run %d, operation %d sub-opt %d  result %d\n",
					   run, LIO_MAX - i -1, k, result);
				}
			}
#ifdef DEBUG
			printf("\n");
#endif
			
			for(k = 0; k < MAX / LIO_MAX; k++) {
				free(lio[j][k]);
			}
			free(lio[j]);
			lio[j] = NULL;
		}	
	}
#ifdef DEBUG
	printf("Done\n");
#endif

	if (tmp_file) {
		unlink(pathname);
	}

	if (failed) {
		printf("FAIL: Atleast one\n");
		exit(1);
	} else {
		printf("PASS: All\n");
		exit(0);
	}
}
