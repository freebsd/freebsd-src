/*-
 * Copyright (c) 1999, 2000, 2001, 2002 Robert N. M. Watson
 * Copyright (c) 2001, 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
 *
 * This software was developed for the FreeBSD Project in part by NAI Labs,
 * the Security Research Division of Network Associates, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA
 * CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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

/*
 * Developed by the TrustedBSD Project.
 * Experiment with a partition-like model.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/mac.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <fs/devfs/devfs.h>

#include <net/bpfdesc.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <vm/vm.h>

#include <sys/mac_policy.h>

#include <security/mac_partition/mac_partition.h>

SYSCTL_DECL(_security_mac);

SYSCTL_NODE(_security_mac, OID_AUTO, partition, CTLFLAG_RW, 0,
    "TrustedBSD mac_partition policy controls");

static int	mac_partition_enabled = 1;
SYSCTL_INT(_security_mac_partition, OID_AUTO, enabled, CTLFLAG_RW,
    &mac_partition_enabled, 0, "Enforce partition policy");

static int	partition_slot;
#define	SLOT(l)	(LABEL_TO_SLOT((l), partition_slot).l_long)

static void
mac_partition_init(struct mac_policy_conf *conf)
{

}

static void
mac_partition_init_label(struct label *label)
{

	SLOT(label) = 0;
}

static void
mac_partition_destroy_label(struct label *label)
{

	SLOT(label) = 0;
}

static int
mac_partition_externalize_label(struct label *label, char *element_name,
    char *element_data, size_t size, size_t *len, int *claimed)
{

	if (strcmp(MAC_PARTITION_LABEL_NAME, element_name) != 0)
		return (0);

	(*claimed)++;
	*len = snprintf(element_data, size, "%ld", SLOT(label));
	return (0);
}

static int
mac_partition_internalize_label(struct label *label, char *element_name,
    char *element_data, int *claimed)
{

	if (strcmp(MAC_PARTITION_LABEL_NAME, element_name) != 0)
		return (0);

	(*claimed)++;
	SLOT(label) = strtol(element_data, NULL, 10);
	return (0);
}

static void
mac_partition_create_cred(struct ucred *cred_parent, struct ucred *cred_child)
{

	SLOT(&cred_child->cr_label) = SLOT(&cred_parent->cr_label);
}

static void
mac_partition_create_proc0(struct ucred *cred)
{

	SLOT(&cred->cr_label) = 0;
}

static void
mac_partition_create_proc1(struct ucred *cred)
{

	SLOT(&cred->cr_label) = 0;
}

static void
mac_partition_relabel_cred(struct ucred *cred, struct label *newlabel)
{

	if (SLOT(newlabel) != 0)
		SLOT(&cred->cr_label) = SLOT(newlabel);
}

static int
label_on_label(struct label *subject, struct label *object)
{

	if (mac_partition_enabled == 0)
		return (0);

	if (SLOT(subject) == 0)
		return (0);

	if (SLOT(subject) == SLOT(object))
		return (0);

	return (EPERM);
}

static int
mac_partition_check_cred_relabel(struct ucred *cred, struct label *newlabel)
{
	int error;

	error = 0;

	/* Treat "0" as a no-op request. */
	if (SLOT(newlabel) != 0) {
		/* If we're already in a partition, can't repartition. */
		if (SLOT(&cred->cr_label) != 0)
			return (EPERM);

		/*
		 * If not in a partition, must have privilege to create
		 * one.
		 */
		error = suser_cred(cred, 0);
	}

	return (error);
}

static int
mac_partition_check_cred_visible(struct ucred *u1, struct ucred *u2)
{
	int error;

	error = label_on_label(&u1->cr_label, &u2->cr_label);

	return (error == 0 ? 0 : ESRCH);
}

static int
mac_partition_check_proc_debug(struct ucred *cred, struct proc *proc)
{
	int error;

	error = label_on_label(&cred->cr_label, &proc->p_ucred->cr_label);

	return (error ? ESRCH : 0);
}

static int
mac_partition_check_proc_sched(struct ucred *cred, struct proc *proc)
{
	int error;

	error = label_on_label(&cred->cr_label, &proc->p_ucred->cr_label);

	return (error ? ESRCH : 0);
}

static int
mac_partition_check_proc_signal(struct ucred *cred, struct proc *proc,
    int signum)
{
	int error;

	error = label_on_label(&cred->cr_label, &proc->p_ucred->cr_label);

	return (error ? ESRCH : 0);
}

static int
mac_partition_check_socket_visible(struct ucred *cred, struct socket *socket,
    struct label *socketlabel)
{
	int error;

	error = label_on_label(&cred->cr_label, socketlabel);

	return (error ? ENOENT : 0);
}

static struct mac_policy_op_entry mac_partition_ops[] =
{
	{ MAC_INIT,
	    (macop_t)mac_partition_init },
	{ MAC_INIT_CRED_LABEL,
	    (macop_t)mac_partition_init_label },
	{ MAC_DESTROY_CRED_LABEL,
	    (macop_t)mac_partition_destroy_label },
	{ MAC_EXTERNALIZE_CRED_LABEL,
	    (macop_t)mac_partition_externalize_label },
	{ MAC_INTERNALIZE_CRED_LABEL,
	    (macop_t)mac_partition_internalize_label },
	{ MAC_CREATE_CRED,
	    (macop_t)mac_partition_create_cred },
	{ MAC_CREATE_PROC0,
	    (macop_t)mac_partition_create_proc0 },
	{ MAC_CREATE_PROC1,
	    (macop_t)mac_partition_create_proc1 },
	{ MAC_RELABEL_CRED,
	    (macop_t)mac_partition_relabel_cred },
	{ MAC_CHECK_CRED_RELABEL,
	    (macop_t)mac_partition_check_cred_relabel },
	{ MAC_CHECK_CRED_VISIBLE,
	    (macop_t)mac_partition_check_cred_visible },
	{ MAC_CHECK_PROC_DEBUG,
	    (macop_t)mac_partition_check_proc_debug },
	{ MAC_CHECK_PROC_SCHED,
	    (macop_t)mac_partition_check_proc_sched },
	{ MAC_CHECK_PROC_SIGNAL,
	    (macop_t)mac_partition_check_proc_signal },
	{ MAC_CHECK_SOCKET_VISIBLE,
	    (macop_t)mac_partition_check_socket_visible },
	{ MAC_OP_LAST, NULL }
};

MAC_POLICY_SET(mac_partition_ops, trustedbsd_mac_partition,
    "TrustedBSD MAC/Partition", MPC_LOADTIME_FLAG_UNLOADOK, &partition_slot);
