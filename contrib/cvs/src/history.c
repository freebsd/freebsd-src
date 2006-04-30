/*
 *
 *    You may distribute under the terms of the GNU General Public License
 *    as specified in the README file that comes with the CVS 1.0 kit.
 *
 * **************** History of Users and Module ****************
 *
 * LOGGING:  Append record to "${CVSROOT}/CVSROOTADM/CVSROOTADM_HISTORY".
 *
 * On For each Tag, Add, Checkout, Commit, Update or Release command,
 * one line of text is written to a History log.
 *
 *	X date | user | CurDir | special | rev(s) | argument '\n'
 *
 * where: [The spaces in the example line above are not in the history file.]
 *
 *  X		is a single character showing the type of event:
 *		T	"Tag" cmd.
 *		O	"Checkout" cmd.
 *              E       "Export" cmd.
 *		F	"Release" cmd.
 *		W	"Update" cmd - No User file, Remove from Entries file.
 *		U	"Update" cmd - File was checked out over User file.
 *		P	"Update" cmd - User file was patched.
 *		G	"Update" cmd - File was merged successfully.
 *		C	"Update" cmd - File was merged and shows overlaps.
 *		M	"Commit" cmd - "Modified" file.
 *		A	"Commit" cmd - "Added" file.
 *		R	"Commit" cmd - "Removed" file.
 *
 *  date	is a fixed length 8-char hex representation of a Unix time_t.
 *		[Starting here, variable fields are delimited by '|' chars.]
 *
 *  user	is the username of the person who typed the command.
 *
 *  CurDir	The directory where the action occurred.  This should be the
 *		absolute path of the directory which is at the same level as
 *		the "Repository" field (for W,U,P,G,C & M,A,R).
 *
 *  Repository	For record types [W,U,P,G,C,M,A,R] this field holds the
 *		repository read from the administrative data where the
 *		command was typed.
 *		T	"A" --> New Tag, "D" --> Delete Tag
 *			Otherwise it is the Tag or Date to modify.
 *		O,F,E	A "" (null field)
 *
 *  rev(s)	Revision number or tag.
 *		T	The Tag to apply.
 *		O,E	The Tag or Date, if specified, else "" (null field).
 *		F	"" (null field)
 *		W	The Tag or Date, if specified, else "" (null field).
 *		U,P	The Revision checked out over the User file.
 *		G,C	The Revision(s) involved in merge.
 *		M,A,R	RCS Revision affected.
 *
 *  argument	The module (for [TOEF]) or file (for [WUPGCMAR]) affected.
 *
 *
 *** Report categories: "User" and "Since" modifiers apply to all reports.
 *			[For "sort" ordering see the "sort_order" routine.]
 *
 *   Extract list of record types
 *
 *	-e, -x [TOEFWUPGCMAR]
 *
 *		Extracted records are simply printed, No analysis is performed.
 *		All "field" modifiers apply.  -e chooses all types.
 *
 *   Checked 'O'ut modules
 *
 *	-o, -w
 *		Checked out modules.  'F' and 'O' records are examined and if
 *		the last record for a repository/file is an 'O', a line is
 *		printed.  "-w" forces the "working dir" to be used in the
 *		comparison instead of the repository.
 *
 *   Committed (Modified) files
 *
 *	-c, -l, -w
 *		All 'M'odified, 'A'dded and 'R'emoved records are examined.
 *		"Field" modifiers apply.  -l forces a sort by file within user
 *		and shows only the last modifier.  -w works as in Checkout.
 *
 *		Warning: Be careful with what you infer from the output of
 *			 "cvs hi -c -l".  It means the last time *you*
 *			 changed the file, not the list of files for which
 *			 you were the last changer!!!
 *
 *   Module history for named modules.
 *	-m module, -l
 *
 *		This is special.  If one or more modules are specified, the
 *		module names are remembered and the files making up the
 *		modules are remembered.  Only records matching exactly those
 *		files and repositories are shown.  Sorting by "module", then
 *		filename, is implied.  If -l ("last modified") is specified,
 *		then "update" records (types WUPCG), tag and release records
 *		are ignored and the last (by date) "modified" record.
 *
 *   TAG history
 *
 *	-T	All Tag records are displayed.
 *
 *** Modifiers.
 *
 *   Since ...		[All records contain a timestamp, so any report
 *			 category can be limited by date.]
 *
 *	-D date		- The "date" is parsed into a Unix "time_t" and
 *			  records with an earlier time stamp are ignored.
 *	-r rev/tag	- A "rev" begins with a digit.  A "tag" does not.  If
 *			  you use this option, every file is searched for the
 *			  indicated rev/tag.
 *	-t tag		- The "tag" is searched for in the history file and no
 *			  record is displayed before the tag is found.  An
 *			  error is printed if the tag is never found.
 *	-b string	- Records are printed only back to the last reference
 *			  to the string in the "module", "file" or
 *			  "repository" fields.
 *
 *   Field Selections	[Simple comparisons on existing fields.  All field
 *			 selections are repeatable.]
 *
 *	-a		- All users.
 *	-u user		- If no user is given and '-a' is not given, only
 *			  records for the user typing the command are shown.
 *			  ==> If -a or -u is not specified, just use "self".
 *
 *	-f filematch	- Only records in which the "file" field contains the
 *			  string "filematch" are considered.
 *
 *	-p repository	- Only records in which the "repository" string is a
 *			  prefix of the "repos" field are considered.
 *
 *	-n modulename	- Only records which contain "modulename" in the
 *			  "module" field are considered.
 *
 *
 * EXAMPLES: ("cvs history", "cvs his" or "cvs hi")
 *
 *** Checked out files for username.  (default self, e.g. "dgg")
 *	cvs hi			[equivalent to: "cvs hi -o -u dgg"]
 *	cvs hi -u user		[equivalent to: "cvs hi -o -u user"]
 *	cvs hi -o		[equivalent to: "cvs hi -o -u dgg"]
 *
 *** Committed (modified) files from the beginning of the file.
 *	cvs hi -c [-u user]
 *
 *** Committed (modified) files since Midnight, January 1, 1990:
 *	cvs hi -c -D 'Jan 1 1990' [-u user]
 *
 *** Committed (modified) files since tag "TAG" was stored in the history file:
 *	cvs hi -c -t TAG [-u user]
 *
 *** Committed (modified) files since tag "TAG" was placed on the files:
 *	cvs hi -c -r TAG [-u user]
 *
 *** Who last committed file/repository X?
 *	cvs hi -c -l -[fp] X
 *
 *** Modified files since tag/date/file/repos?
 *	cvs hi -c {-r TAG | -D Date | -b string}
 *
 *** Tag history
 *	cvs hi -T
 *
 *** History of file/repository/module X.
 *	cvs hi -[fpn] X
 *
 *** History of user "user".
 *	cvs hi -e -u user
 *
 *** Dump (eXtract) specified record types
 *	cvs hi -x [TOEFWUPGCMAR]
 *
 *
 * FUTURE:		J[Join], I[Import]  (Not currently implemented.)
 *
 */

