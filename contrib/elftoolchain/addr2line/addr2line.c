/*-
 * Copyright (c) 2009 Kai Wang
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
#include <dwarf.h>
#include <err.h>
#include <fcntl.h>
#include <gelf.h>
#include <getopt.h>
#include <libdwarf.h>
#include <libelftc.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "_elftc.h"

ELFTC_VCSID("$Id: addr2line.c 3249 2015-10-04 08:11:30Z kaiwang27 $");

static struct option longopts[] = {
	{"target" , required_argument, NULL, 'b'},
	{"demangle", no_argument, NULL, 'C'},
	{"exe", required_argument, NULL, 'e'},
	{"functions", no_argument, NULL, 'f'},
	{"section", required_argument, NULL, 'j'},
	{"basename", no_argument, NULL, 's'},
	{"help", no_argument, NULL, 'H'},
	{"version", no_argument, NULL, 'V'},
	{NULL, 0, NULL, 0}
};
static int demangle, func, base;
static char unknown[] = { '?', '?', '\0' };
static Dwarf_Addr section_base;

#define	USAGE_MESSAGE	"\
Usage: %s [options] hexaddress...\n\
  Map program addresses to source file names and line numbers.\n\n\
  Options:\n\
  -b TGT  | --target=TGT      (Accepted but ignored).\n\
  -e EXE  | --exe=EXE         Use program \"EXE\" to translate addresses.\n\
  -f      | --functions       Display function names.\n\
  -j NAME | --section=NAME    Values are offsets into section \"NAME\".\n\
  -s      | --basename        Only show the base name for each file name.\n\
  -C      | --demangle        Demangle C++ names.\n\
  -H      | --help            Print a help message.\n\
  -V      | --version         Print a version identifier and exit.\n"

static void
usage(void)
{
	(void) fprintf(stderr, USAGE_MESSAGE, ELFTC_GETPROGNAME());
	exit(1);
}

static void
version(void)
{

	fprintf(stderr, "%s (%s)\n", ELFTC_GETPROGNAME(), elftc_version());
	exit(0);
}

/*
 * Handle DWARF 4 'offset from' DW_AT_high_pc.  Although we don't
 * fully support DWARF 4, some compilers (like FreeBSD Clang 3.5.1)
 * generate DW_AT_high_pc as an offset from DW_AT_low_pc.
 *
 * "If the value of the DW_AT_high_pc is of class address, it is the
 * relocated address of the first location past the last instruction
 * associated with the entity; if it is of class constant, the value
 * is an unsigned integer offset which when added to the low PC gives
 * the address of the first location past the last instruction
 * associated with the entity."
 *
 * DWARF4 spec, section 2.17.2.
 */
static int
handle_high_pc(Dwarf_Die die, Dwarf_Unsigned lopc, Dwarf_Unsigned *hipc)
{
	Dwarf_Error de;
	Dwarf_Half form;
	Dwarf_Attribute at;
	int ret;

	ret = dwarf_attr(die, DW_AT_high_pc, &at, &de);
	if (ret == DW_DLV_ERROR) {
		warnx("dwarf_attr failed: %s", dwarf_errmsg(de));
		return (ret);
	}
	ret = dwarf_whatform(at, &form, &de);
	if (ret == DW_DLV_ERROR) {
		warnx("dwarf_whatform failed: %s", dwarf_errmsg(de));
		return (ret);
	}
	if (dwarf_get_form_class(2, 0, 0, form) == DW_FORM_CLASS_CONSTANT)
		*hipc += lopc;

	return (DW_DLV_OK);
}

