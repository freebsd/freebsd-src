/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 The FreeBSD Foundation
 *
 * This software was developed by Olivier Certner <olce.freebsd@certner.fr> at
 * Kumacom SARL under sponsorship from the FreeBSD Foundation.
 */

/*
 * Prototypes for functions used to implement system calls that must manipulate
 * MAC labels.
 */

#ifndef _SECURITY_MAC_MAC_SYSCALLS_H_
#define _SECURITY_MAC_MAC_SYSCALLS_H_

#ifndef _KERNEL
#error "no user-serviceable parts inside"
#endif

int	mac_label_copyin(const void *const u_mac, struct mac *const mac,
	    char **const u_string);
void	free_copied_label(const struct mac *const mac);

int	mac_set_proc_prepare(struct thread *const td,
	    const struct mac *const mac, void **const mac_set_proc_data);
int	mac_set_proc_core(struct thread *const td, struct ucred *const newcred,
	    void *const mac_set_proc_data);
void	mac_set_proc_finish(struct thread *const td, bool proc_label_set,
	    void *const mac_set_proc_data);

#endif /* !_SECURITY_MAC_MAC_SYSCALLS_H_ */
