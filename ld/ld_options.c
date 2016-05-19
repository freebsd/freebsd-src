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
#include "ld_file.h"
#include "ld_path.h"
#include "ld_script.h"
#include "ld_symbols.h"
#include "ld_options.h"
#include "ld_output.h"

ELFTC_VCSID("$Id: ld_options.c 3406 2016-02-14 17:45:43Z jkoshy $");

/*
 * Support routines for parsing command line options.
 */

static const char *ld_short_opts =
    "b:c:de:Ef:Fgh:iI:l:L:m:MnNo:O::qrR:sStT:xXyY:u:vV()";

static struct ld_option ld_opts[] = {
	{"aarchive", KEY_STATIC, ONE_DASH, NO_ARG},
	{"adefault", KEY_DYNAMIC, ONE_DASH, NO_ARG},
	{"ashared", KEY_DYNAMIC, ONE_DASH, NO_ARG},
	{"accept-unknown-input-arch", KEY_ACCEPT_UNKNOWN, ANY_DASH, NO_ARG},
	{"allow-multiple-definition", KEY_Z_MULDEFS, ANY_DASH, NO_ARG},
	{"allow-shlib-undefined", KEY_ALLOW_SHLIB_UNDEF, ANY_DASH, NO_ARG},
	{"assert", KEY_ASSERT, ANY_DASH, NO_ARG},
	{"as-needed", KEY_AS_NEEDED, ANY_DASH, NO_ARG},
	{"auxiliary", 'f', ANY_DASH, REQ_ARG},
	{"build-id", KEY_BUILD_ID, ANY_DASH, OPT_ARG},
	{"call_shared", KEY_DYNAMIC, ONE_DASH, NO_ARG},
	{"check-sections", KEY_CHECK_SECTIONS, ANY_DASH, NO_ARG},
	{"cref", KEY_CREF, ANY_DASH, NO_ARG},
	{"defsym", KEY_DEFSYM, ANY_DASH, REQ_ARG},
	{"demangle", KEY_DEMANGLE, ANY_DASH, OPT_ARG},
	{"dc", 'd', ONE_DASH, NO_ARG},
	{"dp", 'd', ONE_DASH, NO_ARG},
	{"disable-new-dtags", KEY_DISABLE_NEW_DTAGS, ANY_DASH, NO_ARG},
	{"discard-all", 'x', ANY_DASH, NO_ARG},
	{"discard-locals", 'X', ANY_DASH, NO_ARG},
	{"dn", KEY_STATIC, ONE_DASH, NO_ARG},
	{"dy", KEY_DYNAMIC, ONE_DASH, NO_ARG},
	{"dynamic-linker", 'I', ANY_DASH, REQ_ARG},
	{"end-group", ')', ANY_DASH, NO_ARG},
	{"entry", 'e', ANY_DASH, REQ_ARG},
	{"error-unresolved-symbols", KEY_ERR_UNRESOLVE_SYM, ANY_DASH, NO_ARG},
	{"export-dynamic", 'E', ANY_DASH, NO_ARG},
	{"eh-frame-hdr", KEY_EH_FRAME_HDR, ANY_DASH, NO_ARG},
	{"emit-relocs", 'q', ANY_DASH, NO_ARG},
	{"emulation", 'm', ANY_DASH, REQ_ARG},
	{"enable-new-dtags", KEY_ENABLE_NEW_DTAGS, ANY_DASH, NO_ARG},
	{"fatal-warnings", KEY_FATAL_WARNINGS, ANY_DASH, NO_ARG},
	{"filter", 'F', ANY_DASH, NO_ARG},
	{"fini", KEY_FINI, ANY_DASH, NO_ARG},
	{"format", 'b', ANY_DASH, REQ_ARG},
	{"gc-sections", KEY_GC_SECTIONS, ANY_DASH, NO_ARG},
	{"hash-style", KEY_HASH_STYLE, ANY_DASH, REQ_ARG},
	{"help", KEY_HELP, ANY_DASH, NO_ARG},
	{"init", KEY_INIT, ANY_DASH, REQ_ARG},
	{"just-symbols", 'R', ANY_DASH, REQ_ARG},
	{"library", 'l', ANY_DASH, REQ_ARG},
	{"library-path", 'L', ANY_DASH, REQ_ARG},
	{"mri-script", 'c', ANY_DASH, REQ_ARG},
	{"nmagic", 'n', ANY_DASH, NO_ARG},
	{"nostdlib", KEY_NO_STDLIB, ONE_DASH, NO_ARG},
	{"no-accept-unknown-input-arch", KEY_NO_UNKNOWN, ANY_DASH, NO_ARG},
	{"no-allow-shlib-undefined", KEY_NO_SHLIB_UNDEF, ANY_DASH, NO_ARG},
	{"no-as-needed", KEY_NO_AS_NEEDED, ANY_DASH, NO_ARG},
	{"no-check-sections", KEY_NO_CHECK_SECTIONS, ANY_DASH, NO_ARG},
	{"no-define-common", KEY_NO_DEFINE_COMMON, ANY_DASH, NO_ARG},
	{"no-demangle", KEY_NO_DEMANGLE, ANY_DASH, OPT_ARG},
	{"no-gc-sections", KEY_NO_GC_SECTIONS, ANY_DASH, NO_ARG},
	{"no-keep-memory", KEY_NO_KEEP_MEMORY, ANY_DASH, NO_ARG},
	{"no-omagic", KEY_NO_OMAGIC, ANY_DASH, NO_ARG},
	{"no-print-gc-sections", KEY_NO_PRINT_GC_SECTIONS, ANY_DASH, NO_ARG},
	{"no-undefined", KEY_Z_DEFS, ANY_DASH, NO_ARG},
	{"no-undefined-version", KEY_NO_UNDEF_VERSION, ANY_DASH, NO_ARG},
	{"no-whole-archive", KEY_NO_WHOLE_ARCHIVE, ANY_DASH, NO_ARG},
	{"no-warn-mismatch", KEY_NO_WARN_MISMATCH, ANY_DASH, NO_ARG},
	{"non_shared", KEY_STATIC, ONE_DASH, NO_ARG},
	{"oformat", KEY_OFORMAT, TWO_DASH, REQ_ARG},
	{"omagic", 'N', TWO_DASH, NO_ARG},
	{"output", 'o', TWO_DASH, REQ_ARG},
	{"pic-executable", KEY_PIE, ANY_DASH, NO_ARG},
	{"pie", KEY_PIE, ONE_DASH, NO_ARG},
	{"print-gc-sections", KEY_PRINT_GC_SECTIONS, ANY_DASH, NO_ARG},
	{"print-map", 'M', ANY_DASH, NO_ARG},
	{"qmagic", KEY_QMAGIC, ANY_DASH, NO_ARG},
	{"relax", KEY_RELAX, ANY_DASH, NO_ARG},
	{"relocatable", 'r', ANY_DASH, NO_ARG},
	{"retain-symbols-file", KEY_RETAIN_SYM_FILE, ANY_DASH, REQ_ARG},
	{"rpath", KEY_RPATH, ANY_DASH, REQ_ARG},
	{"rpath-link", KEY_RPATH_LINK, ANY_DASH, REQ_ARG},
	{"runpath", KEY_RUNPATH, ANY_DASH, REQ_ARG},
	{"script", 'T', ANY_DASH, REQ_ARG},
	{"section-start", KEY_SECTION_START, ANY_DASH, REQ_ARG},
	{"shared", KEY_SHARED, ONE_DASH, NO_ARG},
	{"soname", 'h', ONE_DASH, REQ_ARG},
	{"sort-common", KEY_SORT_COMMON, ANY_DASH, NO_ARG},
	{"split-by-file", KEY_SPLIT_BY_FILE, ANY_DASH, REQ_ARG},
	{"split-by-reloc", KEY_SPLIT_BY_RELOC, ANY_DASH, REQ_ARG},
	{"start-group", '(', ANY_DASH, NO_ARG},
	{"stats", KEY_STATS, ANY_DASH, NO_ARG},
	{"static", KEY_STATIC, ONE_DASH, NO_ARG},
	{"strip-all", 's', ANY_DASH, NO_ARG},
	{"strip-debug", 'S', ANY_DASH, NO_ARG},
	{"trace", 't', ANY_DASH, NO_ARG},
	{"trace_symbol", 'y', ANY_DASH, NO_ARG},
	{"traditional-format", KEY_TRADITIONAL_FORMAT, ANY_DASH, NO_ARG},
	{"undefined", 'u', ANY_DASH, REQ_ARG},
	{"unique", KEY_UNIQUE, ANY_DASH, OPT_ARG},
	{"unresolved-symbols", KEY_UNRESOLVED_SYMBOLS, ANY_DASH, REQ_ARG},
	{"verbose" , KEY_VERBOSE, ANY_DASH, NO_ARG},
	{"version", 'V', ANY_DASH, NO_ARG},
	{"version-script", KEY_VERSION_SCRIPT, ANY_DASH, REQ_ARG},
	{"warn-common", KEY_WARN_COMMON, ANY_DASH, NO_ARG},
	{"warn-constructors", KEY_WARN_CONSTRUCTORS, ANY_DASH, NO_ARG},
	{"warn-multiple-gp", KEY_WARN_MULTIPLE_GP, ANY_DASH, NO_ARG},
	{"warn-once", KEY_WARN_ONCE, ANY_DASH, NO_ARG},
	{"warn-section-align", KEY_WARN_SECTION_ALIGN, ANY_DASH, NO_ARG},
	{"warn-shared-textrel", KEY_WARN_SHARED_TEXTREL, ANY_DASH, NO_ARG},
	{"warn-unresolved-symbols", KEY_WARN_UNRESOLVE_SYM, ANY_DASH, NO_ARG},
	{"whole-archive", KEY_WHOLE_ARCHIVE, ANY_DASH, NO_ARG},
	{"wrap", KEY_WRAP, ANY_DASH, REQ_ARG},
	{"EB", KEY_EB, ONE_DASH, NO_ARG},
	{"EL", KEY_EL, ONE_DASH, NO_ARG},
	{"Map", KEY_MAP, ONE_DASH, REQ_ARG},
	{"Qy", KEY_QY, ONE_DASH, NO_ARG},
	{"Tbss", KEY_TBSS, ONE_DASH, REQ_ARG},
	{"Tdata", KEY_TDATA, ONE_DASH, REQ_ARG},
	{"Ttext", KEY_TTEXT, ONE_DASH, REQ_ARG},
	{"Ur", KEY_UR, ONE_DASH, NO_ARG},
	{NULL, 0, 0, 0},
};

