/* ftprc.c */

/*  $RCSfile: ftprc.c,v $
 *  $Revision: 14020.11 $
 *  $Date: 93/07/09 10:58:37 $
 */

#include "sys.h"

#include <sys/stat.h>

#include <ctype.h>
#include <signal.h>

#include "util.h"
#include "ftprc.h"
#include "main.h"
#include "cmds.h"
#include "set.h"
#include "defaults.h"
#include "copyright.h"

/* ftprc.c global variables */
siteptr					firstsite = NULL, lastsite = NULL;
recentsite				recents[dMAXRECENTS];
int						nRecents = 0;
int						nSites = 0;
int						keep_recent = dRECENT_ON;
longstring				rcname;
longstring				recent_file;
int						parsing_rc = 0;

extern char				*line, *margv[];
extern int				margc, fromatty;
extern string			anon_password;		/* most likely your email address */
extern string			pager;
extern struct userinfo	uinfo;

int thrash_rc(void)
{
	struct stat			st;
	string				word, str;
	longstring			cwd;
	char				*cp, *dp, *rc;
	FILE				*fp;
	int					i;

	(void) get_cwd(cwd, sizeof(cwd));
	if (cwd[strlen(cwd) - 1] != '/')
		(void) Strncat(cwd, "/");

	/* Because some versions of regular ftp complain about ncftp's
	 * #set commands, FTPRC takes precedence over NETRC.
	 */
	cp = getenv("DOTDIR");
	for (i=0; i<2; i++) {
		rc = (i == 0) ? FTPRC : NETRC;

		(void) sprintf(rcname, "%s%s", cwd, rc);
		if (stat(rcname, &st) == 0)
			goto foundrc;
		
		(void) sprintf(rcname, "%s.%s", cwd, rc);
		if (stat(rcname, &st) == 0)
			goto foundrc;

		if (cp != NULL) {
			(void) sprintf(rcname, "%s/.%s", cp, rc);
			if (stat(rcname, &st) == 0)
				goto foundrc;
		}

		(void) sprintf(rcname, "%s/.%s", uinfo.homedir, rc);
		if (stat(rcname, &st) == 0)
			goto foundrc;
	}

	return (0);	/* it's OK not to have an rc. */
	
foundrc:	
	if ((st.st_mode & 077) != 0)				/* rc must be unreadable by others. */
		(void) chmod(rcname, 0600);

	if ((fp = fopen(rcname, "r")) == NULL) {
		PERROR("thrash_rc", rcname);
		return -1;
	}
	
	parsing_rc = 1;
	while ((cp = FGets(str, fp)) != 0) {
		while (isspace(*cp)) ++cp;		/* skip leading space. */
		if (*cp == '#') {
			if ((strncmp("set", ++cp, (size_t)3) == 0) || (strncmp("unset", cp, (size_t)5) == 0)) {
				(void) strcpy(line, cp);
				makeargv();
				(void) set(margc, margv);			
				/* setting or unsetting a variable. */
			} /* else a comment. */
		} else {
			if (strncmp(cp, "machine", (size_t) 7) == 0) {
				/* We have a new machine record. */
				cp += 7;
				while (isspace(*cp)) ++cp;	/* skip delimiting space. */
				dp = word;
				while (*cp && !isspace(*cp)) *dp++ = *cp++;	/* copy the name. */
				*dp = 0;
				AddNewSitePtr(word);
			}
		}
	}
	(void) fclose(fp);
	parsing_rc = 0;
	return 1;
}	/* thrash_rc */




void AddNewSitePtr(char *word)
{
	siteptr			s;

	if ((s = (siteptr) malloc(sizeof(site))) != 0) {
		s->next = NULL;
		if ((s->name = malloc(strlen(word) + 1)) != 0) {
			(void) strcpy(s->name, word);
			if (firstsite == NULL)
				firstsite = lastsite = s;
			else {
				lastsite->next = s;
				lastsite = s;
			}
			++nSites;
		} else {
			Free(s);
		}
	}
}	/* AddNewSitePtr */




static int RecentCmp(recentsite *a, recentsite *b)
{
	int i = 1;
	
	if (a->lastcall > b->lastcall)
		i = -1;
	else if (a->lastcall == b->lastcall)
		i = 0;
	return i;
}	/* RecentCmp */




