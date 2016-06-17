#ifndef _I386_STRING_I486_H_
#define _I386_STRING_I486_H_

/*
 * This string-include defines all string functions as inline
 * functions. Use gcc. It also assumes ds=es=data space, this should be
 * normal. Most of the string-functions are rather heavily hand-optimized,
 * see especially strtok,strstr,str[c]spn. They should work, but are not
 * very easy to understand. Everything is done entirely within the register
 * set, making the functions fast and clean. 
 *
 *		Copyright (C) 1991, 1992 Linus Torvalds
 *		Revised and optimized for i486/pentium
 *		1994/03/15 by Alberto Vignani/Davide Parodi @crf.it
 *
 *	Split into 2 CPU specific files by Alan Cox to keep #ifdef noise down.
 *
 *	1999/10/5	Proper register args for newer GCCs and minor bugs
 *			fixed - Petko Manolov (petkan@spct.net)
 *	1999/10/14	3DNow memscpy() added - Petkan
 *	2000/05/09	extern changed to static in function definitions
 *			and a few cleanups - Petkan
 */

#define __HAVE_ARCH_STRCPY
static inline char * strcpy(char * dest,const char *src)
{
register char *tmp= (char *)dest;
register char dummy;
__asm__ __volatile__(
	"\n1:\t"
	"movb (%0),%2\n\t"
	"incl %0\n\t"
	"movb %2,(%1)\n\t"
	"incl %1\n\t"
	"testb %2,%2\n\t"
	"jne 1b"
	:"=r" (src), "=r" (tmp), "=q" (dummy)
	:"0" (src), "1" (tmp)
	:"memory");
return dest;
}

#define __HAVE_ARCH_STRNCPY
static inline char * strncpy(char * dest,const char *src,size_t count)
{
register char *tmp= (char *)dest;
register char dummy;
if (count) {
__asm__ __volatile__(
	"\n1:\t"
	"movb (%0),%2\n\t"
	"incl %0\n\t"
	"movb %2,(%1)\n\t"
	"incl %1\n\t"
	"decl %3\n\t"
	"je 3f\n\t"
	"testb %2,%2\n\t"
	"jne 1b\n\t"
	"2:\tmovb %2,(%1)\n\t"
	"incl %1\n\t"
	"decl %3\n\t"
	"jne 2b\n\t"
	"3:"
	:"=r" (src), "=r" (tmp), "=q" (dummy), "=r" (count)
	:"0" (src), "1" (tmp), "3" (count)
	:"memory");
    } /* if (count) */
return dest;
}

#define __HAVE_ARCH_STRCAT
static inline char * strcat(char * dest,const char * src)
{
register char *tmp = (char *)(dest-1);
register char dummy;
__asm__ __volatile__(
	"\n1:\tincl %1\n\t"
	"cmpb $0,(%1)\n\t"
	"jne 1b\n"
	"2:\tmovb (%2),%b0\n\t"
	"incl %2\n\t"
	"movb %b0,(%1)\n\t"
	"incl %1\n\t"
	"testb %b0,%b0\n\t"
	"jne 2b\n"
	:"=q" (dummy), "=r" (tmp), "=r" (src)
	:"1"  (tmp), "2"  (src)
	:"memory");
return dest;
}

#define __HAVE_ARCH_STRNCAT
static inline char * strncat(char * dest,const char * src,size_t count)
{
register char *tmp = (char *)(dest-1);
register char dummy;
__asm__ __volatile__(
	"\n1:\tincl %1\n\t"
	"cmpb $0,(%1)\n\t"
	"jne 1b\n"
	"2:\tdecl %3\n\t"
	"js 3f\n\t"
	"movb (%2),%b0\n\t"
	"incl %2\n\t"
	"movb %b0,(%1)\n\t"
	"incl %1\n\t"
	"testb %b0,%b0\n\t"
	"jne 2b\n"
	"3:\txorb %0,%0\n\t"
	"movb %b0,(%1)\n\t"
	:"=q" (dummy), "=r" (tmp), "=r" (src), "=r" (count)
	:"1"  (tmp), "2"  (src), "3"  (count)
	:"memory");
return dest;
}

