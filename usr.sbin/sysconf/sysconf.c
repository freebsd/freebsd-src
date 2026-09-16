/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * sysconf(8): CLI entry, option parsing, and program state.
 */

#include "sysconf_priv.h"

/* Requests to process (one per name argument) */
struct request	*reqs;		/* set by main() */
unsigned int	nreqs;		/* set by main() */

/* Union of all directives, for `-a' against multi-file targets */
struct dumpent	*dumps;		/* set by scan_cb() */
size_t		ndumps;		/* set by scan_cb() */
size_t		dumpsize;	/* set by scan_cb() */

/* Resolved target state */
char		**conf_files;	/* ordered backing files */
size_t		nconf_files;
size_t		conf_scanidx;	/* file being parsed (scan_pass) */
size_t		write_idx;	/* default write target index */
int		defaults_idx = -1; /* defaults file index in conf_files
		                    * (read-only, never listed); -1 if none */
enum bsdconf_format format = BSDCONF_FORMAT_GENERIC;

/* Extra display information */
const char	*pgm;			/* set to getprogname() by main() */
bool		check = false;		/* `-c' */
bool		defaults_only = false;	/* `-D' */
bool		dump_all = false;	/* `-a' */
bool		existing_only = false;	/* `-E' */
bool		ignore_unknown = false;	/* `-i' */
bool		list_all = false;	/* `-L' */
bool		list_files = false;	/* `-l' */
bool		name_only = false;	/* `-N' */
bool		quiet = false;		/* `-q' */
bool		remove_mode = false;	/* `-x' */
bool		set_knobs = false;	/* `-s' WITH_/WITHOUT_ presence */
bool		show_desc = false;	/* `-d' */
bool		show_equals = false;	/* `-e' */
bool		show_file = false;	/* `-F' */
bool		value_only = false;	/* `-n' */
bool		verbose = false;	/* `-v' */
bool		verbose_trail = false;	/* `-V' */
bool		with_defaults = false;	/* `-A' */

/* Option arguments */
const char	*file = NULL;		/* `-f file' */
bool		file_stdin = false;	/* `-f -' given */
const char	*jailname = NULL;	/* `-j jail' */
const char	*module = NULL;		/* `-k module' */
const char	*rootdir = "";		/* `-R dir' */

/*
 * Locate the target keyword in `argv' without consuming or validating
 * anything: the first argument that is neither an option cluster nor the
 * argument of one (per OPTSTRING; an unknown option letter is assumed to
 * take no argument). A `--' ends option scanning as usual. Returns the
 * argv index of the target, or -1 when there is none. Used before any
 * getopt(3) processing so that the `rc' pass-through target (see
 * exec_sysrc() below) can be detected while the command line is still
 * pristine.
 *
 * Attached arguments (`-fPATH', `-R/altroot') are self-contained: the rest
 * of the cluster is the option's argument, so the following argv element is
 * not consumed. Only a trailing option letter that takes an argument and
 * has nothing after it in the cluster (`-f PATH') skips the next element.
 */
static int
find_target(int argc, char *argv[])
{
	int n;
	const char *o;
	const char *p;

	for (n = 1; n < argc; n++) {
		if (strcmp(argv[n], "--") == 0)
			return (n + 1 < argc ? n + 1 : -1);
		if (argv[n][0] == '-' && argv[n][1] != '\0') {
			for (p = argv[n] + 1; *p != '\0'; p++) {
				o = strchr(OPTSTRING, *p);
				if (o != NULL && o[1] == ':') {
					/* Takes an argument */
					if (p[1] != '\0') {
						;
						/* attached: rest of argv[n] */
					} else if (n + 1 < argc) {
						n++;
						/* separate argv */
					}
					break;
				}
			}
			continue;
		}
		return (n);
	}

	return (-1);
}

/*
 * The `rc' pass-through: rc.conf(5) is sysrc(8)'s domain, and its feature
 * set is a superset of ours, so no option or argument is validated,
 * interpreted, or reordered here -- every argument except the target
 * keyword itself (at argv index `skip') is handed to sysrc(8) verbatim.
 * Never returns.
 */
static void
exec_sysrc(int argc, char *argv[], int skip)
{
	int n;
	int nargc = 0;
	char **nargv;

	if ((nargv = calloc((size_t)argc + 1, sizeof(*nargv))) == NULL)
		err(EXIT_FAILURE, NULL);
	nargv[nargc++] = (char *)(uintptr_t)"sysrc";
	for (n = 1; n < argc; n++) {
		if (n == skip)
			continue;
		nargv[nargc++] = argv[n];
	}
	nargv[nargc] = NULL;

	execvp("sysrc", nargv);
	err(EXIT_FAILURE, "sysrc");
}