#include "cvs.h"
#include "history.h"
#include "savecwd.h"

static struct hrec
{
    char *type;		/* Type of record (In history record) */
    char *user;		/* Username (In history record) */
    char *dir;		/* "Compressed" Working dir (In history record) */
    char *repos;	/* (Tag is special.) Repository (In history record) */
    char *rev;		/* Revision affected (In history record) */
    char *file;		/* Filename (In history record) */
    char *end;		/* Ptr into repository to copy at end of workdir */
    char *mod;		/* The module within which the file is contained */
    time_t date;	/* Calculated from date stored in record */
    long idx;		/* Index of record, for "stable" sort. */
} *hrec_head;
static long hrec_idx;


static void fill_hrec PROTO((char *line, struct hrec * hr));
static int accept_hrec PROTO((struct hrec * hr, struct hrec * lr));
static int select_hrec PROTO((struct hrec * hr));
static int sort_order PROTO((const PTR l, const PTR r));
static int within PROTO((char *find, char *string));
static void expand_modules PROTO((void));
static void read_hrecs PROTO((char *fname));
static void report_hrecs PROTO((void));
static void save_file PROTO((char *dir, char *name, char *module));
static void save_module PROTO((char *module));
static void save_user PROTO((char *name));

#define USER_INCREMENT	2
#define FILE_INCREMENT	128
#define MODULE_INCREMENT 5
#define HREC_INCREMENT	128

static short report_count;

static short extract;
static short extract_all;
static short v_checkout;
static short modified;
static short tag_report;
static short module_report;
static short working;
static short last_entry;
static short all_users;

static short user_sort;
static short repos_sort;
static short file_sort;
static short module_sort;

static short tz_local;
static time_t tz_seconds_east_of_GMT;
static char *tz_name = "+0000";

char *logHistory = ALL_HISTORY_REC_TYPES;

/* -r, -t, or -b options, malloc'd.  These are "" if the option in
   question is not specified or is overridden by another option.  The
   main reason for using "" rather than NULL is historical.  Together
   with since_date, these are a mutually exclusive set; one overrides the
   others.  */
static char *since_rev;
static char *since_tag;
static char *backto;
/* -D option, or 0 if not specified.  RCS format.  */
static char * since_date;

static struct hrec *last_since_tag;
static struct hrec *last_backto;

/* Record types to look for, malloc'd.  Probably could be statically
   allocated, but only if we wanted to check for duplicates more than
   we do.  */
static char *rec_types;

static int hrec_count;
static int hrec_max;

static char **user_list;	/* Ptr to array of ptrs to user names */
static int user_max;		/* Number of elements allocated */
static int user_count;		/* Number of elements used */

static struct file_list_str
{
    char *l_file;
    char *l_module;
} *file_list;			/* Ptr to array file name structs */
static int file_max;		/* Number of elements allocated */
static int file_count;		/* Number of elements used */

static char **mod_list;		/* Ptr to array of ptrs to module names */
static int mod_max;		/* Number of elements allocated */
static int mod_count;		/* Number of elements used */

static char *histfile;		/* Ptr to the history file name */

/* This is pretty unclear.  First of all, separating "flags" vs.
   "options" (I think the distinction is that "options" take arguments)
   is nonstandard, and not something we do elsewhere in CVS.  Second of
   all, what does "reports" mean?  I think it means that you can only
   supply one of those options, but "reports" hardly has that meaning in
   a self-explanatory way.  */
static const char *const history_usg[] =
{
    "Usage: %s %s [-report] [-flags] [-options args] [files...]\n\n",
    "   Reports:\n",
    "        -T              Produce report on all TAGs\n",
    "        -c              Committed (Modified) files\n",
    "        -o              Checked out modules\n",
    "        -m <module>     Look for specified module (repeatable)\n",
    "        -x [" ALL_HISTORY_REC_TYPES "] Extract by record type\n",
    "        -e              Everything (same as -x, but all record types)\n",
    "   Flags:\n",
    "        -a              All users (Default is self)\n",
    "        -l              Last modified (committed or modified report)\n",
    "        -w              Working directory must match\n",
    "   Options:\n",
    "        -D <date>       Since date (Many formats)\n",
    "        -b <str>        Back to record with str in module/file/repos field\n",
    "        -f <file>       Specified file (same as command line) (repeatable)\n",
    "        -n <modulename> In module (repeatable)\n",
    "        -p <repos>      In repository (repeatable)\n",
    "        -r <rev/tag>    Since rev or tag (looks inside RCS files!)\n",
    "        -t <tag>        Since tag record placed in history file (by anyone).\n",
    "        -u <user>       For user name (repeatable)\n",
    "        -z <tz>         Output for time zone <tz> (e.g. -z -0700)\n",
    NULL};

/* Sort routine for qsort:
   - If a user is selected at all, sort it first. User-within-file is useless.
   - If a module was selected explicitly, sort next on module.
   - Then sort by file.  "File" is "repository/file" unless "working" is set,
     then it is "workdir/file".  (Revision order should always track date.)
   - Always sort timestamp last.
*/
static int
sort_order (l, r)
    const PTR l;
    const PTR r;
{
    int i;
    const struct hrec *left = (const struct hrec *) l;
    const struct hrec *right = (const struct hrec *) r;

    if (user_sort)	/* If Sort by username, compare users */
    {
	if ((i = strcmp (left->user, right->user)) != 0)
	    return (i);
    }
    if (module_sort)	/* If sort by modules, compare module names */
    {
	if (left->mod && right->mod)
	    if ((i = strcmp (left->mod, right->mod)) != 0)
		return (i);
    }
    if (repos_sort)	/* If sort by repository, compare them. */
    {
	if ((i = strcmp (left->repos, right->repos)) != 0)
	    return (i);
    }
    if (file_sort)	/* If sort by filename, compare files, NOT dirs. */
    {
	if ((i = strcmp (left->file, right->file)) != 0)
	    return (i);

	if (working)
	{
	    if ((i = strcmp (left->dir, right->dir)) != 0)
		return (i);

	    if ((i = strcmp (left->end, right->end)) != 0)
		return (i);
	}
    }

    /*
     * By default, sort by date, time
     * XXX: This fails after 2030 when date slides into sign bit
     */
    if ((i = ((long) (left->date) - (long) (right->date))) != 0)
	return (i);

    /* For matching dates, keep the sort stable by using record index */
    return (left->idx - right->idx);
}

