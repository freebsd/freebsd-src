#include "ctm.h"

/*---------------------------------------------------------------------------*/
char *
String(char *s)
{
    char *p = malloc(strlen(s) + 1);
    strcpy(p,s);
    return p;
}
/*---------------------------------------------------------------------------*/
void
Fatal_(int ln, char *fn, char *kind)
{
    if(Verbose > 2)
	fprintf(stderr,"Fatal error. (%s:%d)\n",fn,ln);
    fprintf(stderr,"%s Fatal error: %s\n",FileName, kind);
}
#define Fatal(foo) Fatal_(__LINE__,__FILE__,foo)
#define Assert() Fatal_(__LINE__,__FILE__,"Assert failed.")

/*---------------------------------------------------------------------------*/
/* get next field, check that the terminating whitespace is what we expect */
u_char *
Ffield(FILE *fd, MD5_CTX *ctx,u_char term)
{
    static u_char buf[BUFSIZ];
    int i,l;

    for(l=0;;) {
	if((i=getc(fd)) == EOF) {
	    Fatal("Truncated patch.");
	    return 0;
	}
	buf[l++] = i;
	if(isspace(i))
	    break;
	if(l >= sizeof buf) {
	    Fatal("Corrupt patch.");
	    printf("Token is too long.\n");
	    return 0;
	}
    }
    buf[l] = '\0';
    MD5Update(ctx,buf,l);
    if(buf[l-1] != term) {
        Fatal("Corrupt patch.");
	fprintf(stderr,"Expected \"%s\" but didn't find it.\n",
	    term == '\n' ? "\\n" : " ");
	return 0;
    }
    buf[--l] = '\0';
    return buf;
}

int
Fbytecnt(FILE *fd, MD5_CTX *ctx, u_char term)
{
    u_char *p,*q;
    int u_chars;

    p = Ffield(fd,ctx,term);
    if(!p) return -1;
    for(q=p;*q;q++)
	if(!isdigit(*q)) {
	    Fatal("Bytecount contains non-digit.");
	    return -1;
	}
    u_chars=atoi(p);
    if(u_chars > MAXSIZE) {
	Fatal("Bytecount too large.");
	return -1;
    }
    return u_chars;
}

u_char *
Fdata(FILE *fd, int u_chars, MD5_CTX *ctx)
{
    u_char *p = Malloc(u_chars+1);

    if(u_chars+1 != fread(p,1,u_chars+1,fd)) {
	Fatal("Truncated patch.");
	return 0;
    }
    MD5Update(ctx,p,u_chars+1);
    if(p[u_chars] != '\n') {
	if(Verbose > 3)
	    printf("FileData wasn't followed by a newline.\n");
        Fatal("Corrupt patch.");
	return 0;
    }
    p[u_chars] = '\0';
    return p;
}
