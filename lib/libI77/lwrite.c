#include "f2c.h"
#include "fio.h"
#include "fmt.h"
#include "lio.h"
ftnint L_len;

#ifdef KR_headers
t_putc(c)
#else
t_putc(int c)
#endif
{
	f__recpos++;
	putc(c,f__cf);
	return(0);
}
 static VOID
#ifdef KR_headers
lwrt_I(n) long n;
#else
lwrt_I(long n)
#endif
{
	char buf[LINTW],*p;
#ifdef USE_STRLEN
	(void) sprintf(buf," %ld",n);
	if(f__recpos+strlen(buf)>=L_len)
#else
	if(f__recpos + sprintf(buf," %ld",n) >= L_len)
#endif
		(*f__donewrec)();
	for(p=buf;*p;PUT(*p++));
}
 static VOID
#ifdef KR_headers
lwrt_L(n, len) ftnint n; ftnlen len;
#else
lwrt_L(ftnint n, ftnlen len)
#endif
{
	if(f__recpos+LLOGW>=L_len)
		(*f__donewrec)();
	wrt_L((Uint *)&n,LLOGW, len);
}
 static VOID
#ifdef KR_headers
lwrt_A(p,len) char *p; ftnlen len;
#else
lwrt_A(char *p, ftnlen len)
#endif
{
	int i;
	if(f__recpos+len>=L_len)
		(*f__donewrec)();
	if (!f__recpos)
		{ PUT(' '); ++f__recpos; }
	for(i=0;i<len;i++) PUT(*p++);
}

 static int
#ifdef KR_headers
l_g(buf, n) char *buf; double n;
#else
l_g(char *buf, double n)
#endif
{
#ifdef Old_list_output
	doublereal absn;
	char *fmt;

	absn = n;
	if (absn < 0)
		absn = -absn;
	fmt = LLOW <= absn && absn < LHIGH ? LFFMT : LEFMT;
#ifdef USE_STRLEN
	sprintf(buf, fmt, n);
	return strlen(buf);
#else
	return sprintf(buf, fmt, n);
#endif

#else
	register char *b, c, c1;

	b = buf;
	*b++ = ' ';
	if (n < 0) {
		*b++ = '-';
		n = -n;
		}
	else
		*b++ = ' ';
	if (n == 0) {
		*b++ = '0';
		*b++ = '.';
		*b = 0;
		goto f__ret;
		}
	sprintf(b, LGFMT, n);
	if (*b == '0') {
		while(b[0] = b[1])
			b++;
		}
	/* Fortran 77 insists on having a decimal point... */
	else for(;; b++)
		switch(*b) {
			case 0:
				*b++ = '.';
				*b = 0;
				goto f__ret;
			case '.':
				while(*++b);
				goto f__ret;
			case 'E':
				for(c1 = '.', c = 'E';  *b = c1;
					c1 = c, c = *++b);
				goto f__ret;
			}
 f__ret:
	return b - buf;
#endif
	}

 static VOID
#ifdef KR_headers
l_put(s) register char *s;
#else
l_put(register char *s)
#endif
{
#ifdef KR_headers
	register int c, (*pn)() = f__putn;
#else
	register int c, (*pn)(int) = f__putn;
#endif
	while(c = *s++)
		(*pn)(c);
	}

 static VOID
#ifdef KR_headers
lwrt_F(n) double n;
#else
lwrt_F(double n)
#endif
{
	char buf[LEFBL];

	if(f__recpos + l_g(buf,n) >= L_len)
		(*f__donewrec)();
	l_put(buf);
}
 static VOID
#ifdef KR_headers
lwrt_C(a,b) double a,b;
#else
lwrt_C(double a, double b)
#endif
{
	char *ba, *bb, bufa[LEFBL], bufb[LEFBL];
	int al, bl;

	al = l_g(bufa, a);
	for(ba = bufa; *ba == ' '; ba++)
		--al;
	bl = l_g(bufb, b) + 1;	/* intentionally high by 1 */
	for(bb = bufb; *bb == ' '; bb++)
		--bl;
	if(f__recpos + al + bl + 3 >= L_len && f__recpos)
		(*f__donewrec)();
	PUT(' ');
	PUT('(');
	l_put(ba);
	PUT(',');
	if (f__recpos + bl >= L_len) {
		(*f__donewrec)();
		PUT(' ');
		}
	l_put(bb);
	PUT(')');
}
#ifdef KR_headers
l_write(number,ptr,len,type) ftnint *number,type; char *ptr; ftnlen len;
#else
l_write(ftnint *number, char *ptr, ftnlen len, ftnint type)
#endif
{
#define Ptr ((flex *)ptr)
	int i;
	long x;
	double y,z;
	real *xx;
	doublereal *yy;
	for(i=0;i< *number; i++)
	{
		switch((int)type)
		{
		default: f__fatal(204,"unknown type in lio");
		case TYINT1:
			x = Ptr->flchar;
			goto xint;
		case TYSHORT:
			x=Ptr->flshort;
			goto xint;
#ifdef TYQUAD
		case TYQUAD:
			x = Ptr->fllongint;
			goto xint;
#endif
		case TYLONG:
			x=Ptr->flint;
		xint:	lwrt_I(x);
			break;
		case TYREAL:
			y=Ptr->flreal;
			goto xfloat;
		case TYDREAL:
			y=Ptr->fldouble;
		xfloat: lwrt_F(y);
			break;
		case TYCOMPLEX:
			xx= &Ptr->flreal;
			y = *xx++;
			z = *xx;
			goto xcomplex;
		case TYDCOMPLEX:
			yy = &Ptr->fldouble;
			y= *yy++;
			z = *yy;
		xcomplex:
			lwrt_C(y,z);
			break;
		case TYLOGICAL1:
			x = Ptr->flchar;
			goto xlog;
		case TYLOGICAL2:
			x = Ptr->flshort;
			goto xlog;
		case TYLOGICAL:
			x = Ptr->flint;
		xlog:	lwrt_L(Ptr->flint, len);
			break;
		case TYCHAR:
			lwrt_A(ptr,len);
			break;
		}
		ptr += len;
	}
	return(0);
}
