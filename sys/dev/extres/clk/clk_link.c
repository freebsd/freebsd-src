/*-
 * Copyright 2016 Michal Meloun <mmel@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/extres/clk/clk_link.h>

static int clknode_link_init(struct clknode *clk, device_t dev);
static int clknode_link_recalc(struct clknode *clk, uint64_t *freq);
static int clknode_link_set_freq(struct clknode *clk, uint64_t fin,
    uint64_t *fout, int flags, int *stop);
static int clknode_link_set_mux(struct clknode *clk, int idx);
static int clknode_link_set_gate(struct clknode *clk, bool enable);

static clknode_method_t clknode_link_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,	   clknode_link_init),
	CLKNODEMETHOD(clknode_recalc_freq, clknode_link_recalc),
	CLKNODEMETHOD(clknode_set_freq,	   clknode_link_set_freq),
	CLKNODEMETHOD(clknode_set_gate,	   clknode_link_set_gate),
	CLKNODEMETHOD(clknode_set_mux,	   clknode_link_set_mux),
	CLKNODEMETHOD_END
};
DEFINE_CLASS_1(clknode_link, clknode_link_class, clknode_link_methods,
   0, clknode_class);

static int
clknode_link_init(struct clknode *clk, device_t dev)
{
	return(0);
}

static int
clknode_link_recalc(struct clknode *clk, uint64_t *freq)
{

	printf("%s: Attempt to use unresolved linked clock: %s\n", __func__,
	    clknode_get_name(clk));
	return (EBADF);
}

static int
clknode_link_set_freq(struct clknode *clk, uint64_t fin, uint64_t *fout,
    int flags, int *stop)
{

	printf("%s: Attempt to use unresolved linked clock: %s\n", __func__,
	    clknode_get_name(clk));
	return (EBADF);
}

static int
clknode_link_set_mux(struct clknode *clk, int idx)
{

	printf("%s: Attempt to use unresolved linked clock: %s\n", __func__,
	    clknode_get_name(clk));
	return (EBADF);
}

static int
clknode_link_set_gate(struct clknode *clk, bool enable)
{

	printf("%s: Attempt to use unresolved linked clock: %s\n", __func__,
	    clknode_get_name(clk));
	return (EBADF);
}

int
clknode_link_register(struct clkdom *clkdom, struct clk_link_def *clkdef)
{
	struct clknode *clk;
	struct clknode_init_def tmp;

	tmp = clkdef->clkdef;
	tmp.flags |= CLK_NODE_LINKED;
	clk = clknode_create(clkdom, &clknode_link_class, &tmp);
	if (clk == NULL)
		return (1);
	clknode_register(clkdom, clk);
	return (0);
}