static struct ld_option ld_opts_B[] = {
	{"shareable", KEY_SHARED, ONE_DASH, NO_ARG},
	{"static", KEY_STATIC, ONE_DASH, NO_ARG},
	{"dynamic", KEY_DYNAMIC, ONE_DASH, NO_ARG},
	{"group", KEY_GROUP, ONE_DASH, NO_ARG},
	{"symbolic", KEY_SYMBOLIC, ONE_DASH, NO_ARG},
	{"symbolic_functions", KEY_SYMBOLIC_FUNC, ONE_DASH, NO_ARG},
};

static struct ld_option ld_opts_z[] = {
	{"nodefaultlib", KEY_Z_NO_DEFAULT_LIB, ONE_DASH, NO_ARG},
	{"allextract", KEY_WHOLE_ARCHIVE, ONE_DASH, NO_ARG},
	{"defaultextract", KEY_Z_DEFAULT_EXTRACT, ONE_DASH, NO_ARG},
	{"weakextract", KEY_Z_WEAK_EXTRACT, ONE_DASH, NO_ARG},
	{"muldefs", KEY_Z_MULDEFS, ONE_DASH, NO_ARG},
	{"defs", KEY_Z_DEFS, ONE_DASH, NO_ARG},
	{"execstack", KEY_Z_EXEC_STACK, ONE_DASH, NO_ARG},
	{"nodefs", KEY_Z_NO_DEFS, ONE_DASH, NO_ARG},
	{"origin", KEY_Z_ORIGIN, ONE_DASH, NO_ARG},
	{"now", KEY_Z_NOW, ONE_DASH, NO_ARG},
	{"nodelete", KEY_Z_NO_DELETE, ONE_DASH, NO_ARG},
	{"initfirst", KEY_Z_INIT_FIRST, ONE_DASH, NO_ARG},
	{"lazyload", KEY_Z_LAZYLOAD, ONE_DASH, NO_ARG},
	{"noexecstack", KEY_Z_NO_EXEC_STACK, ONE_DASH, NO_ARG},
	{"nodlopen", KEY_Z_NO_DLOPEN, ONE_DASH, NO_ARG},
	{"nolazyload", KEY_Z_NO_LAZYLOAD, ONE_DASH, NO_ARG},
	{"ignore", KEY_Z_IGNORE, ONE_DASH, NO_ARG},
	{"record", KEY_Z_RECORD, ONE_DASH, NO_ARG},
	{"systemlibrary", KEY_Z_SYSTEM_LIBRARY, ONE_DASH, NO_ARG},
};

