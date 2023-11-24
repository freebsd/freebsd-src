/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 George V. Neville-Neil
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

/*
 * SKEL Loadable Kernel Module for the FreeBSD Operating System
 * 
 * The SKEL module is meant to act as a skeleton for creating new
 * kernel modules.
 *
 * This module can be loaded and unloaded from * FreeBSD and is for
 * use in teaching as well.
 *
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/module.h>

/*
 * Every module can have a module specific piece of code that is
 * executed whenever the module is loaded or unloaded.  The following
 * is a trivial example that prints a message on the console whenever
 * the module is loaded or unloaded.
 */

static int
skel_mod_event(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_LOAD:
		printf("SKEL module loading.\n");
		return (0);
	case MOD_UNLOAD:
		printf("SKEL module unloading.\n");
		return (0);
	}
	return (EOPNOTSUPP);
}

/*
 * Modules can have associated data and the module data also contains
 * an entry for the function called by the kernel on load and unload.
 */

static moduledata_t skel_mod = {
	"skel",
	skel_mod_event,
	NULL,
};

/*
 * Each module is declared with its name and module data.  The
 * ordering arguments at the end put this module into the device
 * driver class, which is sufficient for our needs.  The complete list
 * of modules types and ording can be found in sys/kernel.h
 */

DECLARE_MODULE(skel, skel_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);

