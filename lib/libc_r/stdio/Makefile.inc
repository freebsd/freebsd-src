#	@(#)Makefile.inc	8.3 (Berkeley) 4/17/94

# stdio sources
.PATH: ${.CURDIR}/../libc/stdio

SRCS+=	clrerr.c fclose.c fdopen.c feof.c ferror.c fflush.c fgetc.c \
	fgetln.c fgetpos.c fgets.c fileno.c findfp.c flags.c fopen.c \
	fprintf.c fpurge.c fputc.c fputs.c fread.c freopen.c fscanf.c \
	fseek.c fsetpos.c ftell.c funopen.c fvwrite.c fwalk.c fwrite.c \
	getc.c getchar.c gets.c getw.c makebuf.c mktemp.c perror.c \
	printf.c putc.c putchar.c puts.c putw.c refill.c remove.c rewind.c \
	rget.c scanf.c setbuf.c setbuffer.c setvbuf.c snprintf.c sprintf.c \
	sscanf.c stdio.c tempnam.c tmpfile.c tmpnam.c ungetc.c vfprintf.c \
	vfscanf.c vprintf.c vscanf.c vsnprintf.c vsprintf.c vsscanf.c \
	wbuf.c wsetup.c
