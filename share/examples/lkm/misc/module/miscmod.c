/* 25 May 93*/
/*
 * miscmod.c
 *
 * 05 Jun 93	Terry Lambert		Split mycall.c out
 * 25 May 93	Terry Lambert		Original
 *
 * Copyright (c) 1993 Terrence R. Lambert.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Terrence R. Lambert.
 * 4. The name Terrence R. Lambert may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TERRENCE R. LAMBERT ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE TERRENCE R. LAMBERT BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/lkm.h>
#include <a.out.h>
#include <sys/file.h>
#include <sys/errno.h>

/* XXX this should be in a header. */
extern int	misccall __P((struct proc *p, void *uap, int retval[]));

/*
 * These two entries define our system call and module information.  We
 * have 0 arguments to our system call.
 */
static struct sysent newent = {
	0,	misccall		/* # of args, function pointer*/
};

/*
 * Miscellaneous modules must have their own save areas...
 */
static struct sysent	oldent;		/* save are for old callslot entry*/

/*
 * Number of syscall entries for a.out executables
 */
#define nsysent (aout_sysvec.sv_size)

MOD_MISC( misc);


/*
 * This function is called each time the module is loaded or unloaded.
 * Since we are a miscellaneous module, we have to provide whatever
 * code is necessary to patch ourselves into the area we are being
 * loaded to change.
 *
 * For the system call table, we duplicate the code in the kern_lkm.c
 * file for patching into the system call table.  We can tell what
 * has been allocated, etc. by virtue of the fact that we know the
 * criteria used by the system call loader interface.  We still
 * kick out the copyright to the console here (to give an example).
 *
 * The stat information is basically common to all modules, so there
 * is no real issue involved with stat; we will leave it lkm_nullcmd(),
 * cince we don't have to do anything about it.
 */
static int
misc_load( lkmtp, cmd)
struct lkm_table	*lkmtp;
int			cmd;
{
	int			i;
	struct lkm_misc		*args = lkmtp->private.lkm_misc;
	int			err = 0;	/* default = success*/

	switch( cmd) {
	case LKM_E_LOAD:

		/*
		 * Don't load twice! (lkmexists() is exported by kern_lkm.c)
		 */
		if( lkmexists( lkmtp))
			return( EEXIST);

		/*
		 * This is where we would express a slot preference if
		 * we had one; since we don't, we will simply duplicate
		 * the "auto" code and forget the other.
		 */

		/*
		 * Search the table looking for a slot...
		 */
		for( i = 0; i < nsysent; i++)
			if( sysent[ i].sy_call == (sy_call_t *)lkmnosys)
				break;		/* found it!*/
		/* out of allocable slots?*/
		if( i == nsysent) {
			err = ENFILE;
			break;
		}

		/* save old -- we must provide our own data area*/
		bcopy( &sysent[ i], &oldent, sizeof( struct sysent));

		/* replace with new*/
		bcopy( &newent, &sysent[ i], sizeof( struct sysent));

		/* done!*/
		args->lkm_offset = i;	/* slot in sysent[]*/


		/* if we make it to here, print copyright on console*/
		printf( "\nSample Loaded miscellaneous module (system call)\n");
		printf( "Copyright (c) 1993\n");
		printf( "Terrence R. Lambert\n");
		printf( "All rights reserved\n");

		break;		/* Success*/

	case LKM_E_UNLOAD:
		/* current slot...*/
		i = args->lkm_offset;

		/* replace current slot contents with old contents*/
		bcopy( &oldent, &sysent[ i], sizeof( struct sysent));

		break;		/* Success*/

	default:	/* we only understand load/unload*/
		err = EINVAL;
		break;
	}

	return( err);
}

/*
 * External entry point; should generally match name of .o file.  The
 * arguments are always the same for all loaded modules.  The "load",
 * "unload", and "stat" functions in "DISPATCH" will be called under
 * their respective circumstances unless their value is "lkm_nullcmd".  If
 * called, they are called with the same arguments (cmd is included to
 * allow the use of a single function, ver is included for version
 * matching between modules and the kernel loader for the modules).
 *
 * Since we expect to link in the kernel and add external symbols to
 * the kernel symbol name space in a future version, generally all
 * functions used in the implementation of a particular module should
 * be static unless they are expected to be seen in other modules or
 * to resolve unresolved symbols alread existing in the kernel (the
 * second case is not likely to ever occur).
 *
 * The entry point should return 0 unless it is refusing load (in which
 * case it should return an errno from errno.h).
 */
int
misc_mod( lkmtp, cmd, ver)
struct lkm_table	*lkmtp;
int			cmd;
int			ver;
{
	DISPATCH(lkmtp, cmd, ver, misc_load, misc_load, lkm_nullcmd);
}