#define __HAVE_ARCH_STRCMP
static inline int strcmp(const char * cs,const char * ct)
{
register int __res;
__asm__ __volatile__(
	"\n1:\tmovb (%1),%b0\n\t"
	"incl %1\n\t"
	"cmpb %b0,(%2)\n\t"
	"jne 2f\n\t"
	"incl %2\n\t"
	"testb %b0,%b0\n\t"
	"jne 1b\n\t"
	"xorl %0,%0\n\t"
	"jmp 3f\n"
	"2:\tmovl $1,%0\n\t"
	"jb 3f\n\t"
	"negl %0\n"
	"3:"
	:"=q" (__res), "=r" (cs), "=r" (ct)
	:"1" (cs), "2" (ct)
	: "memory" );
return __res;
}

#define __HAVE_ARCH_STRNCMP
static inline int strncmp(const char * cs,const char * ct,size_t count)
{
register int __res;
__asm__ __volatile__(
	"\n1:\tdecl %3\n\t"
	"js 2f\n\t"
	"movb (%1),%b0\n\t"
	"incl %1\n\t"
	"cmpb %b0,(%2)\n\t"
	"jne 3f\n\t"
	"incl %2\n\t"
	"testb %b0,%b0\n\t"
	"jne 1b\n"
	"2:\txorl %0,%0\n\t"
	"jmp 4f\n"
	"3:\tmovl $1,%0\n\t"
	"jb 4f\n\t"
	"negl %0\n"
	"4:"
	:"=q" (__res), "=r" (cs), "=r" (ct), "=r" (count)
	:"1"  (cs), "2"  (ct),  "3" (count));
return __res;
}

#define __HAVE_ARCH_STRCHR
static inline char * strchr(const char * s, int c)
{
register char * __res;
__asm__ __volatile__(
	"movb %%al,%%ah\n"
	"1:\tmovb (%1),%%al\n\t"
	"cmpb %%ah,%%al\n\t"
	"je 2f\n\t"
	"incl %1\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b\n\t"
	"xorl %1,%1\n"
	"2:\tmovl %1,%0\n\t"
	:"=a" (__res), "=r" (s)
	:"0" (c),      "1"  (s));
return __res;
}

#define __HAVE_ARCH_STRRCHR
static inline char * strrchr(const char * s, int c)
{
int	d0, d1;
register char * __res;
__asm__ __volatile__(
	"movb %%al,%%ah\n"
	"1:\tlodsb\n\t"
	"cmpb %%ah,%%al\n\t"
	"jne 2f\n\t"
	"leal -1(%%esi),%0\n"
	"2:\ttestb %%al,%%al\n\t"
	"jne 1b"
	:"=d" (__res), "=&S" (d0), "=&a" (d1)
	:"0" (0), "1" (s), "2" (c));
return __res;
}


#define __HAVE_ARCH_STRCSPN
static inline size_t strcspn(const char * cs, const char * ct)
{
int	d0, d1;
register char * __res;
__asm__ __volatile__(
	"movl %6,%%edi\n\t"
	"repne\n\t"
	"scasb\n\t"
	"notl %%ecx\n\t"
	"decl %%ecx\n\t"
	"movl %%ecx,%%edx\n"
	"1:\tlodsb\n\t"
	"testb %%al,%%al\n\t"
	"je 2f\n\t"
	"movl %6,%%edi\n\t"
	"movl %%edx,%%ecx\n\t"
	"repne\n\t"
	"scasb\n\t"
	"jne 1b\n"
	"2:\tdecl %0"
	:"=S" (__res), "=&a" (d0), "=&c" (d1)
	:"0" (cs), "1" (0), "2" (0xffffffff), "g" (ct)
	:"dx", "di");
return __res-cs;
}