/*
 * Run the getopt(3) loop over `argv', setting the option globals. Called
 * once for the options preceding the target keyword and once for those
 * following it. Returns the number of arguments consumed (optind).
 */
static int
parse_options(int argc, char *argv[])
{
	int ch;

	while ((ch = getopt(argc, argv, OPTSTRING)) != -1) {
		switch (ch) {
		case 'A': /* dump all directives, defaults included */
			with_defaults = 1;
			break;
		case 'a': /* dump all directives */
			dump_all = 1;
			break;
		case 'c': /* check only; do not modify */
			check = 1;
			break;
		case 'd': /* show directive descriptions */
			show_desc = 1;
			break;
		case 'D': /* consult the defaults file only */
			defaults_only = 1;
			break;
		case 'E': /* list existing files only (with -l/-L) */
			existing_only = 1;
			break;
		case 'e': /* show name=value */
			show_equals = 1;
			break;
		case 'F': /* show authoritative file, not value */
			show_file = 1;
			break;
		case 'f': /* explicit file (`-' is standard input) */
			if (strcmp(optarg, "-") == 0) {
				file = "/dev/stdin";
				file_stdin = 1;
			} else {
				file = optarg;
				file_stdin = 0;
			}
			break;
		case 'h': /* help/usage */
			usage();
			break;
		case 'i': /* ignore unknown names */
			ignore_unknown = 1;
			break;
		case 'j': /* jail name or id */
			jailname = optarg;
			break;
		case 'k': /* kernel module drop-in */
			module = optarg;
			break;
		case 'l': /* list backing files */
			list_files = 1;
			break;
		case 'L': /* list all candidate files */
			list_all = 1;
			break;
		case 'n': /* value only */
			value_only = 1;
			break;
		case 'N': /* name only */
			name_only = 1;
			break;
		case 'q': /* quiet */
			quiet = 1;
			break;
		case 'R': /* root dir */
			rootdir = optarg;
			break;
		case 's': /* set empty WITH_/WITHOUT_ knobs (make/src) */
			set_knobs = 1;
			break;
		case 'v': /* verbose (last file + final value) */
			verbose = 1;
			break;
		case 'V': /* verbose trail (every assignment step) */
			verbose_trail = 1;
			break;
		case 'x': /* remove */
			remove_mode = 1;
			break;
		case '?': /* unknown argument (based on optstring) */
		default: /* unhandled argument (based on switch) */
			usage();
		}
	}

	return (optind);
}

/*
 * The native configuration trinity's third member: read and modify
 * loader.conf(5), sysctl.conf(5), make.conf(5), and arbitrary configuration
 * files with sysctl(8) assignment syntax.
 */