static siteptr FindNetrcSite(char *host, int exact)
{
	register siteptr s, s2;
	string str, host2;

	(void) Strncpy(host2, host);
	StrLCase(host2);
	
	/* see if 'host' is in our list of favorite sites (in NETRC). */
	for (s = firstsite; s != NULL; s2=s->next, s=s2) {
		(void) Strncpy(str, s->name);
		StrLCase(str);
		if (exact) {
			if (strcmp(str, host2) == 0)
				return s;
		} else {
			if (strstr(str, host2) != NULL) 
				return s;
		}
	}
	return NULL;
}	/* FindNetrcSite */




static recentsite *FindRecentSite(char *host, int exact)
{
	register recentsite		*r;
	register int			i;
	string					str, host2;

	(void) Strncpy(host2, host);
	StrLCase(host2);

	/* see if 'host' is in our list of favorite sites (in recent-log). */
	for (i=0; i<nRecents; i++) {
		r = &recents[i];
		(void) Strncpy(str, r->name);
		StrLCase(str);
		if (exact) {
			if (strcmp(str, host2) == 0)
				return r;
		} else {
			if (strstr(str, host2) != NULL)
				return r;
		}
	}
	return NULL;
}	/* FindRecentSite */




void ReadRecentSitesFile(void)
{
	FILE *rfp;
	recentsite *r;
	char name[64];
	int offset;
	longstring str;

	nRecents = 0;
	if (recent_file[0] != 0 && keep_recent) {
		rfp = fopen(recent_file, "r");
		if (rfp != NULL) {
			for (; nRecents < dMAXRECENTS; ) {
				r = &recents[nRecents];
				if (FGets(str, rfp) == NULL)
					break;
				(void) RemoveTrailingNewline(str, NULL);
				name[0] = 0;
				offset = 45;
				if (sscanf(str, "%s %lu %n",
					name,
					(unsigned long *) &r->lastcall,
					&offset) >= 2)
				{
					if ((r->name = NewString(name)) != NULL) {
						r->dir = NewString(str + offset);
						if (r->dir != NULL)
							nRecents++;
						else {
							free(r->name);
							r->name = r->dir = NULL;
						}
					}
				}
			}
			(void) fclose(rfp);
		}
	}
}	/* ReadRecentSitesFile */



static void SortRecentList(void)
{
	QSort(recents, nRecents, sizeof(recentsite), RecentCmp);
}	/* SortRecentList */




int WriteRecentSitesFile(void)
{
	FILE			*rfp;
	recentsite		*r;
	int				i;
	int retcode = 0;

	if ((recent_file[0] != 0) && (nRecents > 0) && (keep_recent)) {
		dbprintf("Attempting to write %s...\n", recent_file);
		rfp = fopen(recent_file, "w");
		if (rfp != NULL) {
			SortRecentList();
			for (i=0; i<nRecents; i++) {
				r = &recents[i];
				(void) fprintf(rfp, "%-32s %11lu %s\n", r->name,
					(unsigned long) r->lastcall, r->dir);
			}
			(void) fclose(rfp);
			dbprintf("%s written successfully.\n", recent_file);
			(void) chmod(recent_file, 0600);
		} else {
			perror(recent_file);
			++retcode;
		}
	}
	return retcode;
}	/* WriteRecentSitesFile */




void AddRecentSite(char *host, char *lastdir)
{
	char			*nhost, *ndir;
	recentsite		*r;
	
	if (keep_recent) {
		nhost = NewString(host);
		/* Use '/' to denote that the current directory wasn't known,
		 * because we won't try to cd to the root directory.
		 */
		ndir = NewString(*lastdir ? lastdir : "/");
		
		/* Don't bother if we don't have the memory, or if it is already
		 * in our NETRC.
		 */
		if ((ndir != NULL) && (nhost != NULL) &&
			(FindNetrcSite(host, 1) == NULL)) {
			if (nRecents == dMAXRECENTS) {
				SortRecentList();
				r = &recents[dMAXRECENTS - 1];
				if (r->name != NULL)
					free(r->name);
				if (r->dir != NULL)
					free(r->dir);
			} else {
				r = &recents[nRecents];
				nRecents++;
			}
			r->name = nhost;			
			r->dir = ndir;
			(void) time(&r->lastcall);
			SortRecentList();
		}
	}
}	/* AddRecentSite */