#define __HAVE_ARCH_STRLEN
static inline size_t strlen(const char * s)
{
/*
 * slightly slower on a 486, but with better chances of
 * register allocation
 */
register char dummy, *tmp= (char *)s;
__asm__ __volatile__(
	"\n1:\t"
	"movb\t(%0),%1\n\t"
	"incl\t%0\n\t"
	"testb\t%1,%1\n\t"
	"jne\t1b"
	:"=r" (tmp),"=q" (dummy)
	:"0" (s)
	: "memory" );
return (tmp-s-1);
}

/* Added by Gertjan van Wingerde to make minix and sysv module work */
#define __HAVE_ARCH_STRNLEN
static inline size_t strnlen(const char * s, size_t count)
{
int	d0;
register int __res;
__asm__ __volatile__(
	"movl %3,%0\n\t"
	"jmp 2f\n"
	"1:\tcmpb $0,(%0)\n\t"
	"je 3f\n\t"
	"incl %0\n"
	"2:\tdecl %2\n\t"
	"cmpl $-1,%2\n\t"
	"jne 1b\n"
	"3:\tsubl %3,%0"
	:"=a" (__res), "=&d" (d0)
	:"1" (count), "c" (s));
return __res;
}
/* end of additional stuff */


/*
 *	These ought to get tweaked to do some cache priming.
 */
 
static inline void * __memcpy_by4(void * to, const void * from, size_t n)
{
register void *tmp = (void *)to;
register int dummy1,dummy2;
__asm__ __volatile__ (
	"\n1:\tmovl (%2),%0\n\t"
	"addl $4,%2\n\t"
	"movl %0,(%1)\n\t"
	"addl $4,%1\n\t"
	"decl %3\n\t"
	"jnz 1b"
	:"=r" (dummy1), "=r" (tmp), "=r" (from), "=r" (dummy2) 
	:"1" (tmp), "2" (from), "3" (n/4)
	:"memory");
return (to);
}

static inline void * __memcpy_by2(void * to, const void * from, size_t n)
{
register void *tmp = (void *)to;
register int dummy1,dummy2;
__asm__ __volatile__ (
	"shrl $1,%3\n\t"
	"jz 2f\n"                 /* only a word */
	"1:\tmovl (%2),%0\n\t"
	"addl $4,%2\n\t"
	"movl %0,(%1)\n\t"
	"addl $4,%1\n\t"
	"decl %3\n\t"
	"jnz 1b\n"
	"2:\tmovw (%2),%w0\n\t"
	"movw %w0,(%1)"
	:"=r" (dummy1), "=r" (tmp), "=r" (from), "=r" (dummy2) 
	:"1" (tmp), "2" (from), "3" (n/2)
	:"memory");
return (to);
}

static inline void * __memcpy_g(void * to, const void * from, size_t n)
{
int	d0, d1, d2;
register void *tmp = (void *)to;
__asm__ __volatile__ (
	"shrl $1,%%ecx\n\t"
	"jnc 1f\n\t"
	"movsb\n"
	"1:\tshrl $1,%%ecx\n\t"
	"jnc 2f\n\t"
	"movsw\n"
	"2:\trep\n\t"
	"movsl"
	:"=&c" (d0), "=&D" (d1), "=&S" (d2)
	:"0" (n), "1" ((long) tmp), "2" ((long) from)
	:"memory");
return (to);
}

#define __memcpy_c(d,s,count) \
((count%4==0) ? \
 __memcpy_by4((d),(s),(count)) : \
 ((count%2==0) ? \
  __memcpy_by2((d),(s),(count)) : \
  __memcpy_g((d),(s),(count))))
  
#define __memcpy(d,s,count) \
(__builtin_constant_p(count) ? \
 __memcpy_c((d),(s),(count)) : \
 __memcpy_g((d),(s),(count)))
 
#define __HAVE_ARCH_MEMCPY

#include <linux/config.h>

#ifdef CONFIG_X86_USE_3DNOW

#include <asm/mmx.h>

/*
**      This CPU favours 3DNow strongly (eg AMD K6-II, K6-III, Athlon)
*/