int
history (argc, argv)
    int argc;
    char **argv;
{
    int i, c;
    char *fname;

    if (argc == -1)
	usage (history_usg);

    since_rev = xstrdup ("");
    since_tag = xstrdup ("");
    backto = xstrdup ("");
    rec_types = xstrdup ("");
    optind = 0;
    while ((c = getopt (argc, argv, "+Tacelow?D:b:f:m:n:p:r:t:u:x:X:z:")) != -1)
    {
	switch (c)
	{
	    case 'T':			/* Tag list */
		report_count++;
		tag_report++;
		break;
	    case 'a':			/* For all usernames */
		all_users++;
		break;
	    case 'c':
		report_count++;
		modified = 1;
		break;
	    case 'e':
		report_count++;
		extract_all++;
		free (rec_types);
		rec_types = xstrdup (ALL_HISTORY_REC_TYPES);
		break;
	    case 'l':			/* Find Last file record */
		last_entry = 1;
		break;
	    case 'o':
		report_count++;
		v_checkout = 1;
		break;
	    case 'w':			/* Match Working Dir (CurDir) fields */
		working = 1;
		break;
	    case 'X':			/* Undocumented debugging flag */
#ifdef DEBUG
		histfile = optarg;
#endif
		break;

	    case 'D':			/* Since specified date */
		if (*since_rev || *since_tag || *backto)
		{
		    error (0, 0, "date overriding rev/tag/backto");
		    *since_rev = *since_tag = *backto = '\0';
		}
		since_date = Make_Date (optarg);
		break;
	    case 'b':			/* Since specified file/Repos */
		if (since_date || *since_rev || *since_tag)
		{
		    error (0, 0, "backto overriding date/rev/tag");
		    *since_rev = *since_tag = '\0';
		    if (since_date != NULL)
			free (since_date);
		    since_date = NULL;
		}
		free (backto);
		backto = xstrdup (optarg);
		break;
	    case 'f':			/* For specified file */
		save_file ("", optarg, (char *) NULL);
		break;
	    case 'm':			/* Full module report */
		if (!module_report++) report_count++;
		/* fall through */
	    case 'n':			/* Look for specified module */
		save_module (optarg);
		break;
	    case 'p':			/* For specified directory */
		save_file (optarg, "", (char *) NULL);
		break;
	    case 'r':			/* Since specified Tag/Rev */
		if (since_date || *since_tag || *backto)
		{
		    error (0, 0, "rev overriding date/tag/backto");
		    *since_tag = *backto = '\0';
		    if (since_date != NULL)
			free (since_date);
		    since_date = NULL;
		}
		free (since_rev);
		since_rev = xstrdup (optarg);
		break;
	    case 't':			/* Since specified Tag/Rev */
		if (since_date || *since_rev || *backto)
		{
		    error (0, 0, "tag overriding date/marker/file/repos");
		    *since_rev = *backto = '\0';
		    if (since_date != NULL)
			free (since_date);
		    since_date = NULL;
		}
		free (since_tag);
		since_tag = xstrdup (optarg);
		break;
	    case 'u':			/* For specified username */
		save_user (optarg);
		break;
	    case 'x':
		report_count++;
		extract++;
		{
		    char *cp;

		    for (cp = optarg; *cp; cp++)
			if (!strchr (ALL_HISTORY_REC_TYPES, *cp))
			    error (1, 0, "%c is not a valid report type", *cp);
		}
		free (rec_types);
		rec_types = xstrdup (optarg);
		break;
	    case 'z':
		tz_local = 
		    (optarg[0] == 'l' || optarg[0] == 'L')
		    && (optarg[1] == 't' || optarg[1] == 'T')
		    && !optarg[2];
		if (tz_local)
		    tz_name = optarg;
		else
		{
		    /*
		     * Convert a known time with the given timezone to time_t.
		     * Use the epoch + 23 hours, so timezones east of GMT work.
		     */
		    static char f[] = "1/1/1970 23:00 %s";
		    char *buf = xmalloc (sizeof (f) - 2 + strlen (optarg));
		    time_t t;
		    sprintf (buf, f, optarg);
		    t = get_date (buf, (struct timeb *) NULL);
		    free (buf);
		    if (t == (time_t) -1)
			error (0, 0, "%s is not a known time zone", optarg);
		    else
		    {
			/*
			 * Convert to seconds east of GMT, removing the
			 * 23-hour offset mentioned above.
			 */
			tz_seconds_east_of_GMT = (time_t)23 * 60 * 60  -  t;
			tz_name = optarg;
		    }
		}
		break;
	    case '?':
	    default:
		usage (history_usg);
		break;
	}
    }
    argc -= optind;
    argv += optind;
    for (i = 0; i < argc; i++)
	save_file ("", argv[i], (char *) NULL);


    /* ================ Now analyze the arguments a bit */
    if (!report_count)
	v_checkout++;
    else if (report_count > 1)
	error (1, 0, "Only one report type allowed from: \"-Tcomxe\".");

#ifdef CLIENT_SUPPORT
    if (current_parsed_root->isremote)
    {
	struct file_list_str *f1;
	char **mod;

	/* We're the client side.  Fire up the remote server.  */
	start_server ();
	
	ign_setup ();

	if (tag_report)
	    send_arg("-T");
	if (all_users)
	    send_arg("-a");
	if (modified)
	    send_arg("-c");
	if (last_entry)
	    send_arg("-l");
	if (v_checkout)
	    send_arg("-o");
	if (working)
	    send_arg("-w");
	if (histfile)
	    send_arg("-X");
	if (since_date)
	    client_senddate (since_date);
	if (backto[0] != '\0')
	    option_with_arg ("-b", backto);
	for (f1 = file_list; f1 < &file_list[file_count]; ++f1)
	{
	    if (f1->l_file[0] == '*')
		option_with_arg ("-p", f1->l_file + 1);
	    else
		option_with_arg ("-f", f1->l_file);
	}
	if (module_report)
	    send_arg("-m");
	for (mod = mod_list; mod < &mod_list[mod_count]; ++mod)
	    option_with_arg ("-n", *mod);
	if (*since_rev)
	    option_with_arg ("-r", since_rev);
	if (*since_tag)
	    option_with_arg ("-t", since_tag);
	for (mod = user_list; mod < &user_list[user_count]; ++mod)
	    option_with_arg ("-u", *mod);
	if (extract_all)
	    send_arg("-e");
	if (extract)
	    option_with_arg ("-x", rec_types);
	option_with_arg ("-z", tz_name);

	send_to_server ("history\012", 0);
        return get_responses_and_close ();
    }
#endif

    if (all_users)
	save_user ("");

    if (mod_list)
	expand_modules ();

    if (tag_report)
    {
	if (!strchr (rec_types, 'T'))
	{
	    rec_types = xrealloc (rec_types, strlen (rec_types) + 5);
	    (void) strcat (rec_types, "T");
	}
    }
    else if (extract || extract_all)
    {
	if (user_list)
	    user_sort++;
    }
    else if (modified)
    {
	free (rec_types);
	rec_types = xstrdup ("MAR");
	/*
	 * If the user has not specified a date oriented flag ("Since"), sort
	 * by Repository/file before date.  Default is "just" date.
	 */
	if (last_entry
	    || (!since_date && !*since_rev && !*since_tag && !*backto))
	{
	    repos_sort++;
	    file_sort++;
	    /*
	     * If we are not looking for last_modified and the user specified
	     * one or more users to look at, sort by user before filename.
	     */
	    if (!last_entry && user_list)
		user_sort++;
	}
    }
    else if (module_report)
    {
	free (rec_types);
	rec_types = xstrdup (last_entry ? "OMAR" : ALL_HISTORY_REC_TYPES);
	module_sort++;
	repos_sort++;
	file_sort++;
	working = 0;			/* User's workdir doesn't count here */
    }
    else
	/* Must be "checkout" or default */
    {
	free (rec_types);
	rec_types = xstrdup ("OF");
	/* See comments in "modified" above */
	if (!last_entry && user_list)
	    user_sort++;
	if (last_entry
	    || (!since_date && !*since_rev && !*since_tag && !*backto))
	    file_sort++;
    }

    /* If no users were specified, use self (-a saves a universal ("") user) */
    if (!user_list)
	save_user (getcaller ());

    /* If we're looking back to a Tag value, must consider "Tag" records */
    if (*since_tag && !strchr (rec_types, 'T'))
    {
	rec_types = xrealloc (rec_types, strlen (rec_types) + 5);
	(void) strcat (rec_types, "T");
    }

    if (histfile)
	fname = xstrdup (histfile);
    else
    {
	fname = xmalloc (strlen (current_parsed_root->directory) + sizeof (CVSROOTADM)
			 + sizeof (CVSROOTADM_HISTORY) + 10);
	(void) sprintf (fname, "%s/%s/%s", current_parsed_root->directory,
			CVSROOTADM, CVSROOTADM_HISTORY);
    }

    read_hrecs (fname);
    if(hrec_count>0)
    {
	qsort ((PTR) hrec_head, hrec_count, 
		sizeof (struct hrec), sort_order);
    }
    report_hrecs ();
    free (fname);
    if (since_date != NULL)
	free (since_date);
    free (since_rev);
    free (since_tag);
    free (backto);
    free (rec_types);

    return (0);
}

