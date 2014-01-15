/*-
 * Copyright (c) 2010-2013 Kai Wang
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

#include "ld.h"
#include "ld_arch.h"
#include "ld_ehframe.h"
#include "ld_options.h"
#include "ld_reloc.h"
#include "ld_script.h"
#include "ld_file.h"
#include "ld_input.h"
#include "ld_layout.h"
#include "ld_output.h"
#include "ld_path.h"
#include "ld_symbols.h"

ELFTC_VCSID("$Id: ld_main.c 2959 2013-08-25 03:12:47Z kaiwang27 $");

static struct ld _ld;
struct ld* ld = &_ld;

static void
_init(void)
{

	if ((ld->ld_progname = ELFTC_GETPROGNAME()) == NULL)
		ld->ld_progname = "ld";

	/* Initialise libelf. */
	if (elf_version(EV_CURRENT) == EV_NONE)
		ld_fatal(ld, "ELF library initialization failed: %s",
		    elf_errmsg(-1));

	/* Initialise internal data structure. */
	TAILQ_INIT(&ld->ld_lflist);
	STAILQ_INIT(&ld->ld_lilist);
	STAILQ_INIT(&ld->ld_state.ls_lplist);
	STAILQ_INIT(&ld->ld_state.ls_rplist);
	STAILQ_INIT(&ld->ld_state.ls_rllist);
}

static void
_cleanup(void)
{

	ld_script_cleanup(ld);
	ld_symbols_cleanup(ld);
	ld_path_cleanup(ld);
	ld_input_cleanup(ld);
	ld_file_cleanup(ld);
}

int
main(int argc, char **argv)
{
	struct ld_state *ls;

	_init();

	ls = &ld->ld_state;

	ld->ld_progname = basename(argv[0]);

	ld_arch_init(ld);

restart:

	/* The linker generate an executable by default */
	ld->ld_exec = 1;

	ld_script_init(ld);

	ld_options_parse(ld, argc, argv);

	ld_output_early_init(ld);

	ls->ls_arch_conflict = 0;
	ls->ls_first_elf_object = 1;

	ld_input_init(ld);

	ld_symbols_resolve(ld);

	if (ls->ls_arch_conflict) {
		_cleanup();
		ls->ls_rerun = 1;
		goto restart;
	}

	ld_reloc_load(ld);

	/*
	 * Perform section garbage collection if command line option
	 * -gc-sections is specified. Perform deferred relocation scan
	 * after garbage sections are found.
	 */
	if (ld->ld_gc) {
		ld_reloc_gc_sections(ld);
		ld_reloc_deferred_scan(ld);
	}

	/*
	 * Search for undefined symbols and allocate space for common
	 * symbols. Copy relevant symbols to the dynamic symbol table
	 * if the linker is performing a dyanmic linking.
	 */
	ld_symbols_scan(ld);

	/* Create .eh_frame_hdr section. */
	if (ld->ld_ehframe_hdr)
		ld_ehframe_create_hdr(ld);

	ld_output_init(ld);

	ld_layout_sections(ld);

	ld_output_create(ld);

	_cleanup();

	exit(EXIT_SUCCESS);
}
