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
 *	$Id: socksys.c,v 1.1 1994/10/16 20:38:50 sos Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/conf.h>
#include <sys/sysent.h>
#include <sys/lkm.h>
#include <sys/errno.h>

int sockopen();
int sockclose();
int sockioctl();

struct cdevsw dev_socksys = { 
	(d_open_t *)sockopen,	(d_close_t *)sockclose,	
	(d_rdwr_t *)enodev,	(d_rdwr_t *)enodev,
  	(d_ioctl_t *)sockioctl,	(d_stop_t *)enodev,
	(d_reset_t *)nullop,	NULL,
  	(d_select_t *)seltrue,	(d_mmap_t *)enodev,		
	NULL
};

MOD_DEV("socksys_mod", LM_DT_CHAR, -1, (void *)&dev_socksys)

socksys_load(struct lkm_table *lkmtp, int cmd)
{
	uprintf("socksys driver installed\n");
	return 0;
}

socksys_unload(struct lkm_table *lkmtp, int cmd)
{
	uprintf("socksys driver removed\n");
	return 0;
}

socksys_init(struct lkm_table *lkmtp, int cmd, int ver)
{
	DISPATCH(lkmtp, cmd, ver, socksys_load, socksys_unload, nosys);
}