void
history_write (type, update_dir, revs, name, repository)
    int type;
    const char *update_dir;
    const char *revs;
    const char *name;
    const char *repository;
{
    char *fname;
    char *workdir;
    char *username = getcaller ();
    int fd;
    char *line;
    char *slash = "", *cp;
    const char *cp2, *repos;
    int i;
    static char *tilde = "";
    static char *PrCurDir = NULL;

    if (logoff)			/* History is turned off by noexec or
				 * readonlyfs.
				 */
	return;
    if ( strchr(logHistory, type) == NULL )	
	return;
    fname = xmalloc (strlen (current_parsed_root->directory) + sizeof (CVSROOTADM)
		     + sizeof (CVSROOTADM_HISTORY) + 3);
    (void) sprintf (fname, "%s/%s/%s", current_parsed_root->directory,
		    CVSROOTADM, CVSROOTADM_HISTORY);

    /* turn off history logging if the history file does not exist */
    /* FIXME:  This should check for write permissions instead.  This way,
     * O_CREATE could be added back into the call to open() below and
     * there would be no race condition involved in log rotation.
     *
     * Note that the new method of turning off logging would be either via
     * the CVSROOT/config file (probably the quicker method, but would need
     * to be added, or at least checked for, too) or by creating a dummy
     * history file with 0444 permissions.
     */
    if (!isfile (fname))
    {
	logoff = 1;
	goto out;
    }

    if (trace)
	fprintf (stderr, "%s-> fopen(%s,a)\n",
		 CLIENT_SERVER_STR, fname);
    if (noexec)
	goto out;
    fd = CVS_OPEN (fname, O_WRONLY | O_APPEND | OPEN_BINARY, 0666);
    if (fd < 0)
    {
	if (! really_quiet)
        {
            error (0, errno, "warning: cannot write to history file %s",
                   fname);
        }
        goto out;
    }

    repos = Short_Repository (repository);

    if (!PrCurDir)
    {
	char *pwdir;

	pwdir = get_homedir ();
	PrCurDir = CurDir;
	if (pwdir != NULL)
	{
	    /* Assumes neither CurDir nor pwdir ends in '/' */
	    i = strlen (pwdir);
	    if (!strncmp (CurDir, pwdir, i))
	    {
		PrCurDir += i;		/* Point to '/' separator */
		tilde = "~";
	    }
	    else
	    {
		/* Try harder to find a "homedir" */
		struct saved_cwd cwd;
		char *homedir;

		if (save_cwd (&cwd))
		    error_exit ();

		if ( CVS_CHDIR (pwdir) < 0 || (homedir = xgetwd ()) == NULL)
		    homedir = pwdir;

		if (restore_cwd (&cwd, NULL))
		    error_exit ();
		free_cwd (&cwd);

		i = strlen (homedir);
		if (!strncmp (CurDir, homedir, i))
		{
		    PrCurDir += i;	/* Point to '/' separator */
		    tilde = "~";
		}

		if (homedir != pwdir)
		    free (homedir);
	    }
	}
    }

    if (type == 'T')
    {
	repos = update_dir;
	update_dir = "";
    }
    else if (update_dir && *update_dir)
	slash = "/";
    else
	update_dir = "";

    workdir = xmalloc (strlen (tilde) + strlen (PrCurDir) + strlen (slash)
		       + strlen (update_dir) + 10);
    (void) sprintf (workdir, "%s%s%s%s", tilde, PrCurDir, slash, update_dir);

    /*
     * "workdir" is the directory where the file "name" is. ("^~" == $HOME)
     * "repos"	is the Repository, relative to $CVSROOT where the RCS file is.
     *
     * "$workdir/$name" is the working file name.
     * "$CVSROOT/$repos/$name,v" is the RCS file in the Repository.
     *
     * First, note that the history format was intended to save space, not
     * to be human readable.
     *
     * The working file directory ("workdir") and the Repository ("repos")
     * usually end with the same one or more directory elements.  To avoid
     * duplication (and save space), the "workdir" field ends with
     * an integer offset into the "repos" field.  This offset indicates the
     * beginning of the "tail" of "repos", after which all characters are
     * duplicates.
     *
     * In other words, if the "workdir" field has a '*' (a very stupid thing
     * to put in a filename) in it, then every thing following the last '*'
     * is a hex offset into "repos" of the first character from "repos" to
     * append to "workdir" to finish the pathname.
     *
     * It might be easier to look at an example:
     *
     *  M273b3463|dgg|~/work*9|usr/local/cvs/examples|1.2|loginfo
     *
     * Indicates that the workdir is really "~/work/cvs/examples", saving
     * 10 characters, where "~/work*d" would save 6 characters and mean that
     * the workdir is really "~/work/examples".  It will mean more on
     * directories like: usr/local/gnu/emacs/dist-19.17/lisp/term
     *
     * "workdir" is always an absolute pathname (~/xxx is an absolute path)
     * "repos" is always a relative pathname.  So we can assume that we will
     * never run into the top of "workdir" -- there will always be a '/' or
     * a '~' at the head of "workdir" that is not matched by anything in
     * "repos".  On the other hand, we *can* run off the top of "repos".
     *
     * Only "compress" if we save characters.
     */

    if (!repos)
	repos = "";

    cp = workdir + strlen (workdir) - 1;
    cp2 = repos + strlen (repos) - 1;
    for (i = 0; cp2 >= repos && cp > workdir && *cp == *cp2--; cp--)
	i++;

    if (i > 2)
    {
	i = strlen (repos) - i;
	(void) sprintf ((cp + 1), "*%x", i);
    }

    if (!revs)
	revs = "";
    line = xmalloc (strlen (username) + strlen (workdir) + strlen (repos)
		    + strlen (revs) + strlen (name) + 100);
    sprintf (line, "%c%08lx|%s|%s|%s|%s|%s\n",
	     type, (long) time ((time_t *) NULL),
	     username, workdir, repos, revs, name);

    /* Lessen some race conditions on non-Posix-compliant hosts.  */
    if (lseek (fd, (off_t) 0, SEEK_END) == -1)
	error (1, errno, "cannot seek to end of history file: %s", fname);

    if (write (fd, line, strlen (line)) < 0)
	error (1, errno, "cannot write to history file: %s", fname);
    free (line);
    if (close (fd) != 0)
	error (1, errno, "cannot close history file: %s", fname);
    free (workdir);
 out:
    free (fname);
}

