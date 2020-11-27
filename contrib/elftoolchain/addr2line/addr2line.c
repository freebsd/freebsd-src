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
#include <sys/tree.h>

#include <capsicum_helpers.h>
#include <dwarf.h>
#include <err.h>
#include <fcntl.h>
#include <gelf.h>
#include <getopt.h>
#include <libdwarf.h>
#include <libelftc.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "_elftc.h"

ELFTC_VCSID("$Id: addr2line.c 3499 2016-11-25 16:06:29Z emaste $");

struct Func {
	char *name;
	Dwarf_Unsigned lopc;
	Dwarf_Unsigned hipc;
	Dwarf_Unsigned call_file;
	Dwarf_Unsigned call_line;
	Dwarf_Ranges *ranges;
	Dwarf_Signed ranges_cnt;
	struct Func *inlined_caller;
	STAILQ_ENTRY(Func) next;
};

struct range {
	RB_ENTRY(range) entry;
	Dwarf_Off off;
	Dwarf_Unsigned lopc;
	Dwarf_Unsigned hipc;
	char **srcfiles;
	Dwarf_Signed nsrcfiles;
	STAILQ_HEAD(, Func) funclist;
	Dwarf_Die die;
	Dwarf_Debug dbg;
};

static struct option longopts[] = {
	{"addresses", no_argument, NULL, 'a'},
	{"target" , required_argument, NULL, 'b'},
	{"demangle", no_argument, NULL, 'C'},
	{"exe", required_argument, NULL, 'e'},
	{"functions", no_argument, NULL, 'f'},
	{"inlines", no_argument, NULL, 'i'},
	{"section", required_argument, NULL, 'j'},
	{"pretty-print", no_argument, NULL, 'p'},
	{"basename", no_argument, NULL, 's'},
	{"help", no_argument, NULL, 'H'},
	{"version", no_argument, NULL, 'V'},
	{NULL, 0, NULL, 0}
};

static int demangle, func, base, inlines, print_addr, pretty_print;
static char unknown[] = { '?', '?', '\0' };
static Dwarf_Addr section_base;
/* Need a new curlopc that stores last lopc value. */
static Dwarf_Unsigned curlopc = ~0ULL;
static RB_HEAD(cutree, range) cuhead = RB_INITIALIZER(&cuhead);

static int
lopccmp(struct range *e1, struct range *e2)
{
	return (e1->lopc < e2->lopc ? -1 : e1->lopc > e2->lopc);
}

RB_PROTOTYPE(cutree, range, entry, lopccmp);
RB_GENERATE(cutree, range, entry, lopccmp)

#define	USAGE_MESSAGE	"\
Usage: %s [options] hexaddress...\n\
  Map program addresses to source file names and line numbers.\n\n\
  Options:\n\
  -a      | --addresses       Display address prior to line number info.\n\
  -b TGT  | --target=TGT      (Accepted but ignored).\n\
  -e EXE  | --exe=EXE         Use program \"EXE\" to translate addresses.\n\
  -f      | --functions       Display function names.\n\
  -i      | --inlines         Display caller info for inlined functions.\n\
  -j NAME | --section=NAME    Values are offsets into section \"NAME\".\n\
  -p      | --pretty-print    Display line number info and function name\n\
                              in human readable manner.\n\
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

static struct Func *
search_func(struct range *range, Dwarf_Unsigned addr)
{
	struct Func *f, *f0;
	Dwarf_Unsigned lopc, hipc, addr_base;
	int i;

	f0 = NULL;

	STAILQ_FOREACH(f, &range->funclist, next) {
		if (f->ranges != NULL) {
			addr_base = 0;
			for (i = 0; i < f->ranges_cnt; i++) {
				if (f->ranges[i].dwr_type == DW_RANGES_END)
					break;
				if (f->ranges[i].dwr_type ==
				    DW_RANGES_ADDRESS_SELECTION) {
					addr_base = f->ranges[i].dwr_addr2;
					continue;
				}

				/* DW_RANGES_ENTRY */
				lopc = f->ranges[i].dwr_addr1 + addr_base;
				hipc = f->ranges[i].dwr_addr2 + addr_base;
				if (addr >= lopc && addr < hipc) {
					if (f0 == NULL ||
					    (lopc >= f0->lopc &&
					    hipc <= f0->hipc)) {
						f0 = f;
						f0->lopc = lopc;
						f0->hipc = hipc;
						break;
					}
				}
			}
		} else if (addr >= f->lopc && addr < f->hipc) {
			if (f0 == NULL ||
			    (f->lopc >= f0->lopc && f->hipc <= f0->hipc))
				f0 = f;
		}
	}

	return (f0);
}

