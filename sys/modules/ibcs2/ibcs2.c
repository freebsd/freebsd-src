/*-
 * Copyright (c) 1994 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: ibcs2.c,v 1.11 1997/05/01 06:09:01 jkh Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/sysent.h>
#include <sys/lkm.h>

#include <sys/kernel.h>
#include <sys/module.h>

static int
ibcs2_modevent(module_t mod, modeventtype_t type, void *unused)
{
    switch (type) {
    case MOD_LOAD:
	printf("ibcs2 module loaded\n");
	break;
    case MOD_UNLOAD:
	printf("ibcs2 module unloaded\n");
	break;
    default:
	printf("ibcs2 module unknown event: 0x%x\n", type);
    }
    return 0;
}

moduledata_t ibcs2mod = {
    "ibcs2",
    ibcs2_modevent,
    0
};

DECLARE_MODULE(ibcs2, ibcs2mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