/*
 * save_user() adds a user name to the user list to select.  Zero-length
 *		username ("") matches any user.
 */
static void
save_user (name)
    char *name;
{
    if (user_count == user_max)
    {
	user_max = xsum (user_max, USER_INCREMENT);
	if (size_overflow_p (xtimes (user_max, sizeof (char *))))
	{
	    error (0, 0, "save_user: too many users");
	    return;
	}
	user_list = xrealloc (user_list, xtimes (user_max, sizeof (char *)));
    }
    user_list[user_count++] = xstrdup (name);
}

/*
 * save_file() adds file name and associated module to the file list to select.
 *
 * If "dir" is null, store a file name as is.
 * If "name" is null, store a directory name with a '*' on the front.
 * Else, store concatenated "dir/name".
 *
 * Later, in the "select" stage:
 *	- if it starts with '*', it is prefix-matched against the repository.
 *	- if it has a '/' in it, it is matched against the repository/file.
 *	- else it is matched against the file name.
 */
static void
save_file (dir, name, module)
    char *dir;
    char *name;
    char *module;
{
    char *cp;
    struct file_list_str *fl;

    if (file_count == file_max)
    {
	file_max = xsum (file_max, FILE_INCREMENT);
	if (size_overflow_p (xtimes (file_max, sizeof (*fl))))
	{
	    error (0, 0, "save_file: too many files");
	    return;
	}
	file_list = xrealloc (file_list, xtimes (file_max, sizeof (*fl)));
    }
    fl = &file_list[file_count++];
    fl->l_file = cp = xmalloc (strlen (dir) + strlen (name) + 2);
    fl->l_module = module;

    if (dir && *dir)
    {
	if (name && *name)
	{
	    (void) strcpy (cp, dir);
	    (void) strcat (cp, "/");
	    (void) strcat (cp, name);
	}
	else
	{
	    *cp++ = '*';
	    (void) strcpy (cp, dir);
	}
    }
    else
    {
	if (name && *name)
	{
	    (void) strcpy (cp, name);
	}
	else
	{
	    error (0, 0, "save_file: null dir and file name");
	}
    }
}

static void
save_module (module)
    char *module;
{
    if (mod_count == mod_max)
    {
	mod_max = xsum (mod_max, MODULE_INCREMENT);
	if (size_overflow_p (xtimes (mod_max, sizeof (char *))))
	{
	    error (0, 0, "save_module: too many modules");
	    return;
	}
	mod_list = xrealloc (mod_list, xtimes (mod_max, sizeof (char *)));
    }
    mod_list[mod_count++] = xstrdup (module);
}

static void
expand_modules ()
{
}