int
main(int argc, char *argv[])
{
	int have_reads = 0;
	int have_writes = 0;
	int n;
	int rv = EXIT_SUCCESS;
	const char *target = NULL;

	pgm = getprogname();

	/*
	 * The `rc' target is a pure pass-through to sysrc(8), detected
	 * while the command line is still pristine so that nothing --
	 * `--help' and `--version' included -- is intercepted on its way
	 * there (see exec_sysrc()).
	 */
	n = find_target(argc, argv);
	if (n > 0 && strcmp(argv[n], "rc") == 0)
		exec_sysrc(argc, argv, n); /* never returns */

	/*
	 * Honor `--help' and `--version' wherever they appear (getopt(3)
	 * knows nothing of long options and would reject them as illegal
	 * short options). A bare `--' ends option processing as usual.
	 */
	for (n = 1; n < argc; n++) {
		if (strcmp(argv[n], "--") == 0)
			break;
		if (strcmp(argv[n], "--help") == 0)
			help();
		if (strcmp(argv[n], "--version") == 0) {
			printf("%s (libbsdconf %s)\n", SYSCONF_VERSION,
			    BSDCONF_VERSION);
			exit(EXIT_SUCCESS);
		}
	}

	/*
	 * Process command-line options
	 */
	n = parse_options(argc, argv);
	argc -= n;
	argv += n;

	/*
	 * Consume the (required) target keyword and allow further options
	 * to follow it, mirroring the option placement freedom of
	 * sysctl(8) (e.g., `sysconf sysctl -L'). getopt(3) skips over its
	 * first argument, so hand it the target keyword's slot as the
	 * program name.
	 */
	if (argc > 0) {
		target = argv[0];
		argc--;
		argv++;
#ifdef __FreeBSD__
		optreset = 1;
		optind = 1;
#else
		optind = 0; /* glibc and musl re-initialize at zero */
#endif
		n = parse_options(argc + 1, argv - 1) - 1;
		argc -= n;
		argv += n;
	}

	if (jailname != NULL && *rootdir != '\0') {
		warnx("-j and -R are mutually exclusive");
		usage();
	}
	if (file != NULL && module != NULL) {
		warnx("-f and -k are mutually exclusive");
		usage();
	}
	if (existing_only && !list_files && !list_all) {
		warnx("-E requires -l or -L");
		usage();
	}
	if (set_knobs && remove_mode) {
		warnx("-s and -x are mutually exclusive");
		usage();
	}

#ifdef __FreeBSD__
	if (jailname != NULL) {
		int jid;

		if ((jid = jail_getid(jailname)) < 0)
			errx(EXIT_FAILURE, "%s", jail_errmsg);
		if (jail_attach(jid) != 0)
			err(EXIT_FAILURE, "jail_attach(%d)", jid);
	}
#else
	if (jailname != NULL)
		errx(EXIT_FAILURE, "-j is not supported on this platform");
#endif

	/* Resolve the target format and its backing files */
	resolve_target(target);

	/* `-k' requires a target with a module drop-in directory */
	if (module != NULL) {
		const struct bsdconf_format_def *def;
		const struct bsdconf_source *source;
		int has_moddir = 0;

		def = bsdconf_format_lookup(format);
		if (def != NULL && def->sources != NULL)
			for (source = def->sources; source->path != NULL;
			    source++)
				if (source->type == BSDCONF_SOURCE_MODDIR)
					has_moddir = 1;
		if (!has_moddir) {
			warnx("target does not support -k");
			usage();
		}
	}

	/*
	 * `-d', `-D', and `-A' consult a body of defaults; only targets
	 * that have one support them (make.conf and friends have no
	 * defaults file, and no directive descriptions to give). The
	 * exception is `sysctl -d', whose descriptions come from the
	 * running kernel rather than any file.
	 */
	{
		const struct bsdconf_format_def *def;
		int has_defaults;

		def = bsdconf_format_lookup(format);
		has_defaults = def != NULL && def->defaults != NULL;

		if (show_desc) {
			if (format != BSDCONF_FORMAT_SYSCTL &&
			    !has_defaults) {
				warnx("target does not support -d");
				usage();
			}
			if (file != NULL) {
				warnx("-d and -f are mutually exclusive");
				usage();
			}
			if (list_files || list_all || remove_mode || check ||
			    set_knobs) {
				warnx("-d cannot be combined with "
				    "-c/-l/-L/-s/-x");
				usage();
			}
#ifdef __FreeBSD__
			if (format == BSDCONF_FORMAT_SYSCTL &&
			    *rootdir != '\0')
				errx(EXIT_FAILURE, "sysctl descriptions "
				    "come from the running kernel; "
				    "-R does not apply");
#endif
		}
		if ((defaults_only || with_defaults) && !has_defaults) {
			warnx("target does not support -%c",
			    defaults_only ? 'D' : 'A');
			usage();
		}
		if ((defaults_only || with_defaults) && file != NULL) {
			warnx("-%c and -f are mutually exclusive",
			    defaults_only ? 'D' : 'A');
			usage();
		}
	}

	/* File listings take no names and perform no other work */
	if (list_files || list_all) {
		if (argc != 0 || dump_all) {
			warnx("-l/-L take no names");
			usage();
		}
		exit(do_list());
	}

	/* `-A' with no names dumps everything, defaults included */
	if (with_defaults && argc == 0)
		dump_all = 1;

	/* Display usage and exit if not given at least one name */
	if (argc == 0 && !dump_all) {
		warnx("no names provided");
		usage();
	}
	if (argc != 0 && dump_all) {
		warnx("-a does not take names");
		usage();
	}

	/* Classify the remaining arguments */
	nreqs = (unsigned int)argc;
	if (nreqs > 0 &&
	    (reqs = calloc(nreqs, sizeof(*reqs))) == NULL)
		err(EXIT_FAILURE, NULL);
	if (set_knobs) {
		if (format != BSDCONF_FORMAT_MAKE &&
		    format != BSDCONF_FORMAT_SRC) {
			warnx("-s is only valid for make and src");
			usage();
		}
		if (argc == 0) {
			warnx("-s requires at least one WITH_/WITHOUT_ name");
			usage();
		}
	}
	for (n = 0; n < argc; n++) {
		split_request(argv[n], &reqs[n], remove_mode);
		if (set_knobs) {
			if (reqs[n].newvalue != NULL) {
				warnx("-s does not take a value "
				    "('%s')", argv[n]);
				usage();
			}
			if (!make_knob_name_p(reqs[n].name)) {
				if (strcmp(reqs[n].name,
				    "WITHOUT_MODULES") == 0)
					warnx("WITHOUT_MODULES takes a "
					    "module list; use "
					    "WITHOUT_MODULES=...");
				else
					warnx("-s requires a WITH_ or "
					    "WITHOUT_ name ('%s')",
					    reqs[n].name);
				usage();
			}
			reqs[n].newvalue = "";
		}
			if (reqs[n].newvalue != NULL && !reqs[n].remove)
				warn_with_equals_no(reqs[n].name,
				    reqs[n].newvalue, NULL, 0);
		if (reqs[n].remove || reqs[n].newvalue != NULL)
			have_writes = 1;
		else
			have_reads = 1;
	}
	if (check && !have_writes) {
		warnx("-c requires at least one name=value or -s name");
		usage();
	}

	/* Descriptions are read-only */
	if (show_desc) {
		if (have_writes) {
			warnx("-d does not take values");
			usage();
		}
#ifndef __FreeBSD__
		if (format == BSDCONF_FORMAT_SYSCTL)
			errx(EXIT_FAILURE,
			    "sysctl descriptions require FreeBSD");
#endif
		/* Explicit names need no file scan at all */
		if (!dump_all) {
#ifdef __FreeBSD__
			if (format == BSDCONF_FORMAT_SYSCTL)
				exit(describe_sysctl());
#endif
			exit(describe_defaults());
		}

		/*
		 * Dumps (-ad, -Ad, -aDd) pair each scanned directive with
		 * its description; harvest the descriptions before the
		 * read sandbox slams shut.
		 */
		if (format != BSDCONF_FORMAT_SYSCTL)
			load_descriptions();
	}

	/* Defaults can be read and checked against, never rewritten */
	if ((defaults_only || with_defaults) && have_writes && !check)
		errx(EXIT_FAILURE, "defaults are read-only");

	/* Standard input can be read and checked, never rewritten */
	if (file_stdin && have_writes && !check)
		errx(EXIT_FAILURE, "cannot write to standard input");

#if defined(__FreeBSD__)
	/*
	 * Refuse up front to set sysctl OIDs that can never take effect
	 * from sysctl.conf(5) (non-leaf, read-only, loader-only tunables)
	 * or whose values cannot fit the OID's CTLTYPE (so an overflowing
	 * assignment cannot surprise sysctl(8) at boot). Unknown OIDs are
	 * warned about but still written -- the module may not be loaded
	 * yet, matching init(8)'s handling of sysctl.conf(5). Only
	 * meaningful against the running system's sysctl tree, so skipped
	 * under -R. -i/-q quiet the unknown-OID warning; -k remains the
	 * module drop-in selector.
	 */
	if (format == BSDCONF_FORMAT_SYSCTL && have_writes && !check &&
	    *rootdir == '\0') {
		int quiet_unknown = ignore_unknown || quiet;
		unsigned int u;

		for (u = 0; u < nreqs; u++) {
			if (reqs[u].newvalue == NULL)
				continue;
			if (sysctl_writable(reqs[u].name, reqs[u].newvalue,
			    quiet_unknown) != 0) {
				/* Convert to a no-op; count the failure */
				reqs[u].newvalue = NULL;
				reqs[u].remove = 0;
				reqs[u].name = "";
				rv = EXIT_FAILURE;
			}
		}
		have_writes = 0;
		have_reads = 0;
		for (u = 0; u < nreqs; u++) {
			if (reqs[u].remove || reqs[u].newvalue != NULL)
				have_writes = 1;
			else if (reqs[u].name[0] != '\0')
				have_reads = 1;
		}
		if (rv != EXIT_SUCCESS && !have_writes && !have_reads)
			exit(rv); /* nothing settable remains */
	}
#endif

	/*
	 * Scan the backing files to locate each request's authoritative
	 * definition. Pure reads and -c checks (do_checks() never calls
	 * put) run the scan inside a Capsicum sandbox -- except a sysctl
	 * description dump, whose kernel queries the sandbox would deny.
	 * Classify by whether we will actually write, not by whether the
	 * argv looked like name=value (`-c' sets have_writes for checks).
	 */
	scan_pass((check || !have_writes) &&
	    !(show_desc && format == BSDCONF_FORMAT_SYSCTL));

	/* Materialize list edits now that effective values are known */
	if (merge_list_requests() != EXIT_SUCCESS)
		rv = EXIT_FAILURE;

	if (check) {
		if (do_make_strikes(1) != EXIT_SUCCESS)
			rv = EXIT_FAILURE;
		if (do_checks() != EXIT_SUCCESS)
			rv = EXIT_FAILURE;
		exit(rv);
	}

	/* Apply writes first so that subsequent reads see the result */
	if (have_writes) {
		int wrv;

		if ((wrv = do_make_strikes(0)) != EXIT_SUCCESS)
			rv = wrv;
		if ((wrv = do_writes()) != EXIT_SUCCESS)
			rv = wrv;

		/* Refresh the scan for any trailing read requests */
		if (have_reads || dump_all) {
			for (n = 0; (unsigned int)n < nreqs; n++) {
				size_t ai;

				trail_free(reqs[n].trail, reqs[n].ntrail);
				reqs[n].trail = NULL;
				reqs[n].ntrail = 0;
				for (ai = 0; ai < reqs[n].nassigns; ai++)
					free(reqs[n].assigns[ai].piece);
				free(reqs[n].assigns);
				reqs[n].assigns = NULL;
				reqs[n].nassigns = 0;
				reqs[n].found = 0;
				reqs[n].append_present = 0;
				reqs[n].srcidx = -1;
				reqs[n].line = 0;
				memset(reqs[n].infile, 0, nconf_files);
			}
			scan_pass(0);
		}
	}

	if (have_reads || dump_all) {
		int rrv;

		if ((rrv = do_reads()) != EXIT_SUCCESS)
			rv = rrv;
	}

	exit(rv);
}