static void
collect_func(Dwarf_Debug dbg, Dwarf_Die die, struct Func *parent,
    struct range *range)
{
	Dwarf_Die ret_die, abst_die, spec_die;
	Dwarf_Error de;
	Dwarf_Half tag;
	Dwarf_Unsigned lopc, hipc, ranges_off;
	Dwarf_Signed ranges_cnt;
	Dwarf_Off ref;
	Dwarf_Attribute abst_at, spec_at;
	Dwarf_Ranges *ranges;
	const char *funcname;
	struct Func *f;
	int found_ranges, ret;

	f = NULL;
	abst_die = spec_die = NULL;

	if (dwarf_tag(die, &tag, &de)) {
		warnx("dwarf_tag: %s", dwarf_errmsg(de));
		goto cont_search;
	}
	if (tag == DW_TAG_subprogram || tag == DW_TAG_entry_point ||
	    tag == DW_TAG_inlined_subroutine || tag == DW_TAG_label) {
		/*
		 * Function address range can be specified by either
		 * a DW_AT_ranges attribute which points to a range list or
		 * by a pair of DW_AT_low_pc and DW_AT_high_pc attributes.
		 */
		ranges = NULL;
		ranges_cnt = 0;
		found_ranges = 0;
		if (dwarf_attrval_unsigned(die, DW_AT_ranges, &ranges_off,
		    &de) == DW_DLV_OK &&
		    dwarf_get_ranges(dbg, (Dwarf_Off) ranges_off, &ranges,
		    &ranges_cnt, NULL, &de) == DW_DLV_OK) {
			if (ranges != NULL && ranges_cnt > 0) {
				found_ranges = 1;
				goto get_func_name;
			}
		}

		/*
		 * Ranges pointer not found.  Search for DW_AT_low_pc, and
		 * DW_AT_high_pc iff die is not a label.  Labels doesn't have
		 * hipc attr. */
		if (tag == DW_TAG_label) {
			if (dwarf_attrval_unsigned(die, DW_AT_low_pc, &lopc,
			    &de) != DW_DLV_OK)
				goto cont_search;
		} else {
			if (dwarf_attrval_unsigned(die, DW_AT_low_pc, &lopc,
			    &de) || dwarf_attrval_unsigned(die, DW_AT_high_pc,
			    &hipc, &de))
				goto cont_search;
			if (handle_high_pc(die, lopc, &hipc) != DW_DLV_OK)
				goto cont_search;
		}

	get_func_name:
		/*
		 * Most common case the function name is stored in DW_AT_name
		 * attribute.
		 */
		if (dwarf_attrval_string(die, DW_AT_name, &funcname, &de) ==
		    DW_DLV_OK)
			goto add_func;

		/*
		 * For inlined function, the actual name is probably in the DIE
		 * referenced by DW_AT_abstract_origin. (if present)
		 */
		if (dwarf_attr(die, DW_AT_abstract_origin, &abst_at, &de) ==
		    DW_DLV_OK &&
		    dwarf_global_formref(abst_at, &ref, &de) == DW_DLV_OK &&
		    dwarf_offdie(dbg, ref, &abst_die, &de) == DW_DLV_OK &&
		    dwarf_attrval_string(abst_die, DW_AT_name, &funcname,
		    &de) == DW_DLV_OK)
			goto add_func;

		/*
		 * If DW_AT_name is not present, but DW_AT_specification is
		 * present, then probably the actual name is in the DIE
		 * referenced by DW_AT_specification.
		 */
		if (dwarf_attr(die, DW_AT_specification, &spec_at, &de) ==
		    DW_DLV_OK &&
		    dwarf_global_formref(spec_at, &ref, &de) == DW_DLV_OK &&
		    dwarf_offdie(dbg, ref, &spec_die, &de) == DW_DLV_OK &&
		    dwarf_attrval_string(spec_die, DW_AT_name, &funcname,
		    &de) == DW_DLV_OK)
			goto add_func;

		/* Skip if no name associated with this DIE. */
		goto cont_search;

	add_func:
		if ((f = calloc(1, sizeof(*f))) == NULL)
			err(EXIT_FAILURE, "calloc");
		if ((f->name = strdup(funcname)) == NULL)
			err(EXIT_FAILURE, "strdup");
		if (found_ranges) {
			f->ranges = ranges;
			f->ranges_cnt = ranges_cnt;
		} else {
			f->lopc = lopc;
			f->hipc = hipc;
		}
		if (tag == DW_TAG_inlined_subroutine) {
			f->inlined_caller = parent;
			dwarf_attrval_unsigned(die, DW_AT_call_file,
			    &f->call_file, &de);
			dwarf_attrval_unsigned(die, DW_AT_call_line,
			    &f->call_line, &de);
		}
		STAILQ_INSERT_TAIL(&range->funclist, f, next);
	}

cont_search:

	/* Search children. */
	ret = dwarf_child(die, &ret_die, &de);
	if (ret == DW_DLV_ERROR)
		warnx("dwarf_child: %s", dwarf_errmsg(de));
	else if (ret == DW_DLV_OK) {
		if (f != NULL)
			collect_func(dbg, ret_die, f, range);
		else
			collect_func(dbg, ret_die, parent, range);
	}

	/* Search sibling. */
	ret = dwarf_siblingof(dbg, die, &ret_die, &de);
	if (ret == DW_DLV_ERROR)
		warnx("dwarf_siblingof: %s", dwarf_errmsg(de));
	else if (ret == DW_DLV_OK)
		collect_func(dbg, ret_die, parent, range);

	/* Cleanup */
	if (die != range->die)
		dwarf_dealloc(dbg, die, DW_DLA_DIE);

	if (abst_die != NULL)
		dwarf_dealloc(dbg, abst_die, DW_DLA_DIE);

	if (spec_die != NULL)
		dwarf_dealloc(dbg, spec_die, DW_DLA_DIE);
}

static void
print_inlines(struct range *range, struct Func *f, Dwarf_Unsigned call_file,
    Dwarf_Unsigned call_line)
{
	char demangled[1024];
	char *file;

	if (call_file > 0 && (Dwarf_Signed) call_file <= range->nsrcfiles)
		file = range->srcfiles[call_file - 1];
	else
		file = unknown;

	if (pretty_print)
		printf(" (inlined by) ");

	if (func) {
		if (demangle && !elftc_demangle(f->name, demangled,
		    sizeof(demangled), 0)) {
			if (pretty_print)
				printf("%s at ", demangled);
			else
				printf("%s\n", demangled);
		} else {
			if (pretty_print)
				printf("%s at ", f->name);
			else
				printf("%s\n", f->name);
		}
	}
	(void) printf("%s:%ju\n", base ? basename(file) : file,
	    (uintmax_t) call_line);

	if (f->inlined_caller != NULL)
		print_inlines(range, f->inlined_caller, f->call_file,
		    f->call_line);
}

static struct range *
culookup(Dwarf_Unsigned addr)
{
	struct range find, *res;

	find.lopc = addr;
	res = RB_NFIND(cutree, &cuhead, &find);
	if (res != NULL) {
		if (res->lopc != addr)
			res = RB_PREV(cutree, &cuhead, res);
		if (res != NULL && addr >= res->lopc && addr < res->hipc)
			return (res);
	} else {
		res = RB_MAX(cutree, &cuhead);
		if (res != NULL && addr >= res->lopc && addr < res->hipc)
			return (res);
	}
	return (NULL);
}

/*
 * When DW_AT_ranges, DW_AT_low_pc/DW_AT_high_pc are all absent, we check the
 * children of cu die for labels.  If the address falls into one of the labels
 * ranges(aranges), return the label DIE.
 */