static void _copy_optarg(struct ld *ld, char **dst, char *src);
static void _process_options(struct ld *ld, int key, char *arg);
static int _parse_long_options(struct ld *, struct ld_option *, int,
    int, char **, char *, enum ld_dash);
static void _print_version(struct ld *ld);

void
ld_options_parse(struct ld* ld, int argc, char **argv)
{
	enum ld_dash d;
	char *p, *p0, *oli;
	int ac, ac0;

	ac = 1;

	while (ac < argc) {
		p = argv[ac];
		if (*p != '-' || p[1] == '\0') {
			_process_options(ld, KEY_FILE, p);
			ac++;
			continue;
		}

		if (*++p == '-') {
			if (p[1] == '\0') {
				/* Option --. Ignore the rest of options. */
				return;
			}
			p++;
			d = TWO_DASH;
		} else {
			d = ONE_DASH;
			if (*p == 'B' || *p == 'z') {
				ac0 = ac;
				if (*(p0 = p + 1) == '\0')
					p0 = argv[++ac0];
				ac = _parse_long_options(ld,
				    *p == 'B' ? ld_opts_B : ld_opts_z,
				    ac0, argc, argv, p0, d);
				if (ac > 0)
					continue;
				ld_fatal(ld, "unrecognized options -%c: %s",
				    *p, p0);
			}
		}

		ac0 = _parse_long_options(ld, ld_opts, ac, argc, argv, p, d);
		if (ac0 > 0) {
			ac = ac0;
			continue;
		}

		if (d == TWO_DASH)
			ld_fatal(ld, "unrecognized option %s", p);

		/*
		 * Search short options.
		 */
		while (*p != '\0') {
			if ((oli = strchr(ld_short_opts, *p)) == NULL)
				ld_fatal(ld, "unrecognized option -%c", *p);
			if (*++oli != ':') {
				_process_options(ld, *p++, NULL);
				continue;
			}
			if (p[1] != '\0')
				_process_options(ld, *p, &p[1]);
			else if (oli[1] != ':') {
				if (++ac >= argc)
					ld_fatal(ld, "require arg for"
					    " option -%c", *p);
				_process_options(ld, *p, argv[ac]);
			}
			break;
		}

		ac++;
	}
}