static void
search_func(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Addr addr, char **rlt_func)
{
	Dwarf_Die ret_die, spec_die;
	Dwarf_Error de;
	Dwarf_Half tag;
	Dwarf_Unsigned lopc, hipc;
	Dwarf_Off ref;
	Dwarf_Attribute sub_at, spec_at;
	char *func0;
	const char *func1;
	int ret;

	if (*rlt_func != NULL)
		goto done;

	if (dwarf_tag(die, &tag, &de)) {
		warnx("dwarf_tag: %s", dwarf_errmsg(de));
		goto cont_search;
	}
	if (tag == DW_TAG_subprogram) {
		if (dwarf_attrval_unsigned(die, DW_AT_low_pc, &lopc, &de) ||
		    dwarf_attrval_unsigned(die, DW_AT_high_pc, &hipc, &de))
			goto cont_search;
		if (handle_high_pc(die, lopc, &hipc) != DW_DLV_OK)
			goto cont_search;
		if (addr < lopc || addr >= hipc)
			goto cont_search;

		/* Found it! */

		if ((*rlt_func = strdup(unknown)) == NULL)
			err(EXIT_FAILURE, "strdup");
		ret = dwarf_attr(die, DW_AT_name, &sub_at, &de);
		if (ret == DW_DLV_ERROR)
			goto done;
		if (ret == DW_DLV_OK) {
			if (dwarf_formstring(sub_at, &func0, &de) ==
			    DW_DLV_OK) {
				free(*rlt_func);
				if ((*rlt_func = strdup(func0)) == NULL)
					err(EXIT_FAILURE, "strdup");
			}
			goto done;
		}

		/*
		 * If DW_AT_name is not present, but DW_AT_specification is
		 * present, then probably the actual name is in the DIE
		 * referenced by DW_AT_specification.
		 */
		if (dwarf_attr(die, DW_AT_specification, &spec_at, &de))
			goto done;
		if (dwarf_global_formref(spec_at, &ref, &de))
			goto done;
		if (dwarf_offdie(dbg, ref, &spec_die, &de))
			goto done;
		if (dwarf_attrval_string(spec_die, DW_AT_name, &func1, &de) ==
		    DW_DLV_OK) {
			free(*rlt_func);
			if ((*rlt_func = strdup(func1)) == NULL)
			    err(EXIT_FAILURE, "strdup");
		}

		goto done;
	}

cont_search:

	/* Search children. */
	ret = dwarf_child(die, &ret_die, &de);
	if (ret == DW_DLV_ERROR)
		errx(EXIT_FAILURE, "dwarf_child: %s", dwarf_errmsg(de));
	else if (ret == DW_DLV_OK)
		search_func(dbg, ret_die, addr, rlt_func);

	/* Search sibling. */
	ret = dwarf_siblingof(dbg, die, &ret_die, &de);
	if (ret == DW_DLV_ERROR)
		errx(EXIT_FAILURE, "dwarf_siblingof: %s", dwarf_errmsg(de));
	else if (ret == DW_DLV_OK)
		search_func(dbg, ret_die, addr, rlt_func);

done:
	dwarf_dealloc(dbg, die, DW_DLA_DIE);
}

static void
translate(Dwarf_Debug dbg, const char* addrstr)
{
	Dwarf_Die die, ret_die;
	Dwarf_Line *lbuf;
	Dwarf_Error de;
	Dwarf_Half tag;
	Dwarf_Unsigned lopc, hipc, addr, lineno, plineno;
	Dwarf_Signed lcount;
	Dwarf_Addr lineaddr, plineaddr;
	char *funcname;
	char *file, *file0, *pfile;
	char demangled[1024];
	int i, ret;

	addr = strtoull(addrstr, NULL, 16);
	addr += section_base;
	lineno = 0;
	file = unknown;
	lbuf = NULL;
	lcount = 0;

	while ((ret = dwarf_next_cu_header(dbg, NULL, NULL, NULL, NULL, NULL,
	    &de)) ==  DW_DLV_OK) {
		die = NULL;
		while (dwarf_siblingof(dbg, die, &ret_die, &de) == DW_DLV_OK) {
			if (die != NULL)
				dwarf_dealloc(dbg, die, DW_DLA_DIE);
			die = ret_die;
			if (dwarf_tag(die, &tag, &de) != DW_DLV_OK) {
				warnx("dwarf_tag failed: %s",
				    dwarf_errmsg(de));
				goto next_cu;
			}

			/* XXX: What about DW_TAG_partial_unit? */
			if (tag == DW_TAG_compile_unit)
				break;
		}
		if (ret_die == NULL) {
			warnx("could not find DW_TAG_compile_unit die");
			goto next_cu;
		}
		if (!dwarf_attrval_unsigned(die, DW_AT_low_pc, &lopc, &de) &&
		    !dwarf_attrval_unsigned(die, DW_AT_high_pc, &hipc, &de)) {
			/*
			 * Check if the address falls into the PC range of
			 * this CU.
			 */
			if (handle_high_pc(die, lopc, &hipc) != DW_DLV_OK)
				goto next_cu;
			if (addr < lopc || addr >= hipc)
				goto next_cu;
		}

		switch (dwarf_srclines(die, &lbuf, &lcount, &de)) {
		case DW_DLV_OK:
			break;
		case DW_DLV_NO_ENTRY:
			/* If a CU lacks debug info, just skip it. */
			goto next_cu;
		default:
			warnx("dwarf_srclines: %s", dwarf_errmsg(de));
			goto out;
		}

		plineaddr = ~0ULL;
		plineno = 0;
		pfile = unknown;
		for (i = 0; i < lcount; i++) {
			if (dwarf_lineaddr(lbuf[i], &lineaddr, &de)) {
				warnx("dwarf_lineaddr: %s",
				    dwarf_errmsg(de));
				goto out;
			}
			if (dwarf_lineno(lbuf[i], &lineno, &de)) {
				warnx("dwarf_lineno: %s",
				    dwarf_errmsg(de));
				goto out;
			}
			if (dwarf_linesrc(lbuf[i], &file0, &de)) {
				warnx("dwarf_linesrc: %s",
				    dwarf_errmsg(de));
			} else
				file = file0;
			if (addr == lineaddr)
				goto out;
			else if (addr < lineaddr && addr > plineaddr) {
				lineno = plineno;
				file = pfile;
				goto out;
			}
			plineaddr = lineaddr;
			plineno = lineno;
			pfile = file;
		}
	next_cu:
		if (die != NULL) {
			dwarf_dealloc(dbg, die, DW_DLA_DIE);
			die = NULL;
		}
	}

out:
	funcname = NULL;
	if (ret == DW_DLV_OK && func) {
		search_func(dbg, die, addr, &funcname);
		die = NULL;
	}

	if (func) {
		if (funcname == NULL)
			if ((funcname = strdup(unknown)) == NULL)
				err(EXIT_FAILURE, "strdup");
		if (demangle &&
		    !elftc_demangle(funcname, demangled, sizeof(demangled), 0))
			printf("%s\n", demangled);
		else
			printf("%s\n", funcname);
		free(funcname);
	}

	(void) printf("%s:%ju\n", base ? basename(file) : file, lineno);

	if (die != NULL)
		dwarf_dealloc(dbg, die, DW_DLA_DIE);

	/*
	 * Reset internal CU pointer, so we will start from the first CU
	 * next round.
	 */
	while (ret != DW_DLV_NO_ENTRY) {
		if (ret == DW_DLV_ERROR)
			errx(EXIT_FAILURE, "dwarf_next_cu_header: %s",
			    dwarf_errmsg(de));
		ret = dwarf_next_cu_header(dbg, NULL, NULL, NULL, NULL, NULL,
		    &de);
	}
}