static inline void * __constant_memcpy3d(void * to, const void * from, size_t len)
{
	if (len < 512)
		return __memcpy_c(to, from, len);
	return _mmx_memcpy(to, from, len);
}

static inline void *__memcpy3d(void *to, const void *from, size_t len)
{
	if(len < 512)
		return __memcpy_g(to, from, len);
	return _mmx_memcpy(to, from, len);
}

#define memcpy(d, s, count) \
(__builtin_constant_p(count) ? \
 __constant_memcpy3d((d),(s),(count)) : \
 __memcpy3d((d),(s),(count)))
 
#else /* CONFIG_X86_USE_3DNOW */

/*
**	Generic routines
*/


#define memcpy(d, s, count) __memcpy(d, s, count)

#endif /* CONFIG_X86_USE_3DNOW */ 


extern void __struct_cpy_bug( void );

#define struct_cpy(x,y)				\
({						\
	if (sizeof(*(x)) != sizeof(*(y)))	\
		__struct_cpy_bug;		\
	memcpy(x, y, sizeof(*(x)));		\
})


#define __HAVE_ARCH_MEMMOVE
static inline void * memmove(void * dest,const void * src, size_t n)
{
int	d0, d1, d2;
register void *tmp = (void *)dest;
if (dest<src)
__asm__ __volatile__ (
	"rep\n\t"
	"movsb"
	:"=&c" (d0), "=&S" (d1), "=&D" (d2)
	:"0" (n), "1" (src), "2" (tmp)
	:"memory");
else
__asm__ __volatile__ (
	"std\n\t"
	"rep\n\t"
	"movsb\n\t"
	"cld"
	:"=&c" (d0), "=&S" (d1), "=&D" (d2)
	:"0" (n), "1" (n-1+(const char *)src), "2" (n-1+(char *)tmp)
	:"memory");
return dest;
}


#define	__HAVE_ARCH_MEMCMP
static inline int memcmp(const void * cs,const void * ct,size_t count)
{
int	d0, d1, d2;
register int __res;
__asm__ __volatile__(
	"repe\n\t"
	"cmpsb\n\t"
	"je 1f\n\t"
	"sbbl %0,%0\n\t"
	"orb $1,%b0\n"
	"1:"
	:"=a" (__res), "=&S" (d0), "=&D" (d1), "=&c" (d2)
	:"0" (0), "1" (cs), "2" (ct), "3" (count));
return __res;
}


#define __HAVE_ARCH_MEMCHR
static inline void * memchr(const void * cs,int c,size_t count)
{
int	d0;
register void * __res;
if (!count)
	return NULL;
__asm__ __volatile__(
	"repne\n\t"
	"scasb\n\t"
	"je 1f\n\t"
	"movl $1,%0\n"
	"1:\tdecl %0"
	:"=D" (__res), "=&c" (d0)
	:"a" (c), "0" (cs), "1" (count));
return __res;
}

#define __memset_cc(s,c,count) \
((count%4==0) ? \
 __memset_cc_by4((s),(c),(count)) : \
 ((count%2==0) ? \
  __memset_cc_by2((s),(c),(count)) : \
  __memset_cg((s),(c),(count))))

#define __memset_gc(s,c,count) \
((count%4==0) ? \
 __memset_gc_by4((s),(c),(count)) : \
 ((count%2==0) ? \
  __memset_gc_by2((s),(c),(count)) : \
  __memset_gg((s),(c),(count))))

#define __HAVE_ARCH_MEMSET
#define memset(s,c,count) \
(__builtin_constant_p(c) ? \
 (__builtin_constant_p(count) ? \
  __memset_cc((s),(c),(count)) : \
  __memset_cg((s),(c),(count))) : \
 (__builtin_constant_p(count) ? \
  __memset_gc((s),(c),(count)) : \
  __memset_gg((s),(c),(count))))

