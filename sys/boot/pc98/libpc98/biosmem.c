/*
 * mjs copyright
 *
 * $FreeBSD$
 */

/*
 * Obtain memory configuration information from the BIOS
 *
 * Note that we don't try too hard here; knowing the size of
 * base memory and extended memory out to 16 or 64M is enough for
 * the requirements of the bootstrap.
 *
 * We also maintain a pointer to the top of physical memory
 * once called to allow rangechecking of load/copy requests.
 */
#include <stand.h>
#include "btxv86.h"

vm_offset_t	memtop;

/*
 * Return base memory size in kB.
 */
int
getbasemem(void)
{
#ifdef PC98
    return ((*(u_char *)PTOV(0xA1501)&0x07)+1)*128;
#else
    v86.ctl = 0;
    v86.addr = 0x12;		/* int 0x12 */
    v86int();

    return(v86.eax & 0xffff);
#endif
}

/*
 * Return extended memory size in kB
 */
int
getextmem(void)
{
    int		extkb;
    
#ifdef PC98
    extkb = *(u_char *)PTOV(0xA1401)*128 + *(unsigned short *)PTOV(0xA1594)*1024;
#else    
    v86.ctl = 0;
    v86.addr = 0x15;		/* int 0x15 function 0x88*/
    v86.eax = 0x8800;
    v86int();
    extkb = v86.eax & 0xffff;
#endif

    /* Set memtop to actual top or 16M, whicheve is less */
    memtop = min((0x100000 + (extkb * 1024)), (16 * 1024 * 1024));
    
    return(extkb);
}