/* fill_hrec
 *
 * Take a ptr to 7-part history line, ending with a newline, for example:
 *
 *	M273b3463|dgg|~/work*9|usr/local/cvs/examples|1.2|loginfo
 *
 * Split it into 7 parts and drop the parts into a "struct hrec".
 * Return a pointer to the character following the newline.
 * 
 */

#define NEXT_BAR(here) do { \
	while (isspace(*line)) line++; \
	hr->here = line; \
	while ((c = *line++) && c != '|') ; \
	if (!c) return; line[-1] = '\0'; \
	} while (0)

static void
fill_hrec (line, hr)
    char *line;
    struct hrec *hr;
{
    char *cp;
    int c;

    hr->type = hr->user = hr->dir = hr->repos = hr->rev = hr->file =
	hr->end = hr->mod = NULL;
    hr->date = -1;
    hr->idx = ++hrec_idx;

    while (isspace ((unsigned char) *line))
	line++;

    hr->type = line++;
    hr->date = strtoul (line, &cp, 16);
    if (cp == line || *cp != '|')
	return;
    line = cp + 1;
    NEXT_BAR (user);
    NEXT_BAR (dir);
    if ((cp = strrchr (hr->dir, '*')) != NULL)
    {
	*cp++ = '\0';
	hr->end = line + strtoul (cp, NULL, 16);
    }
    else
	hr->end = line - 1;		/* A handy pointer to '\0' */
    NEXT_BAR (repos);
    NEXT_BAR (rev);
    if (strchr ("FOET", *(hr->type)))
	hr->mod = line;

    NEXT_BAR (file);
}


#ifndef STAT_BLOCKSIZE
#if HAVE_STRUCT_STAT_ST_BLKSIZE
#define STAT_BLOCKSIZE(s) (s).st_blksize
#else
#define STAT_BLOCKSIZE(s) (4 * 1024)
#endif
#endif


/* read_hrecs's job is to read the history file and fill in all the "hrec"
 * (history record) array elements with the ones we need to print.
 *
 * Logic:
 * - Read a block from the file. 
 * - Walk through the block parsing line into hr records. 
 * - if the hr isn't used, free its strings, if it is, bump the hrec counter
 * - at the end of a block, copy the end of the current block to the start 
 * of space for the next block, then read in the next block.  If we get less
 * than the whole block, we're done. 
 */
static void
read_hrecs (fname)
    char *fname;
{
    unsigned char *cpstart, *cpend, *cp, *nl;
    char *hrline;
    int i;
    int fd;
    struct stat st_buf;

    if ((fd = CVS_OPEN (fname, O_RDONLY | OPEN_BINARY)) < 0)
	error (1, errno, "cannot open history file: %s", fname);

    if (fstat (fd, &st_buf) < 0)
	error (1, errno, "can't stat history file");

    if (!(st_buf.st_size))
	error (1, 0, "history file is empty");

    cpstart = xmalloc (2 * STAT_BLOCKSIZE(st_buf));
    cpstart[0] = '\0';
    cp = cpend = cpstart;

    hrec_max = HREC_INCREMENT;
    hrec_head = xmalloc (hrec_max * sizeof (struct hrec));
    hrec_idx = 0;

    for (;;)
    {
	for (nl = cp; nl < cpend && *nl != '\n'; nl++)
	    if (!isprint(*nl)) *nl = ' ';

	if (nl >= cpend)
	{
	    if (nl - cp >= STAT_BLOCKSIZE(st_buf))
	    {
		error(1, 0, "history line %ld too long (> %lu)", hrec_idx + 1,
		      (unsigned long) STAT_BLOCKSIZE(st_buf));
	    }
	    if (nl > cp)
		memmove (cpstart, cp, nl - cp);
	    nl = cpstart + (nl - cp);
	    cp = cpstart;
	    i = read (fd, nl, STAT_BLOCKSIZE(st_buf));
	    if (i > 0)
	    {
		cpend = nl + i;
		*cpend = '\0';
		continue;
	    }
	    if (i < 0)
		error (1, errno, "error reading history file");
	    if (nl == cp) break;
	    error (0, 0, "warning: no newline at end of history file");
	}
	*nl = '\0';

	if (hrec_count == hrec_max)
	{
	    struct hrec *old_head = hrec_head;

	    hrec_max += HREC_INCREMENT;
	    hrec_head = xrealloc ((char *) hrec_head,
				  hrec_max * sizeof (struct hrec));
	    if (last_since_tag)
		last_since_tag = hrec_head + (last_since_tag - old_head);
	    if (last_backto)
		last_backto = hrec_head + (last_backto - old_head);
	}

	/* fill_hrec dates from when history read the entire 
	   history file in one chunk, and then records were pulled out
	   by pointing to the various parts of this big chunk.  This is
	   why there are ugly hacks here:  I don't want to completely
	   re-write the whole history stuff right now.  */

	hrline = xstrdup ((char *)cp);
	fill_hrec (hrline, &hrec_head[hrec_count]);
	if (select_hrec (&hrec_head[hrec_count]))
	    hrec_count++;
	else 
	    free(hrline);

	cp = nl + 1;
    }
    free (cpstart);
    close (fd);

    /* Special selection problem: If "since_tag" is set, we have saved every
     * record from the 1st occurrence of "since_tag", when we want to save
     * records since the *last* occurrence of "since_tag".  So what we have
     * to do is bump hrec_head forward and reduce hrec_count accordingly.
     */
    if (last_since_tag)
    {
	hrec_count -= (last_since_tag - hrec_head);
	hrec_head = last_since_tag;
    }

    /* Much the same thing is necessary for the "backto" option. */
    if (last_backto)
    {
	hrec_count -= (last_backto - hrec_head);
	hrec_head = last_backto;
    }
}

/* Utility program for determining whether "find" is inside "string" */
static int
within (find, string)
    char *find, *string;
{
    int c, len;

    if (!find || !string)
	return (0);

    c = *find++;
    len = strlen (find);

    while (*string)
    {
	if (!(string = strchr (string, c)))
	    return (0);
	string++;
	if (!strncmp (find, string, len))
	    return (1);
    }
    return (0);
}

/* The purpose of "select_hrec" is to apply the selection criteria based on
 * the command arguments and defaults and return a flag indicating whether
 * this record should be remembered for printing.
 */
static int
select_hrec (hr)
    struct hrec *hr;
{
    char **cpp, *cp, *cp2;
    struct file_list_str *fl;
    int count;

