/*
 * mjs copyright
 */
/*
 * MD primitives supporting placement of module data 
 *
 * XXX should check load address/size against memory top.
 */
#include <stand.h>

#include "libi386.h"
#include "btxv86.h"

#define READIN_BUF	4096

int
i386_copyin(void *src, vm_offset_t dest, size_t len)
{
    if (dest + len >= memtop)
	return(0);

    bcopy(src, PTOV(dest), len);
    return(len);
}

int
i386_copyout(vm_offset_t src, void *dest, size_t len)
{
    if (src + len >= memtop)
	return(0);
    
    bcopy(PTOV(src), dest, len);
    return(len);
}


int
i386_readin(int fd, vm_offset_t dest, size_t len)
{
    void	*buf;
    size_t	resid, chunk, get, got;

    if (dest + len >= memtop)
	return(0);

    chunk = min(READIN_BUF, len);
    buf = malloc(chunk);
    if (buf == NULL)
	return(0);

    for (resid = len; resid > 0; resid -= got, dest += got) {
	get = min(chunk, resid);
	got = read(fd, buf, get);
	if (got <= 0)
	    break;
	bcopy(buf, PTOV(dest), chunk);
    }
    free(buf);
    return(len - resid);
}

    
