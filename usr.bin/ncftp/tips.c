/* tips.c */

/*  $RCSfile: tips.c,v $
 *  $Revision: 14020.11 $
 *  $Date: 93/05/21 05:44:39 $
 */

#include "sys.h"

#ifndef NO_TIPS

#include "util.h"

/* Make sure that the indentations are spaces, not tabs.
 * Try newform -i-4 < tips.c > tips.c.new
 *
 * Always add new tips right above the last one.
 */

static char *tiplist[] = {
    "Have you tried typing 'open' by itself lately?",

    "You know what?  You're using obselete software.  Ask your sysadmin \n\
	 to upgrade to a version of ncftp numbered 2.2 or higher.",

    "If you don't want a .ncrecent file in your home directory, put the \n\
     command '#unset recent-list' in your .ncftprc file.",

    "pseudo-filename-completion is supported in some commands.  To use it,\n\
     use a wildcard expression that will match exactly one file.  I.e., if you\n\
     want to fetch obnoxiouslylongfilename.zip, try 'get obn*.zip.'  Note that\n\
     you can't use the cd command with this feature (yet).",

    "You don't need to type the exact site name with open.  If a site is in\n\
     your .ncftprc or the recent-file (.ncrecent), just type a unique\n\
     abbreviation (substring really).   I.e. 'open wuar' if you have the site\n\
     wuarchive.wustl.edu in your rc or recent-file.",

    "You can put set commands in your .ncftprc, by adding lines such\n\
     as '#set local-dir /usr/tmp' to the file, which will be run at startup.",

    "Use the .ncftprc file to set variables at startup and to add sites that \n\
     need init macros.\n\
     Sample .ncftprc:\n\
     #set pager \"less -M\"\n\
     \n\
     machine wuarchive.wustl.edu\n\
         macdef init\n\
         cd /pub\n\
         get README\n\
         dir\n\
         (blank line to end macro)",
    
    "If you want to keep your .netrc's for ftp and ncftp separate, name\n\
     ncftp's rc to .ncftprc.",

    "Type 'open' by itself to get a list of the sites in your recent-file and\n\
     your .ncftprc.  You can then supply '#5' at the prompt, or use 'open #5'\n\
     later.",

    "Colon-mode is a quick way to get a file from your shell.  Try something\n\
     like 'ncftp wuarchive.wustl.edu:/pub/README.'",

    "The open command accepts several flags.  Do a 'help open' for details.",

    "Sometimes a directory listing is several screens long and you won't\n\
     remember the thing you wanted.  Use the 'predir' command to re-view the\n\
     listing.  The program keeps the copy locally, so you won't have to wait\n\
     for the remote server to re-send it to you.",

    "Use the 'page' (or 'more') command to view a remote file with your pager.",

    "ncftp may be keeping detailed information on everything you transfer.\n\
     Run the 'version' command and if you see SYSLOG, your actions are being\n\
     recorded on the system log.",

    "Try the 'redir' command to re-display the last directory listing (ls,\n\
     dir, ls -lrt, etc).  'predir' does the same, only with your pager.",

    "This program is pronounced Nik-F-T-P.  NCEMRSoft is Nik'-mer-soft.",

#ifdef GETLINE
    "NcFTP was compiled with the Getline command-line/history editor! (by\n\
     Chris Thewalt <thewalt@ce.berkeley.edu>).  To activate it, use the up\n\
     and down arrows to scroll through the history, and/or use EMACS-style\n\
     commands to edit the line.",
#endif

#ifdef READLINE
    "NcFTP was compiled with the GNU Readline command-line/history editor!\n\
     To activate it, use the up & down arrows to scroll through the history,\n\
     and/or use EMACS-style (or maybe VI-style) commands to edit the line.",
#endif

    "You can get the newest version of NcFTP from ftp.cs.unl.edu, in the\n\
     /pub/ncftp directory, AFTER business hours.",

    "The type of progress-meter that will be used depends if the remote host\n\
     supports the SIZE command, and whether your terminal is capable of ANSI\n\
     escape codes.",

    "To report a bug, mail your message to mgleason@cse.unl.edu.  Include the\n\
     output of the 'version' command in your message.  An easy way to do that\n\
     is to compose your message, then do a 'ncftp -H >> msg.'",

    "Don't put a site in your .ncftprc unless you want an 'init' macro.  The \n\
     recent-file saves sites with the last directory you were in, unlike \n\
     the rc file, while still letting you use sitename abbreviations.",

    "You can use World Wide Web style paths instead of colon-mode paths.\n\
     For example, if the colon-mode path was 'ftp.cs.unl.edu:pub/ncftp',\n\
     the WWW-style path would be 'ftp://ftp.cs.unl.edu/pub/ncftp'.",

    "Sick and tired of these tips?  Put '#unset tips' in your .ncftprc."
};

/* Not another dinky header, por favor. */
#define NTIPS ((int) (sizeof(tiplist) / sizeof(char *)))
void PrintTip(void);
extern int fromatty, debug;

int tips = 1;
#endif  /* NO_TIPS */

void PrintTip(void)
{
#ifndef NO_TIPS
    int cheap_rn, i, tn;
    string str;

    if (tips && fromatty) {
        cheap_rn = (int) getpid() % NTIPS;
        if (debug) {
            (void) printf("pid: %d;  ntips: %d\n", getpid(), NTIPS);
            (void) Gets("*** Tip# (-1 == all): ", str, sizeof(str));
            tn = atoi(str) - 1;
			if (tn == -1)
				tn = 0;
            if (tn < -1)
                for(i=0; i<NTIPS; i++)
                    (void) printf("Tip: %s\n", tiplist[i]);
            else if (tn < NTIPS)
                (void) printf("Tip: %s\n", tiplist[tn]);
        } else
            (void) printf("Tip: %s\n", tiplist[cheap_rn]);
    }
#endif  /* NO_TIPS */
}   /* PrintTip */

/* tips.c */
