/*-
 * Copyright (c) 1999 Assar Westerlund
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/syscall.h>
#include <sys/module.h>

/*
 * Acts like "nosys" but can be identified in sysent for dynamic call 
 * number assignment for a limited number of calls. 
 * 
 * Place holder for system call slots reserved for loadable modules.
 */     
int
lkmnosys(struct proc *p, struct nosys_args *args)
{
	return(nosys(p, args));
}

int
lkmressys(struct proc *p, struct nosys_args *args)
{
	return(nosys(p, args));
}

int
syscall_register(int *offset, struct sysent *new_sysent,
		 struct sysent *old_sysent)
{
       if (*offset == NO_SYSCALL) {
               int i;

               for (i = 1; i < SYS_MAXSYSCALL; ++i)
                       if (sysent[i].sy_call == (sy_call_t *)lkmnosys)
                               break;
               if (i == SYS_MAXSYSCALL)
                       return ENFILE;
               *offset = i;
       } else if (*offset < 0 || *offset >= SYS_MAXSYSCALL)
               return EINVAL;
       else if (sysent[*offset].sy_call != (sy_call_t *)lkmnosys &&
				sysent[*offset].sy_call != (sy_call_t *)lkmressys)
               return EEXIST;

       *old_sysent = sysent[*offset];
       sysent[*offset] = *new_sysent;
       return 0;
}

int
syscall_deregister(int *offset, struct sysent *old_sysent)
{
       if (*offset)
               sysent[*offset] = *old_sysent;
       return 0;
}

int
syscall_module_handler(struct module *mod, int what, void *arg)
{
       struct syscall_module_data *data = (struct syscall_module_data*)arg;
       modspecific_t ms;
       int error;

       switch (what) {
       case MOD_LOAD :
               error = syscall_register(data->offset, data->new_sysent,
                                        &data->old_sysent);
               if (error)
                       return error;
	       ms.intval = *data->offset;
	       module_setspecific(mod, &ms);
               if (data->chainevh)
                       error = data->chainevh(mod, what, data->chainarg);
               return error;

       case MOD_UNLOAD :
               if (data->chainevh) {
                       error = data->chainevh(mod, what, data->chainarg);
                       if (error)
                               return error;
               }
               error = syscall_deregister(data->offset, &data->old_sysent);
               return error;
       }

       if (data->chainevh)
               return data->chainevh(mod, what, data->chainarg);
       else
               return 0;
}
