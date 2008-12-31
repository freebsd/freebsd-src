/*-
 * Copyright (c) 2006 Kip Macy
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/sun4v/mdesc/mdesc_subr.c,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/malloc.h>


#include <machine/cddl/mdesc.h>
#include <machine/cddl/mdesc_impl.h>


int
md_get_prop_alloc(md_t *ptr, mde_cookie_t node, char *namep, int tag_type,
		 uint8_t **datap)
{
        mde_str_cookie_t prop_name;
        md_impl_t       *mdp;
        mde_cookie_t     elem;
	int              len;

        mdp = (md_impl_t *)ptr;

        if (node == MDE_INVAL_ELEM_COOKIE) {
                return (-1);
        }

        prop_name = md_find_name(ptr, namep);
        if (prop_name == MDE_INVAL_STR_COOKIE) {
                return (-1);
        }

        elem = md_find_node_prop(mdp, node, prop_name, tag_type);

        if (elem != MDE_INVAL_ELEM_COOKIE) {
                md_element_t *mdep;
                mdep = &(mdp->mdep[(int)elem]);

                len = (int)MDE_PROP_DATA_LEN(mdep);
		KASSERT(len > 0, ("invalid length"));
		*datap = malloc(len, M_MDPROP, M_WAITOK);
		bcopy(mdp->datap + (int)MDE_PROP_DATA_OFFSET(mdep), *datap, len);
                return (0);
        }

        return (-1);    /* no such property name */
}