static int
check_labels(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Unsigned addr,
    struct range **range) {
	Dwarf_Addr start;
	Dwarf_Arange *aranges;
	Dwarf_Die prev_die, ret_die;
	Dwarf_Error de;
	Dwarf_Half tag;
	Dwarf_Off die_off;
	Dwarf_Unsigned lopc, length;
	Dwarf_Signed arcnt;
	struct range *labelp, **labels;
	int i, j, label_cnt, ret;

	prev_die = ret_die = NULL;
	labels = NULL;
	i = label_cnt = 0;

	/* Find aranges. */
	ret = dwarf_get_aranges(dbg, &aranges, &arcnt, &de);
	if (ret != DW_DLV_OK && ret != DW_DLV_NO_ENTRY)
		warnx("dwarf_get_aranges failed: %s", dwarf_errmsg(de));

	/* Child of current CU. */
	ret = dwarf_child(die, &prev_die, &de);
	if (ret == DW_DLV_ERROR)
		warnx("dwarf_child: %s", dwarf_errmsg(de));

	/* Count labels. */
	while (1) {
		if (dwarf_tag(prev_die, &tag, &de) != DW_DLV_OK) {
			warnx("dwarf_tag failed: %s",
				dwarf_errmsg(de));
			return DW_DLV_ERROR;
		}
		if (tag == DW_TAG_label) {
			if (dwarf_attrval_unsigned(prev_die, DW_AT_low_pc,
			    &lopc, &de) == DW_DLV_OK)
				label_cnt++;
		}

		if (dwarf_siblingof(dbg, prev_die, &ret_die, &de) != DW_DLV_OK)
			break;

		if (prev_die != NULL)
			dwarf_dealloc(dbg, prev_die, DW_DLA_DIE);
		prev_die = ret_die;
	}

	if (label_cnt == 0)
		return (DW_DLV_NO_ENTRY);

	/* Allocate space for labels. */
	if ((labels = calloc(label_cnt, sizeof(struct range *))) == NULL)
		err(EXIT_FAILURE, "calloc");

	/* Add labels to list. */
	ret = dwarf_child(die, &prev_die, &de);
	if (ret == DW_DLV_ERROR)
		warnx("dwarf_child: %s", dwarf_errmsg(de));
	while (1) {
		if (dwarf_tag(prev_die, &tag, &de) != DW_DLV_OK) {
			warnx("dwarf_tag failed: %s",
				dwarf_errmsg(de));
			return DW_DLV_ERROR;
		}
		if (tag == DW_TAG_label) {
			if (dwarf_attrval_unsigned(prev_die, DW_AT_low_pc,
			    &lopc, &de) == DW_DLV_OK) {
				if (curlopc == lopc) {
					for (i = 0; i < label_cnt - 1; i++) {
						if (labels[i] != *range)
							free(labels[i]);
					}
					free(labels);
					return DW_DLV_ERROR;
				}
				labelp = calloc(1, sizeof(struct range));
				if (labelp == NULL)
					err(EXIT_FAILURE, "calloc");
				labelp->lopc = lopc;
				labelp->die = prev_die;
				labelp->dbg = dbg;
				STAILQ_INIT(&labelp->funclist);
				labels[i++] = labelp;
			}
		}
		if (dwarf_siblingof(dbg, prev_die, &ret_die, &de) != DW_DLV_OK)
			break;
		if (prev_die != NULL && tag != DW_TAG_label)
			dwarf_dealloc(dbg, prev_die, DW_DLA_DIE);
		prev_die = ret_die;
	}

	/* Set hipc for each label using aranges */
	for (i = 0; i < label_cnt; i++) {
		for (j = 0; j < arcnt; j++) {
			if (dwarf_get_arange_info(aranges[j], &start, &length,
			    &die_off, &de) != DW_DLV_OK) {
				warnx("dwarf_get_arange_info failed: %s",
					dwarf_errmsg(de));
				continue;
			}
			if (labels[i]->lopc == (Dwarf_Unsigned)start) {
				labels[i]->hipc = start + length;
				break;
			}
		}
	}

	/* If addr in label's range, we have found the range for this label. */
	for (i = 0; i < label_cnt; i++) {
		if (addr >= labels[i]->lopc && addr < labels[i]->hipc) {
			*range = labels[i];
			RB_INSERT(cutree, &cuhead, (*range));
			curlopc = (*range)->lopc;
			break;
		}
	}

	for (i = 0; i < label_cnt - 1; i++) {
		if (labels[i] != *range)
			free(labels[i]);
	}
	free(labels);

	if (*range != NULL)
		return (DW_DLV_OK);
	else
		return (DW_DLV_NO_ENTRY);
}

/*
 * Check whether addr falls into range(s) of current CU.
 * If so, save current CU to lookup tree.
 */
static int
check_range(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Unsigned addr,
    struct range **range)
{
	Dwarf_Error de;
	Dwarf_Unsigned addr_base, lopc, hipc;
	Dwarf_Off ranges_off;
	Dwarf_Signed ranges_cnt;
	Dwarf_Ranges *ranges;
	int i, ret;
	bool in_cu;

	addr_base = 0;
	ranges = NULL;
	ranges_cnt = 0;
	in_cu = false;