/*
 * Print short usage statement to stderr and exit with error status.
 */
void
usage(void)
{

	fprintf(stderr,
	    "usage: %s target [-AcDeFinNqsvVx] [-j jail | -R dir]"
	    " [-f file | -k module]\n"
	    "               name[[+|-]=value] ...\n", pgm);
	fprintf(stderr,
	    "       %s target [-ADeFnNqvV] [-j jail | -R dir]"
	    " [-f file | -k module] -a\n", pgm);
	fprintf(stderr,
	    "       %s target [-AaDinNq] [-j jail | -R dir]"
	    " -d [name ...]\n", pgm);
	fprintf(stderr,
	    "       %s target [-E] [-j jail | -R dir] [-k module]"
	    " -l | -L\n", pgm);
	fprintf(stderr,
	    "       %s rc [sysrc(8) argument ...]\n", pgm);
	fprintf(stderr, "Try `%s --help' for more information.\n", pgm);
	exit(EXIT_FAILURE);
}

/*
 * Print long usage statement to stderr and exit with error status.
 */
void
help(void)
{

	fprintf(stderr,
	    "usage: %s target [-AcDeFinNqsvVx] [-j jail | -R dir]"
	    " [-f file | -k module] name[[+|-]=value] ...\n", pgm);
	fprintf(stderr, "TARGETS:\n");
#define TGTFMT "\t%-9s %s\n"
	fprintf(stderr, TGTFMT, "loader",
	    "files named by loader_conf_files et al. in");
	fprintf(stderr, TGTFMT, "",
	    "/boot/defaults/loader.conf (typically /boot/loader.conf,");
	fprintf(stderr, TGTFMT, "",
	    "loader.conf.d/*.conf, loader.conf.local)");
	fprintf(stderr, TGTFMT, "sysctl",
	    "/etc/sysctl.conf, sysctl.conf.local, sysctl.kld.d/*.conf");
	fprintf(stderr, TGTFMT, "make",
	    "/etc/make.conf (or -f for any make-syntax file)");
	fprintf(stderr, TGTFMT, "src",
	    "/usr/src build triad: src-env.conf, make.conf, src.conf");
	fprintf(stderr, TGTFMT, "",
	    "(new writes prefer src.conf; env: SRC_ENV_CONF,");
	fprintf(stderr, TGTFMT, "",
	    "__MAKE_CONF, SRCCONF)");
	fprintf(stderr, TGTFMT, "rc",
	    "pass-through: all other arguments go to sysrc(8) verbatim");
	fprintf(stderr, TGTFMT, "generic",
	    "no default files; requires -f file");
	fprintf(stderr, "OPTIONS:\n");
#define OPTFMT "\t%-9s %s\n"
	fprintf(stderr, OPTFMT, "-A",
	    "Include the target's defaults file in the sourcing order");
	fprintf(stderr, OPTFMT, "",
	    "(alone, dumps everything, defaults included).");
	fprintf(stderr, OPTFMT, "-a",
	    "Dump the union of all directives from the target's files.");
	fprintf(stderr, OPTFMT, "-c",
	    "Check. Return success if no changes needed, else error.");
	fprintf(stderr, OPTFMT, "-D",
	    "Consult only the target's defaults file.");
	fprintf(stderr, OPTFMT, "-d",
	    "Show directive descriptions (from the defaults file's");
	fprintf(stderr, OPTFMT, "",
	    "comments; for sysctl, from the running kernel). With");
	fprintf(stderr, OPTFMT, "",
	    "-a, -A, or -D, describe every directive in scope.");
	fprintf(stderr, OPTFMT, "-E",
	    "With -l or -L, list only files that exist on disk.");
	fprintf(stderr, OPTFMT, "-e",
	    "Separate name and value with `=' (reads and write echoes).");
	fprintf(stderr, OPTFMT, "-F",
	    "Show the file holding each directive's effective value.");
	fprintf(stderr, OPTFMT, "-f file",
	    "Operate on file, in the target's format, instead of the");
	fprintf(stderr, OPTFMT, "",
	    "target's standard files (`-' means standard input).");
	fprintf(stderr, OPTFMT, "-h",
	    "Print a short usage statement to stderr and exit.");
	fprintf(stderr, OPTFMT, "--help",
	    "Print this message to stderr and exit.");
	fprintf(stderr, OPTFMT, "--version",
	    "Print the utility and library versions and exit.");
	fprintf(stderr, OPTFMT, "-i",
	    "Ignore unknown names (and quiet unknown sysctl OID warnings).");
	fprintf(stderr, OPTFMT, "-j jail",
	    "Operate within the jail `jail' (name or numeric id).");
	fprintf(stderr, OPTFMT, "-k module",
	    "Include the kernel module drop-in file for `module'.");
	fprintf(stderr, OPTFMT, "-l",
	    "List the pathnames of the target's backing files.");
	fprintf(stderr, OPTFMT, "-L",
	    "List all candidate files, including module drop-ins.");
	fprintf(stderr, OPTFMT, "-n",
	    "Show only directive values, not their names.");
	fprintf(stderr, OPTFMT, "-N",
	    "Show only directive names, not their values.");
	fprintf(stderr, OPTFMT, "-q",
	    "Quiet. Suppress unknown-directive warnings and the");
	fprintf(stderr, OPTFMT, "",
	    "`old -> new' echo of writes.");
	fprintf(stderr, OPTFMT, "-R dir",
	    "Operate within the root directory `dir' rather than `/'.");
	fprintf(stderr, OPTFMT, "-s",
	    "Set empty WITH_/WITHOUT_ knobs (make and src only).");
	fprintf(stderr, OPTFMT, "-v",
	    "Verbose. Print the pathname of the configuration file");
	fprintf(stderr, OPTFMT, "",
	    "holding the final effective value.");
	fprintf(stderr, OPTFMT, "-V",
	    "Trail. Reads: each assignment step (file:line, operator,");
	fprintf(stderr, OPTFMT, "",
	    "fragment, running effective). Writes: file:line:");
	fprintf(stderr, OPTFMT, "",
	    "name=old -> name=new (or name=value (unchanged)).");
	fprintf(stderr, OPTFMT, "-x",
	    "Remove name(s) from the target's files.");
	fprintf(stderr, "ENVIRONMENT:\n");
	fprintf(stderr, OPTFMT, "LOADER_DEFAULTS",
	    "Defaults file for the loader target (in place of");
	fprintf(stderr, OPTFMT, "",
	    "/boot/defaults/loader.conf).");
	fprintf(stderr, OPTFMT, "SRC_ENV_CONF",
	    "src-env.conf for the src target (default /etc/src-env.conf).");
	fprintf(stderr, OPTFMT, "__MAKE_CONF",
	    "make.conf for the src target (default /etc/make.conf).");
	fprintf(stderr, OPTFMT, "SRCCONF",
	    "src.conf for the src target (default /etc/src.conf).");
	exit(EXIT_FAILURE);
}