    /* basic validity checking */
    if (!hr->type || !hr->user || !hr->dir || !hr->repos || !hr->rev ||
	!hr->file || !hr->end)
    {
	error (0, 0, "warning: history line %ld invalid", hr->idx);
	return (0);
    }

    /* "Since" checking:  The argument parser guarantees that only one of the
     *			  following four choices is set:
     *
     * 1. If "since_date" is set, it contains the date specified on the
     *    command line. hr->date fields earlier than "since_date" are ignored.
     * 2. If "since_rev" is set, it contains either an RCS "dotted" revision
     *    number (which is of limited use) or a symbolic TAG.  Each RCS file
     *    is examined and the date on the specified revision (or the revision
     *    corresponding to the TAG) in the RCS file (CVSROOT/repos/file) is
     *    compared against hr->date as in 1. above.
     * 3. If "since_tag" is set, matching tag records are saved.  The field
     *    "last_since_tag" is set to the last one of these.  Since we don't
     *    know where the last one will be, all records are saved from the
     *    first occurrence of the TAG.  Later, at the end of "select_hrec"
     *    records before the last occurrence of "since_tag" are skipped.
     * 4. If "backto" is set, all records with a module name or file name
     *    matching "backto" are saved.  In addition, all records with a
     *    repository field with a *prefix* matching "backto" are saved.
     *    The field "last_backto" is set to the last one of these.  As in
     *    3. above, "select_hrec" adjusts to include the last one later on.
     */
    if (since_date)
    {
	char *ourdate = date_from_time_t (hr->date);
	count = RCS_datecmp (ourdate, since_date);
	free (ourdate);
	if (count < 0)
	    return (0);
    }
    else if (*since_rev)
    {
	Vers_TS *vers;
	time_t t;
	struct file_info finfo;

	memset (&finfo, 0, sizeof finfo);
	finfo.file = hr->file;
	/* Not used, so don't worry about it.  */
	finfo.update_dir = NULL;
	finfo.fullname = finfo.file;
	finfo.repository = hr->repos;
	finfo.entries = NULL;
	finfo.rcs = NULL;

	vers = Version_TS (&finfo, (char *) NULL, since_rev, (char *) NULL,
			   1, 0);
	if (vers->vn_rcs)
	{
	    if ((t = RCS_getrevtime (vers->srcfile, vers->vn_rcs, (char *) 0, 0))
		!= (time_t) 0)
	    {
		if (hr->date < t)
		{
		    freevers_ts (&vers);
		    return (0);
		}
	    }
	}
	freevers_ts (&vers);
    }
    else if (*since_tag)
    {
	if (*(hr->type) == 'T')
	{
	    /*
	     * A 'T'ag record, the "rev" field holds the tag to be set,
	     * while the "repos" field holds "D"elete, "A"dd or a rev.
	     */
	    if (within (since_tag, hr->rev))
	    {
		last_since_tag = hr;
		return (1);
	    }
	    else
		return (0);
	}
	if (!last_since_tag)
	    return (0);
    }
    else if (*backto)
    {
	if (within (backto, hr->file) || within (backto, hr->mod) ||
	    within (backto, hr->repos))
	    last_backto = hr;
	else
	    return (0);
    }

    /* User checking:
     *
     * Run down "user_list", match username ("" matches anything)
     * If "" is not there and actual username is not there, return failure.
     */
    if (user_list && hr->user)
    {
	for (cpp = user_list, count = user_count; count; cpp++, count--)
	{
	    if (!**cpp)
		break;			/* null user == accept */
	    if (!strcmp (hr->user, *cpp))	/* found listed user */
		break;
	}
	if (!count)
	    return (0);			/* Not this user */
    }

    /* Record type checking:
     *
     * 1. If Record type is not in rec_types field, skip it.
     * 2. If mod_list is null, keep everything.  Otherwise keep only modules
     *    on mod_list.
     * 3. If neither a 'T', 'F' nor 'O' record, run through "file_list".  If
     *    file_list is null, keep everything.  Otherwise, keep only files on
     *    file_list, matched appropriately.
     */
    if (!strchr (rec_types, *(hr->type)))
	return (0);
    if (!strchr ("TFOE", *(hr->type)))	/* Don't bother with "file" if "TFOE" */
    {
	if (file_list)			/* If file_list is null, accept all */
	{
	    for (fl = file_list, count = file_count; count; fl++, count--)
	    {
		/* 1. If file_list entry starts with '*', skip the '*' and
		 *    compare it against the repository in the hrec.
		 * 2. If file_list entry has a '/' in it, compare it against
		 *    the concatenation of the repository and file from hrec.
		 * 3. Else compare the file_list entry against the hrec file.
		 */
		char *cmpfile = NULL;

		if (*(cp = fl->l_file) == '*')
		{
		    cp++;
		    /* if argument to -p is a prefix of repository */
		    if (!strncmp (cp, hr->repos, strlen (cp)))
		    {
			hr->mod = fl->l_module;
			break;
		    }
		}
		else
		{
		    if (strchr (cp, '/'))
		    {
			cmpfile = xmalloc (strlen (hr->repos)
					   + strlen (hr->file)
					   + 10);
			(void) sprintf (cmpfile, "%s/%s",
					hr->repos, hr->file);
			cp2 = cmpfile;
		    }
		    else
		    {
			cp2 = hr->file;
		    }

		    /* if requested file is found within {repos}/file fields */
		    if (within (cp, cp2))
		    {
			hr->mod = fl->l_module;
			break;
		    }
		    if (cmpfile != NULL)
			free (cmpfile);
		}
	    }
	    if (!count)
		return (0);		/* String specified and no match */
	}
    }
    if (mod_list)
    {
	for (cpp = mod_list, count = mod_count; count; cpp++, count--)
	{
	    if (hr->mod && !strcmp (hr->mod, *cpp))	/* found module */
		break;
	}
	if (!count)
	    return (0);	/* Module specified & this record is not one of them. */
    }

    return (1);		/* Select this record unless rejected above. */
}

/* The "sort_order" routine (when handed to qsort) has arranged for the
 * hrecs files to be in the right order for the report.
 *
 * Most of the "selections" are done in the select_hrec routine, but some
 * selections are more easily done after the qsort by "accept_hrec".
 */