static inline void * __memset_cc_by4(void * s, char c, size_t count)
{
/*
 * register char *tmp = s;
 */
register char *tmp = (char *)s;
register int  dummy;
__asm__ __volatile__ (
	"\n1:\tmovl %2,(%0)\n\t"
	"addl $4,%0\n\t"
	"decl %1\n\t"
	"jnz 1b"
	:"=r" (tmp), "=r" (dummy)
	:"q" (0x01010101UL * (unsigned char) c), "0" (tmp), "1" (count/4)
	:"memory");
return s;
}

static inline void * __memset_cc_by2(void * s, char c, size_t count)
{
register void *tmp = (void *)s;
register int  dummy;
__asm__ __volatile__ (
	"shrl $1,%1\n\t"          /* may be divisible also by 4 */
	"jz 2f\n"
	"\n1:\tmovl %2,(%0)\n\t"
	"addl $4,%0\n\t"
	"decl %1\n\t"
	"jnz 1b\n"
	"2:\tmovw %w2,(%0)"
	:"=r" (tmp), "=r" (dummy)
	:"q" (0x01010101UL * (unsigned char) c), "0" (tmp), "1" (count/2)
	:"memory");
return s;
}

static inline void * __memset_gc_by4(void * s, char c, size_t count)
{
register void *tmp = (void *)s;
register int dummy;
__asm__ __volatile__ (
	"movb %b0,%h0\n"
	"pushw %w0\n\t"
	"shll $16,%0\n\t"
	"popw %w0\n"
	"1:\tmovl %0,(%1)\n\t"
	"addl $4,%1\n\t"
	"decl %2\n\t"
	"jnz 1b\n"
	:"=q" (c), "=r" (tmp), "=r" (dummy)
	:"0" ((unsigned) c),  "1"  (tmp), "2" (count/4)
	:"memory");
return s;
}

static inline void * __memset_gc_by2(void * s, char c, size_t count)
{
register void *tmp = (void *)s;
register int dummy1,dummy2;
__asm__ __volatile__ (
	"movb %b0,%h0\n\t"
	"shrl $1,%2\n\t"          /* may be divisible also by 4 */
	"jz 2f\n\t"
	"pushw %w0\n\t"
	"shll $16,%0\n\t"
	"popw %w0\n"
	"1:\tmovl %0,(%1)\n\t"
	"addl $4,%1\n\t"
	"decl %2\n\t"
	"jnz 1b\n"
	"2:\tmovw %w0,(%1)"
	:"=q" (dummy1), "=r" (tmp), "=r" (dummy2)
	:"0" ((unsigned) c),  "1"  (tmp), "2" (count/2)
	:"memory");
return s;
}

static inline void * __memset_cg(void * s, char c, size_t count)
{
int	d0, d1;
register void *tmp = (void *)s;
__asm__ __volatile__ (
	"shrl $1,%%ecx\n\t"
	"rep\n\t"
	"stosw\n\t"
	"jnc 1f\n\t"
	"movb %%al,(%%edi)\n"
	"1:"
	:"=&c" (d0), "=&D" (d1) 
	:"a" (0x0101U * (unsigned char) c), "0" (count), "1" (tmp)
	:"memory");
return s;
}

static inline void * __memset_gg(void * s,char c,size_t count)
{
int	d0, d1, d2;
register void *tmp = (void *)s;
__asm__ __volatile__ (
	"movb %%al,%%ah\n\t"
	"shrl $1,%%ecx\n\t"
	"rep\n\t"
	"stosw\n\t"
	"jnc 1f\n\t"
	"movb %%al,(%%edi)\n"
	"1:"
	:"=&c" (d0), "=&D" (d1), "=&D" (d2)
	:"0" (count), "1" (tmp), "2" (c)
	:"memory");
return s;
}


/*
 * find the first occurrence of byte 'c', or 1 past the area if none
 */
#define __HAVE_ARCH_MEMSCAN
static inline void * memscan(void * addr, int c, size_t size)
{
	if (!size)
		return addr;
	__asm__("repnz; scasb
		jnz 1f
		dec %%edi
1:		"
		: "=D" (addr), "=c" (size)
		: "0" (addr), "1" (size), "a" (c));
	return addr;
}

#endif
