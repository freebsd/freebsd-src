/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * Copyright (C) 1993  Hannu Savolainen
 * Ported to 386bsd by Serge Vakulenko
 * based on tools/build.c by Linus Torvalds
 * $Id$
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <a.out.h>
#include <string.h>

int
main(int argc, char **argv)
{
	pid_t Pext, Pgzip, Ppiggy, Pld;
	int pipe1[2], pipe2[2];
	int status,fdi,fdo;
	char obj[BUFSIZ];
	char out[BUFSIZ];

	if(argc != 2) {
		fprintf(stderr,"usage:\n\t%s kernel\n",argv[0]);
		return 2;
	}

	strcpy(obj,argv[1]); strcat(obj,".o");
	strcpy(out,argv[1]); strcat(out,".kz");

	fdi = open(argv[1],O_RDONLY);
	if(fdi<0) {
		perror(argv[1]);
		return 2;
	}
	fdo = open(obj,O_WRONLY|O_TRUNC|O_CREAT,0666);
	if(fdo<0) {
		perror(obj);
		return 2;
	}
	
	if (pipe(pipe1) < 0) { perror("pipe()"); return 1; }

	if (pipe(pipe2) < 0) { perror("pipe()"); return 1; }

	Pext = fork();
	if (Pext < 0) { perror("fork()"); return 1; }
	if (!Pext) {
		dup2(fdi,0);
		dup2(pipe1[1],1);
		close(pipe1[0]); close(pipe1[1]);
		close(pipe2[0]); close(pipe2[1]);
		close(fdi); close(fdo);
		extract(argv[1]);
		exit(0);
	}

	Pgzip = fork();
	if (Pgzip < 0) { perror("fork()"); return 1; }
	if (!Pgzip) {
		dup2(pipe1[0],0);
		dup2(pipe2[1],1);
		close(pipe1[0]); close(pipe1[1]);
		close(pipe2[0]); close(pipe2[1]);
		close(fdi); close(fdo);
		execlp("gzip", "gzip", "-9", "-n", 0);
		exit (0);
	}

	Ppiggy = fork();
	if (Ppiggy < 0) { perror("fork()"); return 1; }
	if (!Ppiggy) {
		dup2(pipe2[0],0);
		dup2(fdo,1);
		close(pipe1[0]); close(pipe1[1]);
		close(pipe2[0]); close(pipe2[1]);
		close(fdi); close(fdo);
		piggyback(obj);
		exit(0);
	}

	close(pipe1[0]); close(pipe1[1]);
	close(pipe2[0]); close(pipe2[1]);
	close(fdi); close(fdo);

	if (waitpid(Pext, &status,0) < 0) 
		{ perror("waitpid(Pextract)"); return 1; }

	if(status) {
		fprintf(stderr,"extract returned %x\n",status);
		return 3;
	}

	if (waitpid(Pgzip, &status,0) < 0) 
		{ perror("waitpid(Pgzip)"); return 1; }
	
	if(status) {
		fprintf(stderr,"gzip returned %x\n",status);
		return 3;
	}

	if (waitpid(Ppiggy, &status,0) < 0) 
		{ perror("waitpid(Ppiggy)"); return 1; }

	if(status) {
		fprintf(stderr,"piggyback returned %x\n",status);
		return 3;
	}

	Pld = fork();
	if (Pld < 0) { perror("fork()"); return 1; }
	if (!Pld) {
		execlp("ld",
			"ld",
			"-Bstatic",
			"-Z",
			"-T",
			KZBASE,
			"-o",
			out,
			"/usr/lib/kzip.o",
			obj,
			0);
		exit(2);
	}

	if (waitpid(Pld, &status,0) < 0) 
		{ perror("waitpid(Pld)"); return 1; }

	if(status) {
		fprintf(stderr,"ld returned %x\n",status);
		return 3;
	}

	unlink(obj);
	exit(0);
}

int 
extract (char *file)
{
	int sz;
	char buf[BUFSIZ];
	struct exec hdr;

	if (read (0, (char *)&hdr, sizeof(hdr)) != sizeof(hdr)) {
		perror(file);
		exit(2);
	}
	if (hdr.a_magic != ZMAGIC) {
		fprintf(stderr,"Bad magic in file %s, probably not a kernel\n",
			file);
		exit(2);
	}
	if (lseek (0, N_TXTOFF(hdr), 0) < 0) {
		perror(file);
		exit(2);
	}

	sz = N_SYMOFF (hdr) - N_TXTOFF (hdr);

	while (sz) {
		int l, n;

		l = sz;
		if (l > sizeof(buf))
			l = sizeof(buf);

		n = read (0, buf, l);
		if (n != l) {
			if (n == -1) 
				perror (file);
			else
				fprintf (stderr, "Unexpected EOF\n");

			exit(1);
		}

		write (1, buf, l);
		sz -= l;
	}
	exit(0);
}


char string_names[] = {"_input_data\0_input_len\0"};

struct nlist var_names[2] = {                           /* Symbol table */
	{ { (char*)  4 }, N_EXT|N_TEXT, 0, 0, 0 },      /* _input_data  */
	{ { (char*) 16 }, N_EXT|N_TEXT, 0, 0, 0 },      /* _input_len */
};

int 
piggyback(char *file)
{
	int n, len;
	struct exec hdr;                        /* object header */
	char image[1024*1024];                   /* kernel image buffer */

	len = 0;
	while ((n = read (0, &image[len], sizeof(image)-len+1)) > 0)
	      len += n;

	if (n < 0) {
		perror ("stdin");
		exit (1);
	}

	if (len >= sizeof(image)) {
		fprintf (stderr,"Input too large\n");
		exit (1);
	}

	/*
	 *      Output object header
	 */
	memset(&hdr,0,sizeof hdr);
	hdr.a_magic = OMAGIC;
	hdr.a_text = len + sizeof(long);
	hdr.a_syms = sizeof(var_names);
	write (1, (char *)&hdr, sizeof(hdr));

	/*
	 *      Output text segment (compressed system & len)
	 */
	write (1, image, len);
	write (1, (char *)&len, sizeof(len));

	/*
	 *      Output symbol table
	 */
	var_names[1].n_value = len;
	write (1, (char *)&var_names, sizeof(var_names));

	/*
	 *      Output string table
	 */
	len = sizeof(string_names) + sizeof(len);
	write (1, (char *)&len, sizeof(len));
	write (1, string_names, sizeof(string_names));

	return (0);
}
