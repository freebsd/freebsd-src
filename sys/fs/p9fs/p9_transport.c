/*-
 * Copyright (c) 2022-present Doug Rabson
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
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/kassert.h>
#include <sys/libkern.h>

#include <fs/p9fs/p9_transport.h>

TAILQ_HEAD(, p9_trans_module) transports;

static void
p9_transport_init(void)
{

        TAILQ_INIT(&transports);
}

SYSINIT(p9_transport, SI_SUB_DRIVERS, SI_ORDER_FIRST, p9_transport_init, NULL);

void
p9_register_trans(struct p9_trans_module *m)
{

        TAILQ_INSERT_TAIL(&transports, m, link);
}
        
void
p9_unregister_trans(struct p9_trans_module *m)
{

        TAILQ_REMOVE(&transports, m, link);
}

struct p9_trans_module *
p9_get_trans_by_name(char *name)
{
        struct p9_trans_module *m;

        TAILQ_FOREACH(m, &transports, link) {
                if (strcmp(m->name, name) == 0)
                        return (m);
        }
        return (NULL);
}

