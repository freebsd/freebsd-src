/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * All rights reserved.
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
 */

#include <string.h>
#include <stand.h>
#include <bootstrap.h>

/*
 * Concatenate the (argc) elements of (argv) into a single string, and return
 * a copy of same.
 */
char *
unargv(int argc, char *argv[])
{
    size_t	hlong;
    int		i;
    char	*cp;

    for (i = 0, hlong = 0; i < argc; i++)
	hlong += strlen(argv[i]) + 2;

    if(hlong == 0)
	return(NULL);

    cp = malloc(hlong);
    cp[0] = 0;
    for (i = 0; i < argc; i++) {
	strcat(cp, argv[i]);
	if (i < (argc - 1))
	  strcat(cp, " ");
    }
	  
    return(cp);
}

/*
 * Get the length of a string in kernel space
 */
size_t
strlenout(vm_offset_t src)
{
    char	c;
    size_t	len;
    
    for (len = 0; ; len++) {
	archsw.arch_copyout(src++, &c, 1);
	if (c == 0)
	    break;
    }
    return(len);
}

/*
 * Make a duplicate copy of a string in kernel space
 */
char *
strdupout(vm_offset_t str)
{
    char	*result, *cp;
    
    result = malloc(strlenout(str) + 1);
    for (cp = result; ;cp++) {
	archsw.arch_copyout(str++, cp, 1);
	if (*cp == 0)
	    break;
    }
    return(result);
}

/* Zero a region in kernel space. */
void
kern_bzero(vm_offset_t dest, size_t len)
{
	char buf[256];
	size_t chunk, resid;

	bzero(buf, sizeof(buf));
	resid = len;
	while (resid > 0) {
		chunk = min(sizeof(buf), resid);
		archsw.arch_copyin(buf, dest, chunk);
		resid -= chunk;
		dest += chunk;
	}
}

/*
 * Read the specified part of a file to kernel space.  Unlike regular
 * pread, the file pointer is advanced to the end of the read data,
 * and it just returns 0 if successful.
 */
int
kern_pread(readin_handle_t fd, vm_offset_t dest, size_t len, off_t off)
{

	if (VECTX_LSEEK(fd, off, SEEK_SET) == -1) {
#ifdef DEBUG
		printf("\nlseek failed\n");
#endif
		return (-1);
	}
	if ((size_t)archsw.arch_readin(fd, dest, len) != len) {
#ifdef DEBUG
		printf("\nreadin failed\n");
#endif
		return (-1);
	}
	return (0);
}

/*
 * Read the specified part of a file to a malloced buffer.  The file
 * pointer is advanced to the end of the read data.
 */
/* coverity[ -tainted_data_return ] */
void *
alloc_pread(readin_handle_t fd, off_t off, size_t len)
{
	void *buf;

	buf = malloc(len);
	if (buf == NULL) {
#ifdef DEBUG
		printf("\nmalloc(%d) failed\n", (int)len);
#endif
		errno = ENOMEM;
		return (NULL);
	}
	if (VECTX_LSEEK(fd, off, SEEK_SET) == -1) {
#ifdef DEBUG
		printf("\nlseek failed\n");
#endif
		free(buf);
		return (NULL);
	}
	if ((size_t)VECTX_READ(fd, buf, len) != len) {
#ifdef DEBUG
		printf("\nread failed\n");
#endif
		free(buf);
		return (NULL);
	}
	return (buf);
}

/*
 * mount new rootfs and unmount old, set "currdev" environment variable.
 */
int mount_currdev(struct env_var *ev, int flags, const void *value)
{
	int rv;

	/* mount new rootfs */
	rv = mount(value, "/", 0, NULL);
	if (rv == 0) {
		/*
		 * Note we unmount any previously mounted fs only after
		 * successfully mounting the new because we do not want to
		 * end up with unmounted rootfs.
		 */
		if (ev->ev_value != NULL)
			unmount(ev->ev_value, 0);
		env_setenv(ev->ev_name, flags | EV_NOHOOK, value, NULL, NULL);
	}
	return (rv);
}

/*
 * Set currdev to suit the value being supplied in (value)
 */
int
gen_setcurrdev(struct env_var *ev, int flags, const void *value)
{
	struct devdesc *ncurr;
	int rv;

	if ((rv = devparse(&ncurr, value, NULL)) != 0)
		return (rv);
	free(ncurr);

	return (mount_currdev(ev, flags, value));
}

/*
 * Wrapper to set currdev and loaddev at the same time.
 */
void
set_currdev(const char *devname)
{

	env_setenv("currdev", EV_VOLATILE, devname, gen_setcurrdev,
	    env_nounset);
	/*
	 * Don't execute hook here; the loaddev hook makes it immutable
	 * once we've determined what the proper currdev is.
	 */
	env_setenv("loaddev", EV_VOLATILE | EV_NOHOOK, devname, env_noset,
	    env_nounset);
}