static int
_parse_long_options(struct ld *ld, struct ld_option *opts, int ac,
    int argc, char **argv, char *opt, enum ld_dash dash)
{
	char *equal;
	size_t av_len;
	int i, match;

	if ((equal = strchr(opt, '=')) != NULL) {
		av_len = equal - opt;
		equal++;
		if (*equal == '\0')
			ld_fatal(ld, "no argument after =");
	} else
		av_len = strlen(opt);

	match = 0;
	for (i = 0; opts[i].lo_long != NULL; i++) {
		if (opts[i].lo_dash != ANY_DASH && opts[i].lo_dash != dash)
			continue;
		if (strlen(opts[i].lo_long) == av_len &&
		    !strncmp(opt, opts[i].lo_long, av_len)) {
			match = 1;
			break;
		}
	}
	if (!match)
		return (-1);

	switch (opts[i].lo_arg) {
	case NO_ARG:
		if (equal != NULL) {
			ld_fatal(ld, "option %s does not accept argument",
			    opts[i].lo_long);
		}
		_process_options(ld, opts[i].lo_key, NULL);
		break;
	case REQ_ARG:
		if (equal != NULL)
			_process_options(ld, opts[i].lo_key, equal);
		else {
			if (++ac >= argc)
				ld_fatal(ld, "require arg for option %s",
				    opts[i].lo_long);
			_process_options(ld, opts[i].lo_key, argv[ac]);
		}
		break;
	case OPT_ARG:
		_process_options(ld, opts[i].lo_key, equal);
		break;
	default:
		assert(0);
		break;
	}

	return (++ac);
}