static void
report_hrecs ()
{
    struct hrec *hr, *lr;
    struct tm *tm;
    int i, count, ty;
    char *cp;
    int user_len, file_len, rev_len, mod_len, repos_len;

    if (*since_tag && !last_since_tag)
    {
	(void) printf ("No tag found: %s\n", since_tag);
	return;
    }
    else if (*backto && !last_backto)
    {
	(void) printf ("No module, file or repository with: %s\n", backto);
	return;
    }
    else if (hrec_count < 1)
    {
	(void) printf ("No records selected.\n");
	return;
    }

    user_len = file_len = rev_len = mod_len = repos_len = 0;

    /* Run through lists and find maximum field widths */
    hr = lr = hrec_head;
    hr++;
    for (count = hrec_count; count--; lr = hr, hr++)
    {
	char *repos;

	if (!count)
	    hr = NULL;
	if (!accept_hrec (lr, hr))
	    continue;

	ty = *(lr->type);
	repos = xstrdup (lr->repos);
	if ((cp = strrchr (repos, '/')) != NULL)
	{
	    if (lr->mod && !strcmp (++cp, lr->mod))
	    {
		(void) strcpy (cp, "*");
	    }
	}
	if ((i = strlen (lr->user)) > user_len)
	    user_len = i;
	if ((i = strlen (lr->file)) > file_len)
	    file_len = i;
	if (ty != 'T' && (i = strlen (repos)) > repos_len)
	    repos_len = i;
	if (ty != 'T' && (i = strlen (lr->rev)) > rev_len)
	    rev_len = i;
	if (lr->mod && (i = strlen (lr->mod)) > mod_len)
	    mod_len = i;
	free (repos);
    }

    /* Walk through hrec array setting "lr" (Last Record) to each element.
     * "hr" points to the record following "lr" -- It is NULL in the last
     * pass.
     *
     * There are two sections in the loop below:
     * 1. Based on the report type (e.g. extract, checkout, tag, etc.),
     *    decide whether the record should be printed.
     * 2. Based on the record type, format and print the data.
     */
    for (lr = hrec_head, hr = (lr + 1); hrec_count--; lr = hr, hr++)
    {
	char *workdir;
	char *repos;

	if (!hrec_count)
	    hr = NULL;
	if (!accept_hrec (lr, hr))
	    continue;

	ty = *(lr->type);
	if (!tz_local)
	{
	    time_t t = lr->date + tz_seconds_east_of_GMT;
	    tm = gmtime (&t);
	}
	else
	    tm = localtime (&(lr->date));

	(void) printf ("%c %04d-%02d-%02d %02d:%02d %s %-*s", ty,
		  tm->tm_year+1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
		  tm->tm_min, tz_name, user_len, lr->user);

	workdir = xmalloc (strlen (lr->dir) + strlen (lr->end) + 10);
	(void) sprintf (workdir, "%s%s", lr->dir, lr->end);
	if ((cp = strrchr (workdir, '/')) != NULL)
	{
	    if (lr->mod && !strcmp (++cp, lr->mod))
	    {
		(void) strcpy (cp, "*");
	    }
	}
	repos = xmalloc (strlen (lr->repos) + 10);
	(void) strcpy (repos, lr->repos);
	if ((cp = strrchr (repos, '/')) != NULL)
	{
	    if (lr->mod && !strcmp (++cp, lr->mod))
	    {
		(void) strcpy (cp, "*");
	    }
	}

	switch (ty)
	{
	    case 'T':
		/* 'T'ag records: repository is a "tag type", rev is the tag */
		(void) printf (" %-*s [%s:%s]", mod_len, lr->mod, lr->rev,
			       repos);
		if (working)
		    (void) printf (" {%s}", workdir);
		break;
	    case 'F':
	    case 'E':
	    case 'O':
		if (lr->rev && *(lr->rev))
		    (void) printf (" [%s]", lr->rev);
		(void) printf (" %-*s =%s%-*s %s", repos_len, repos, lr->mod,
			       mod_len + 1 - (int) strlen (lr->mod),
			       "=", workdir);
		break;
	    case 'W':
	    case 'U':
	    case 'P':
	    case 'C':
	    case 'G':
	    case 'M':
	    case 'A':
	    case 'R':
		(void) printf (" %-*s %-*s %-*s =%s= %s", rev_len, lr->rev,
			       file_len, lr->file, repos_len, repos,
			       lr->mod ? lr->mod : "", workdir);
		break;
	    default:
		(void) printf ("Hey! What is this junk? RecType[0x%2.2x]", ty);
		break;
	}
	(void) putchar ('\n');
	free (workdir);
	free (repos);
    }
}

static int
accept_hrec (lr, hr)
    struct hrec *hr, *lr;
{
    int ty;

    ty = *(lr->type);

    if (last_since_tag && ty == 'T')
	return (1);

    if (v_checkout)
    {
	if (ty != 'O')
	    return (0);			/* Only interested in 'O' records */

	/* We want to identify all the states that cause the next record
	 * ("hr") to be different from the current one ("lr") and only
	 * print a line at the allowed boundaries.
	 */

	if (!hr ||			/* The last record */
	    strcmp (hr->user, lr->user) ||	/* User has changed */
	    strcmp (hr->mod, lr->mod) ||/* Module has changed */
	    (working &&			/* If must match "workdir" */
	     (strcmp (hr->dir, lr->dir) ||	/*    and the 1st parts or */
	      strcmp (hr->end, lr->end))))	/*    the 2nd parts differ */

	    return (1);
    }
    else if (modified)
    {
	if (!last_entry ||		/* Don't want only last rec */
	    !hr ||			/* Last entry is a "last entry" */
	    strcmp (hr->repos, lr->repos) ||	/* Repository has changed */
	    strcmp (hr->file, lr->file))/* File has changed */
	    return (1);

	if (working)
	{				/* If must match "workdir" */
	    if (strcmp (hr->dir, lr->dir) ||	/*    and the 1st parts or */
		strcmp (hr->end, lr->end))	/*    the 2nd parts differ */
		return (1);
	}
    }
    else if (module_report)
    {
	if (!last_entry ||		/* Don't want only last rec */
	    !hr ||			/* Last entry is a "last entry" */
	    strcmp (hr->mod, lr->mod) ||/* Module has changed */
	    strcmp (hr->repos, lr->repos) ||	/* Repository has changed */
	    strcmp (hr->file, lr->file))/* File has changed */
	    return (1);
    }
    else
    {
	/* "extract" and "tag_report" always print selected records. */
	return (1);
    }

    return (0);
}