/*
 * After you are done with a site (by closing it or quitting), we
 * need to update the list of recent sites called.
 */
void UpdateRecentSitesList(char *host, char *lastdir)
{
	recentsite *r;
	char *ndir;

	if (keep_recent) {	
		r = FindRecentSite(host, 1);
		if (r == NULL)
			AddRecentSite(host, lastdir);
		else {
			/* Update the last time connected, and the directory we left in. */
			if ((ndir = NewString(*lastdir ? lastdir : "/")) != NULL) {
				free(r->dir);
				r->dir = ndir;
			}
			(void) time(&r->lastcall);
		}
	}
}	/* UpdateRecentSitesList */



/*
 * Prints out the number of sites we know about, so the user can figure out
 * an abbreviation or type it's number to open (setpeer).
 */
void PrintSiteList(void)
{
	int						i, j;
	siteptr					s, s2;
	FILE					*pagerfp;
	Sig_t					sigpipe;

	if (fromatty) {
		j = 0;
		i = 1;
		sigpipe = Signal(SIGPIPE, SIG_IGN);
		if ((pagerfp = popen(pager + 1, "w")) == NULL)
			pagerfp = stdout;
		if (nRecents > 0) {
			j++;
			(void) fprintf(pagerfp, "\nRecently called sites:\n");
			for (; i<=nRecents; i++) {
				(void) fprintf(pagerfp, "%4d. %-32s", i, recents[i-1].name);
				i++;
				if (i <= nRecents) {
					(void) fprintf(pagerfp, "%5d. %-32s", i, recents[i-1].name);
				} else {
					(void) fprintf(pagerfp, "\n");
					break;
				}
				(void) fprintf(pagerfp, "\n");
			}
		}
		if (nSites > 0) {
			j++;
			(void) fprintf(pagerfp, "Sites in your netrc (%s):\n", rcname);
			for (s = firstsite; s != NULL; s2=s->next, s=s2, ++i) {
				(void) fprintf(pagerfp, "%4d. %-32s", i, s->name);
				s2=s->next;
				s=s2;
				i++;
				if (s != NULL) {
					(void) fprintf(pagerfp, "%5d. %-32s", i, s->name);
				} else {
					(void) fprintf(pagerfp, "\n");
					break;
				}
				(void) fprintf(pagerfp, "\n");
			}
		}
		if (j > 0) {
			(void) fprintf(pagerfp, "\
Note that you can specify an abbreviation of any name, or #x, where x is the\n\
number of the site you want to connect to.\n\n");
		}

		if (pagerfp != stdout)
			(void) pclose(pagerfp);
		Signal(SIGPIPE, sigpipe);
	}
}	/* PrintRecentSiteList */




/*
 * Given a sitename, check to see if the name was really an abbreviation
 * of a site in the NETRC, or a site in our list of recently connected
 * sites.  Also check if the name was in the format #x, where x which
 * would mean to use recents[x].name as the site; if x was greater than
 * the number of sites in the recent list, then index into the netrc
 * site list.
 */