static void
_process_options(struct ld *ld, int key, char *arg)
{
	struct ld_state *ls;

	assert(ld != NULL);
	ls = &ld->ld_state;

	switch (key) {
	case 'b':
		ls->ls_itgt = elftc_bfd_find_target(arg);
		if (ls->ls_itgt == NULL)
			ld_fatal(ld, "invalid BFD target `%s'", arg);
		break;
	case 'd':
		ld->ld_common_alloc = 1;
		break;
	case 'e':
		_copy_optarg(ld, &ld->ld_entry, arg);
		break;
	case 'h':
		_copy_optarg(ld, &ld->ld_soname, arg);
		break;
	case 'I':
		_copy_optarg(ld, &ld->ld_interp, arg);
		break;
	case 'l':
		ld_path_search_library(ld, arg);
		break;
	case 'L':
		ld_path_add(ld, arg, LPT_L);
		break;
	case 'M':
		ld->ld_print_linkmap = 1;
		break;
	case 'o':
		_copy_optarg(ld, &ld->ld_output_file, arg);
		break;
	case 'q':
		ld->ld_emit_reloc = 1;
		break;
	case 'r':
		ld->ld_reloc = 1;
		break;
	case 'T':
		ld_script_parse(arg);
		break;
	case 'u':
		ld_symbols_add_extern(ld, arg);
		break;
	case 'v':
	case 'V':
		_print_version(ld);
		break;
	case '(':
		ls->ls_group_level++;
		if (ls->ls_group_level > LD_MAX_NESTED_GROUP)
			ld_fatal(ld, "too many nested archive groups");
		break;
	case ')':
		ls->ls_group_level--;
		break;
	case KEY_AS_NEEDED:
		ls->ls_as_needed = 1;
		break;
	case KEY_DYNAMIC:
		ls->ls_static = 0;
		break;
	case KEY_EH_FRAME_HDR:
		ld->ld_ehframe_hdr = 1;
		break;
	case KEY_GC_SECTIONS:
		ld->ld_gc = 1;
		break;
	case KEY_NO_AS_NEEDED:
		ls->ls_as_needed = 0;
		break;
	case KEY_NO_DEFINE_COMMON:
		ld->ld_common_no_alloc = 1;
		break;
	case KEY_NO_GC_SECTIONS:
		ld->ld_gc = 0;
		break;
	case KEY_NO_PRINT_GC_SECTIONS:
		ld->ld_gc_print = 0;
		break;
	case KEY_NO_WHOLE_ARCHIVE:
		ls->ls_whole_archive = 0;
		break;
	case KEY_OFORMAT:
		ld_output_format(ld, arg, arg, arg);
		break;
	case KEY_PIE:
		ld->ld_exec = 0;
		ld->ld_pie = 1;
		ld->ld_dynamic_link = 1;
		break;
	case KEY_PRINT_GC_SECTIONS:
		ld->ld_gc_print = 1;
		break;
	case KEY_RPATH:
		ld_path_add_multiple(ld, arg, LPT_RPATH);
		break;
	case KEY_RPATH_LINK:
		ld_path_add_multiple(ld, arg, LPT_RPATH_LINK);
		break;
	case KEY_SHARED:
		ld->ld_exec = 0;
		ld->ld_dso = 1;
		ld->ld_dynamic_link = 1;
		break;
	case KEY_STATIC:
		ls->ls_static = 1;
		break;
	case KEY_WHOLE_ARCHIVE:
		ls->ls_whole_archive = 1;
		break;
	case KEY_FILE:
		ld_file_add(ld, arg, LFT_UNKNOWN);
		break;
	case KEY_VERSION_SCRIPT:
		ld_script_parse(arg);
		break;
	case KEY_Z_EXEC_STACK:
		ld->ld_gen_gnustack = 1;
		ld->ld_stack_exec_set = 1;
		ld->ld_stack_exec = 1;
		break;
	case KEY_Z_NO_EXEC_STACK:
		ld->ld_gen_gnustack = 1;
		ld->ld_stack_exec_set = 1;
		ld->ld_stack_exec = 0;
		break;
	default:
		break;
	}
}

static void
_print_version(struct ld *ld)
{

	(void) printf("%s (%s)\n", ELFTC_GETPROGNAME(), elftc_version());
	ld->ld_print_version = 1;
}

static void
_copy_optarg(struct ld *ld, char **dst, char *src)
{

	if (*dst != NULL)
		free(*dst);
	if ((*dst = strdup(src)) == NULL)
		ld_fatal_std(ld, "strdup");
}

struct ld_wildcard *
ld_wildcard_alloc(struct ld *ld)
{
	struct ld_wildcard *lw;

	if ((lw = calloc(1, sizeof(*lw))) == NULL)
		ld_fatal_std(ld, "calloc");

	return (lw);
}

void
ld_wildcard_free(void *ptr)
{
	struct ld_wildcard *lw;

	lw = ptr;
	if (lw == NULL)
		return;

	free(lw->lw_name);
	free(lw);
}
