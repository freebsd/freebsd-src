/*
 * Copyright (c) 2001-2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 *	All rights reserved.
 *
 * Author: Harti Brandt <harti@freebsd.org>
 *
 * Redistribution of this software and documentation and use in source and
 * binary forms, with or without modification, are permitted provided that
 * the following conditions are met:
 *
 * 1. Redistributions of source code or documentation must retain the above
 *    copyright notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE AND DOCUMENTATION IS PROVIDED BY FRAUNHOFER FOKUS
 * AND ITS CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * FRAUNHOFER FOKUS OR ITS CONTRIBUTORS  BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Begemot: bsnmp/snmp_mibII/mibII_ifstack.c,v 1.5 2003/01/28 13:44:35 hbb Exp $
 *
 * ifStackTable. Read-only.
 */
#include "mibII.h"

int
mib_ifstack_create(const struct mibif *lower, const struct mibif *upper)
{
	struct mibifstack *stack;

	if ((stack = malloc(sizeof(*stack))) == NULL)
		return (-1);

	stack->index.len = 2;
	stack->index.subs[0] = upper ? upper->index : 0;
	stack->index.subs[1] = lower ? lower->index : 0;

	INSERT_OBJECT_OID(stack, &mibifstack_list);

	mib_ifstack_last_change = get_ticks();

	return (0);
}

void
mib_ifstack_delete(const struct mibif *lower, const struct mibif *upper)
{
	struct mibifstack *stack;

	TAILQ_FOREACH(stack, &mibifstack_list, link)
		if (stack->index.subs[0] == (upper ? upper->index : 0) &&
		    stack->index.subs[1] == (lower ? lower->index : 0)) {
			TAILQ_REMOVE(&mibifstack_list, stack, link);
			free(stack);
			mib_ifstack_last_change = get_ticks();
			return;
		}
}

int
op_ifstack(struct snmp_context *ctx __unused, struct snmp_value *value,
    u_int sub, u_int iidx __unused, enum snmp_op op)
{
	struct mibifstack *stack;

	switch (op) {

	  case SNMP_OP_GETNEXT:
		if ((stack = NEXT_OBJECT_OID(&mibifstack_list, &value->var, sub)) == NULL)
			return (SNMP_ERR_NOSUCHNAME);
		index_append(&value->var, sub, &stack->index);
		break;

	  case SNMP_OP_GET:
		if ((stack = FIND_OBJECT_OID(&mibifstack_list, &value->var, sub)) == NULL)
			return (SNMP_ERR_NOSUCHNAME);
		break;

	  case SNMP_OP_SET:
		if ((stack = FIND_OBJECT_OID(&mibifstack_list, &value->var, sub)) == NULL)
			return (SNMP_ERR_NO_CREATION);
		return (SNMP_ERR_NOT_WRITEABLE);

	  case SNMP_OP_ROLLBACK:
	  case SNMP_OP_COMMIT:
		abort();
	}

	switch (value->var.subs[sub - 1]) {

	  case LEAF_ifStackStatus:
		value->v.integer = 1;
		break;
	}
	return (SNMP_ERR_NOERROR);
}