#include <netdb.h>
void GetFullSiteName(char *host, char *lastdir)
{
	register siteptr		s, s2;
	register recentsite		*r;
	char					*ndir, *nhost, *cp;
	int						x, i, isAllDigits, exact;
	struct hostent			*hostentp;

	ndir = nhost = NULL;
	x = exact = 0;

	/* First, see if the "abbreviation" is really just the name of
	 * a machine in the local domain, or if it was a full hostname
	 * already.  That way we can avoid problems associated with
	 * short names, such as having "ce" as a machine in the local
	 * domain, but having "faces.unl.edu", where we would most likely
	 * want to use ce instead of faces if the user said "open ce".
	 * This will also prevent problems when you have a host named
	 * xx.yy.unl.edu, and another host named yy.unl.edu.  If the user
	 * said "open yy.unl.edu" we should use yy.unl.edu, and not look
	 * for matches containing yy.unl.edu.
	 */
	if ((hostentp = gethostbyname(host)) != NULL) {
		strcpy(host, hostentp->h_name);
		exact = 1;
	}

	/* Don't allow just numbers as abbreviations;  "open 2" could be
	 * confused between site numbers in the open 'menu,' like
	 * "2. unlinfo.unl.edu" and IP numbers "128.93.2.1" or even numbers
	 * in the site name like "simtel20.army.mil."
	 */
	
	for (isAllDigits = 1, cp = host; *cp != 0; cp++) {
		if (!isdigit(*cp)) {
			isAllDigits = 0;
			break;
		}
	}

	if (!isAllDigits) {
		if (host[0] == '#')
			(void) sscanf(host + 1, "%d", &x);
		/* Try matching the abbreviation, since it isn't just a number. */
		/* see if 'host' is in our list of favorite sites (in NETRC). */

		if (x == 0) {
			if ((s = FindNetrcSite(host, exact)) != NULL) {
				nhost = s->name;
			} else if ((r = FindRecentSite(host, exact)) != NULL) {
				nhost = r->name;
				ndir = r->dir;
			}
		}
	} else if (sscanf(host, "%d", &x) != 1) {
		x = 0;
	}

	if (--x >= 0) {
		if (x < nRecents) {
			nhost = recents[x].name;
			ndir = recents[x].dir;
		} else {
			x -= nRecents;
			if (x < nSites) {
				for (i = 0, s = firstsite; i < x; s2=s->next, s=s2)
					++i;				
				nhost = s->name;
			}
		}
	}

	if (nhost != NULL) {
		(void) strcpy(host, nhost);
		if (lastdir != NULL) {
			*lastdir = 0;
			/* Don't cd if the dir is the root directory. */
			if (ndir != NULL && (strcmp("/", ndir) != 0))
				(void) strcpy(lastdir, ndir);
		}
	}
}	/* GetFullSiteName */




int ruserpass2(char *host, char **username, char **pass, char **acct)
{
	FILE			*fp;
	char			*cp, *dp, *dst, *ep;
	str32			macname;
	char			*varname;
	int				site_found;
	string			str;
	static string	auser;
	static str32	apass, aacct;

	site_found = 0;

	if ((fp = fopen(rcname, "r")) != NULL) {
		parsing_rc = 1;
		while (FGets(str, fp)) {
			if ((cp = strstr(str, "machine")) != 0) {
				/* Look for the machine token. */
				cp += 7;
				while (isspace(*cp))
					cp++;
			} else
				continue;
			if (strncmp(host, cp, strlen(host)) == 0) {
				site_found = 1;
				while (!isspace(*cp))
					++cp;		/* skip the site name. */
				do {
					/* Skip any comments ahead of time. */
					for (dp=cp; *dp; dp++) {
						if (*dp == '#') {
							*dp = 0;
							break;
						}
					}

					ep = cp;
					while (1) {
						varname = strtok(ep, RC_DELIM);
						if (!varname) break;
						dst = ep = NULL;
						switch (*varname) {
							case 'u':	/* user */
								*username = dst = auser;
								break;
							case 'l':	/* login */
								*username = dst = auser;
								break;
							case 'p':	/* password */
								*pass = dst = apass;
								break;
							case 'a':	/* account */
								*acct = dst = aacct;
								break;
						/*	case 'd':  /o default */
						/* unused -- use 'set anon_password.' */
							case 'm':	/* macdef or machine */
								if (strcmp(varname, "macdef"))
									goto done;	/* new machine record... */
								dst = macname;
								break;
							default:
								(void) fprintf(stderr, "Unknown .netrc keyword \"%s\"\n",
									varname
								);
						}
						if (dst) {
							dp = strtok(ep, RC_DELIM);
							if (dp)
								(void) strcpy(dst, dp);
							if (dst == macname) {
								/*
								 *	Read in the lines of the macro.
								 *	The macro's end is denoted by
								 *	a blank line.
								 */
								(void) make_macro(macname, fp);
								goto nextline;
							}
						}
					}
nextline: ;
				} while ((cp = FGets(str, fp)) != 0);
				break;
			}		/* end if we found the machine record. */
		}
done:
		parsing_rc = 0;
		(void) fclose(fp);
	}

	if (!site_found) {
		/* didn't find it in the rc. */
		return (0);
	}

	return (1);	/* found */
}	/* ruserpass2 */

/* eof ftprc.c */