	ret = dwarf_attrval_unsigned(die, DW_AT_ranges, &ranges_off, &de);
	if (ret == DW_DLV_OK) {
		ret = dwarf_get_ranges(dbg, ranges_off, &ranges,
			&ranges_cnt, NULL, &de);
		if (ret != DW_DLV_OK)
			return (ret);

		if (!ranges || ranges_cnt <= 0)
			return (DW_DLV_ERROR);

		for (i = 0; i < ranges_cnt; i++) {
			if (ranges[i].dwr_type == DW_RANGES_END)
				return (DW_DLV_NO_ENTRY);

			if (ranges[i].dwr_type ==
				DW_RANGES_ADDRESS_SELECTION) {
				addr_base = ranges[i].dwr_addr2;
				continue;
			}

			/* DW_RANGES_ENTRY */
			lopc = ranges[i].dwr_addr1 + addr_base;
			hipc = ranges[i].dwr_addr2 + addr_base;

			if (lopc == curlopc)
				return (DW_DLV_ERROR);

			if (addr >= lopc && addr < hipc){
				in_cu = true;
				break;
			}
		}
	} else {
		if (dwarf_attrval_unsigned(die, DW_AT_low_pc, &lopc, &de) ==
		    DW_DLV_OK) {
			if (lopc == curlopc)
				return (DW_DLV_ERROR);
			if (dwarf_attrval_unsigned(die, DW_AT_high_pc, &hipc,
				&de) == DW_DLV_OK) {
				/*
				 * Check if the address falls into the PC
				 * range of this CU.
				 */
				if (handle_high_pc(die, lopc, &hipc) !=
					DW_DLV_OK)
					return (DW_DLV_ERROR);
			} else {
				/* Assume ~0ULL if DW_AT_high_pc not present. */
				hipc = ~0ULL;
			}

			if (addr >= lopc && addr < hipc) {
				in_cu = true;
			}
		} else {
			/* Addr not in range die, try labels. */
			ret = check_labels(dbg, die, addr, range);
			return ret;
		}
	}

	if (in_cu) {
		if ((*range = calloc(1, sizeof(struct range))) == NULL)
			err(EXIT_FAILURE, "calloc");
		(*range)->lopc = lopc;
		(*range)->hipc = hipc;
		(*range)->die = die;
		(*range)->dbg = dbg;
		STAILQ_INIT(&(*range)->funclist);
		RB_INSERT(cutree, &cuhead, *range);
		curlopc = lopc;
		return (DW_DLV_OK);
	} else {
		return (DW_DLV_NO_ENTRY);
	}
}