static void
find_section_base(const char *exe, Elf *e, const char *section)
{
	Dwarf_Addr off;
	Elf_Scn *scn;
	GElf_Ehdr eh;
	GElf_Shdr sh;
	size_t shstrndx;
	int elferr;
	const char *name;

	if (gelf_getehdr(e, &eh) != &eh) {
		warnx("gelf_getehdr failed: %s", elf_errmsg(-1));
		return;
	}

	if (!elf_getshstrndx(e, &shstrndx)) {
		warnx("elf_getshstrndx failed: %s", elf_errmsg(-1));
		return;
	}

	(void) elf_errno();
	off = 0;
	scn = NULL;
	while ((scn = elf_nextscn(e, scn)) != NULL) {
		if (gelf_getshdr(scn, &sh) == NULL) {
			warnx("gelf_getshdr failed: %s", elf_errmsg(-1));
			continue;
		}
		if ((name = elf_strptr(e, shstrndx, sh.sh_name)) == NULL)
			goto next;
		if (!strcmp(section, name)) {
			if (eh.e_type == ET_EXEC || eh.e_type == ET_DYN) {
				/*
				 * For executables, section base is the virtual
				 * address of the specified section.
				 */
				section_base = sh.sh_addr;
			} else if (eh.e_type == ET_REL) {
				/*
				 * For relocatables, section base is the
				 * relative offset of the specified section
				 * to the start of the first section.
				 */
				section_base = off;
			} else
				warnx("unknown e_type %u", eh.e_type);
			return;
		}
	next:
		off += sh.sh_size;
	}
	elferr = elf_errno();
	if (elferr != 0)
		warnx("elf_nextscn failed: %s", elf_errmsg(elferr));

	errx(EXIT_FAILURE, "%s: cannot find section %s", exe, section);
}

int
main(int argc, char **argv)
{
	Elf *e;
	Dwarf_Debug dbg;
	Dwarf_Error de;
	const char *exe, *section;
	char line[1024];
	int fd, i, opt;

	exe = NULL;
	section = NULL;
	while ((opt = getopt_long(argc, argv, "b:Ce:fj:sHV", longopts, NULL)) !=
	    -1) {
		switch (opt) {
		case 'b':
			/* ignored */
			break;
		case 'C':
			demangle = 1;
			break;
		case 'e':
			exe = optarg;
			break;
		case 'f':
			func = 1;
			break;
		case 'j':
			section = optarg;
			break;
		case 's':
			base = 1;
			break;
		case 'H':
			usage();
		case 'V':
			version();
		default:
			usage();
		}
	}

	argv += optind;
	argc -= optind;

	if (exe == NULL)
		exe = "a.out";

	if ((fd = open(exe, O_RDONLY)) < 0)
		err(EXIT_FAILURE, "%s", exe);

	if (dwarf_init(fd, DW_DLC_READ, NULL, NULL, &dbg, &de))
		errx(EXIT_FAILURE, "dwarf_init: %s", dwarf_errmsg(de));

	if (dwarf_get_elf(dbg, &e, &de) != DW_DLV_OK)
		errx(EXIT_FAILURE, "dwarf_get_elf: %s", dwarf_errmsg(de));

	if (section)
		find_section_base(exe, e, section);
	else
		section_base = 0;

	if (argc > 0)
		for (i = 0; i < argc; i++)
			translate(dbg, argv[i]);
	else
		while (fgets(line, sizeof(line), stdin) != NULL) {
			translate(dbg, line);
			fflush(stdout);
		}

	dwarf_finish(dbg, &de);

	(void) elf_end(e);

	exit(0);
}
