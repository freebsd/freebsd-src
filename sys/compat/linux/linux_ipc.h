/*-
 * Copyright (c) 2000 Marcel Moolenaar
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
 *    derived from this software without specific prior written permission.
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
 * $FreeBSD$
 */

#ifndef _LINUX_IPC_H_
#define _LINUX_IPC_H_

#ifndef __alpha__

#define linux_msgctl_args linux_ipc_args 
#define linux_msgget_args linux_ipc_args
#define linux_msgrcv_args linux_ipc_args
#define linux_msgsnd_args linux_ipc_args

#define linux_semctl_args linux_ipc_args
#define linux_semget_args linux_ipc_args
#define linux_semop_args linux_ipc_args

#define linux_shmat_args linux_ipc_args
#define linux_shmctl_args linux_ipc_args
#define linux_shmdt_args linux_ipc_args
#define linux_shmget_args linux_ipc_args

int linux_msgctl __P((struct proc *, struct linux_msgctl_args *));
int linux_msgget __P((struct proc *, struct linux_msgget_args *));
int linux_msgrcv __P((struct proc *, struct linux_msgrcv_args *));
int linux_msgsnd __P((struct proc *, struct linux_msgsnd_args *));

int linux_semctl __P((struct proc *, struct linux_semctl_args *));
int linux_semget __P((struct proc *, struct linux_semget_args *));
int linux_semop  __P((struct proc *, struct linux_semop_args *));

int linux_shmat  __P((struct proc *, struct linux_shmat_args *));
int linux_shmctl __P((struct proc *, struct linux_shmctl_args *));
int linux_shmdt  __P((struct proc *, struct linux_shmdt_args *));
int linux_shmget __P((struct proc *, struct linux_shmget_args *));
#endif	/*!__alpha__*/

#endif /* _LINUX_IPC_H_ */