static void
translate(Dwarf_Debug dbg, Elf *e, const char* addrstr)
{
	Dwarf_Die die, ret_die;
	Dwarf_Line *lbuf;
	Dwarf_Error de;
	Dwarf_Half tag;
	Dwarf_Unsigned addr, lineno, plineno;
	Dwarf_Signed lcount;
	Dwarf_Addr lineaddr, plineaddr;
	struct range *range;
	struct Func *f;
	const char *funcname;
	char *file, *file0, *pfile;
	char demangled[1024];
	int ec, i, ret;

	addr = strtoull(addrstr, NULL, 16);
	addr += section_base;
	lineno = 0;
	file = unknown;
	die = NULL;
	ret = DW_DLV_OK;

	range = culookup(addr);
	if (range != NULL) {
		die = range->die;
		dbg = range->dbg;
		goto status_ok;
	}

	while (true) {
		/*
		 * We resume the CU scan from the last place we found a match.
		 * Because when we have 2 sequential addresses, and the second
		 * one is of the next CU, it is faster to just go to the next CU
		 * instead of starting from the beginning.
		 */
		ret = dwarf_next_cu_header(dbg, NULL, NULL, NULL, NULL, NULL,
		    &de);
		if (ret == DW_DLV_NO_ENTRY) {
			if (curlopc == ~0ULL)
				goto out;
			ret = dwarf_next_cu_header(dbg, NULL, NULL, NULL, NULL,
			    NULL, &de);
		}
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
		ret = check_range(dbg, die, addr, &range);
		if (ret == DW_DLV_OK)
			break;
		if (ret == DW_DLV_ERROR)
			goto out;
next_cu:
		if (die != NULL) {
			dwarf_dealloc(dbg, die, DW_DLA_DIE);
			die = NULL;
		}
	}

	if (ret != DW_DLV_OK || die == NULL)
		goto out;

status_ok:
	switch (dwarf_srclines(die, &lbuf, &lcount, &de)) {
	case DW_DLV_OK:
		break;
	case DW_DLV_NO_ENTRY:
		/* If a CU lacks debug info, just skip it. */
		goto out;
	default:
		warnx("dwarf_srclines: %s", dwarf_errmsg(de));
		goto out;
	}

	plineaddr = ~0ULL;
	plineno = 0;
	pfile = unknown;
	for (i = 0; i < lcount; i++) {
		if (dwarf_lineaddr(lbuf[i], &lineaddr, &de)) {
			warnx("dwarf_lineaddr: %s", dwarf_errmsg(de));
			goto out;
		}
		if (dwarf_lineno(lbuf[i], &lineno, &de)) {
			warnx("dwarf_lineno: %s", dwarf_errmsg(de));
			goto out;
		}
		if (dwarf_linesrc(lbuf[i], &file0, &de)) {
			warnx("dwarf_linesrc: %s", dwarf_errmsg(de));
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

out:
	f = NULL;
	funcname = NULL;
	if (ret == DW_DLV_OK && (func || inlines) && range != NULL) {
		if (range->srcfiles == NULL)
			if (dwarf_srcfiles(die, &range->srcfiles,
			    &range->nsrcfiles, &de))
				warnx("dwarf_srcfiles: %s", dwarf_errmsg(de));
		if (STAILQ_EMPTY(&range->funclist)) {
			collect_func(dbg, range->die, NULL, range);
			die = NULL;
		}
		f = search_func(range, addr);
		if (f != NULL)
			funcname = f->name;
	}

	if (print_addr) {
		if ((ec = gelf_getclass(e)) == ELFCLASSNONE) {
			warnx("gelf_getclass failed: %s", elf_errmsg(-1));
			ec = ELFCLASS64;
		}
		if (ec == ELFCLASS32) {
			if (pretty_print)
				printf("0x%08jx: ", (uintmax_t) addr);
			else
				printf("0x%08jx\n", (uintmax_t) addr);
		} else {
			if (pretty_print)
				printf("0x%016jx: ", (uintmax_t) addr);
			else
				printf("0x%016jx\n", (uintmax_t) addr);
		}
	}

	if (func) {
		if (funcname == NULL)
			funcname = unknown;
		if (demangle && !elftc_demangle(funcname, demangled,
		    sizeof(demangled), 0)) {
			if (pretty_print)
				printf("%s at ", demangled);
			else
				printf("%s\n", demangled);
		} else {
			if (pretty_print)
				printf("%s at ", funcname);
			else
				printf("%s\n", funcname);
		}
	}

	(void) printf("%s:%ju\n", base ? basename(file) : file,
	    (uintmax_t) lineno);

	if (ret == DW_DLV_OK && inlines && range != NULL &&
	    range->srcfiles != NULL && f != NULL && f->inlined_caller != NULL)
		print_inlines(range, f->inlined_caller, f->call_file,
		    f->call_line);
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
	cap_rights_t rights;
	Elf *e;
	Dwarf_Debug dbg;
	Dwarf_Error de;
	const char *exe, *section;
	char line[1024];
	int fd, i, opt;

	exe = NULL;
	section = NULL;
	while ((opt = getopt_long(argc, argv, "ab:Ce:fij:psHV", longopts,
	    NULL)) != -1) {
		switch (opt) {
		case 'a':
			print_addr = 1;
			break;
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
		case 'i':
			inlines = 1;
			break;
		case 'j':
			section = optarg;
			break;
		case 'p':
			pretty_print = 1;
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

	if (caph_rights_limit(fd, cap_rights_init(&rights, CAP_FSTAT,
	    CAP_MMAP_R)) < 0)
		errx(EXIT_FAILURE, "caph_rights_limit");

	caph_cache_catpages();
	if (caph_limit_stdio() < 0)
		errx(EXIT_FAILURE, "failed to limit stdio rights");
	if (caph_enter() < 0)
		errx(EXIT_FAILURE, "failed to enter capability mode");

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
			translate(dbg, e, argv[i]);
	else {
		setvbuf(stdout, NULL, _IOLBF, 0);
		while (fgets(line, sizeof(line), stdin) != NULL)
			translate(dbg, e, line);
	}

	dwarf_finish(dbg, &de);

	(void) elf_end(e);

	exit(0);
}
