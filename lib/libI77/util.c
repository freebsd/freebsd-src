#ifndef NON_UNIX_STDIO
#include "sys/types.h"
#include "sys/stat.h"
#endif
#include "f2c.h"
#include "fio.h"

 VOID
#ifdef KR_headers
g_char(a,alen,b) char *a,*b; ftnlen alen;
#else
g_char(char *a, ftnlen alen, char *b)
#endif
{
	char *x = a + alen, *y = b + alen;

	for(;; y--) {
		if (x <= a) {
			*b = 0;
			return;
			}
		if (*--x != ' ')
			break;
		}
	*y-- = 0;
	do *y-- = *x;
		while(x-- > a);
	}

 VOID
#ifdef KR_headers
b_char(a,b,blen) char *a,*b; ftnlen blen;
#else
b_char(char *a, char *b, ftnlen blen)
#endif
{	int i;
	for(i=0;i<blen && *a!=0;i++) *b++= *a++;
	for(;i<blen;i++) *b++=' ';
}
#ifndef NON_UNIX_STDIO
#ifdef KR_headers
long f__inode(a, dev) char *a; int *dev;
#else
long f__inode(char *a, int *dev)
#endif
{	struct stat x;
	if(stat(a,&x)<0) return(-1);
	*dev = x.st_dev;
	return(x.st_ino);
}
#endif

#define INTBOUND sizeof(int)-1
 VOID
#ifdef KR_headers
f__mvgbt(n,len,a,b) char *a,*b;
#else
f__mvgbt(int n, int len, char *a, char *b)
#endif
{	register int num=n*len;
	if( ((int)a&INTBOUND)==0 && ((int)b&INTBOUND)==0 && (num&INTBOUND)==0 )
	{	register int *x=(int *)a,*y=(int *)b;
		num /= sizeof(int);
		if(x>y) for(;num>0;num--) *y++= *x++;
		else for(num--;num>=0;num--) *(y+num)= *(x+num);
	}
	else
	{	register char *x=a,*y=b;
		if(x>y) for(;num>0;num--) *y++= *x++;
		else for(num--;num>=0;num--) *(y+num)= *(x+num);
	}
}
