/* 08 Nov 1998*/
/*
 * miscmod.c
 *
 * 08 Nov 1998  Rajesh Vaidheeswarran  - adapted from the lkm miscmod.c
 *
 * Copyright (c) 1998 Rajesh Vaidheeswarran
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
 *      This product includes software developed by Rajesh Vaidheeswarran.
 * 4. The name Rajesh Vaidheeswarran may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY RAJESH VAIDHEESWARRAN ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE RAJESH VAIDHEESWARRAN BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
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
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/conf.h>

#include "misccall.h"

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
static struct sysent	oldent;		/* save area for old callslot entry*/

/*
 * Number of syscall entries for a.out executables
 */
#define nsysent (aout_sysvec.sv_size)

/*
 * If you have a data structure to pass ...
 */

typedef struct misc_data {
    char name[20];
    struct sysent * nent;
    struct sysent * oent;
    int offset;
} misc_data_t;

static misc_data_t misc_arg = {
    "misccall",
    &newent,
    &oldent,
    -1
};

static int misc_load(module_t,   modeventtype_t, void *);

static moduledata_t misc_mod = {
    "misc_mod",
    misc_load,
    (void *)&misc_arg /* This is our real data */
};

/*
 * This function is called each time the module is loaded or unloaded.
 *
 * For the system call table, we duplicate the code in the kern_lkm.c
 * file for patching into the system call table.  We can tell what
 * has been allocated, etc. by virtue of the fact that we know the
 * criteria used by the system call loader interface.  We still
 * kick out the copyright to the console here (to give an example).
 */
static int
misc_load(mod, cmd, arg)
    module_t	mod;
    modeventtype_t cmd;
    void * arg;
{
    int i, err = 0;

    misc_data_t * args = (misc_data_t *)arg;

    switch (cmd) {
    case MOD_LOAD:
	
	/*
	 * Search the table looking for a slot...
	 */
	for(i = 1; i < nsysent; i++)
	    if(sysent[i].sy_call == (sy_call_t *)nosys)
		break;		/* found it!*/
	/* out of allocable slots?*/
	if(i == nsysent) {
	    err = ENFILE;
	    break;
	}

	/* save old -- we must provide our own data area*/
	bcopy(&sysent[i], args->oent, sizeof(struct sysent));

	/* replace with new*/
	bcopy(args->nent, &sysent[i], sizeof(struct sysent));

	/* done!*/
	args->offset = i;	/* slot in sysent[]*/


	/* if we make it to here, print copyright on console*/
	printf("\nSample Loaded kld module (system call)\n");
	printf("Copyright (c) 1998\n");
	printf("Rajesh Vaidheeswarran\n");
	printf("All rights reserved\n");
	printf("System call %s loaded into slot %d\n", args->name, 
	       args->offset);
	break;		/* Success*/

    case MOD_UNLOAD:
	/* current slot...*/
	i = args->offset;

	/* replace current slot contents with old contents*/
	bcopy(args->oent, &sysent[ i], sizeof( struct sysent));

	printf("Unloaded kld module (system call)\n");

	break;		/* Success*/

    default:	/* we only understand load/unload*/
	err = EINVAL;
	break;
    }

    return(err);
}

/* Now declare the module to the system */

DECLARE_MODULE(misc, misc_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
