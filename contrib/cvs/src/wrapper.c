/* This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#include "cvs.h"
#include "getline.h"

/*
  Original Author:  athan@morgan.com <Andrew C. Athan> 2/1/94
  Modified By:      vdemarco@bou.shl.com

  This package was written to support the NEXTSTEP concept of
  "wrappers."  These are essentially directories that are to be
  treated as "files."  This package allows such wrappers to be
  "processed" on the way in and out of CVS.  The intended use is to
  wrap up a wrapper into a single tar, such that that tar can be
  treated as a single binary file in CVS.  To solve the problem
  effectively, it was also necessary to be able to prevent rcsmerge
  application at appropriate times.

  ------------------
  Format of wrapper file ($CVSROOT/CVSROOT/cvswrappers or .cvswrappers)

  wildcard	[option value][option value]...

  where option is one of
  -f		from cvs filter		value: path to filter
  -t		to cvs filter		value: path to filter
  -m		update methodology	value: MERGE or COPY
  -k		default -k rcs option to use on import or add

  and value is a single-quote delimited value.

  E.g:
  *.nib		-f 'gunzipuntar' -t 'targzip' -m 'COPY'
*/


typedef struct {
    char *wildCard;
    char *tocvsFilter;
    char *fromcvsFilter;
    char *rcsOption;
    WrapMergeMethod mergeMethod;
} WrapperEntry;

static WrapperEntry **wrap_list=NULL;
static WrapperEntry **wrap_saved_list=NULL;

static int wrap_size=0;
static int wrap_count=0;
static int wrap_tempcount=0;

/* FIXME: wrap_saved_count is never set to any non-zero value.
   wrap_name_has and wrap_matching_entry should be using
   wrap_tempcount instead.  I believe the consequence of this is that
   .cvswrappers files are ignored (that was my own experience when I
   tried to use one).  If this bug is fixed, would be nice to write a
   sanity.sh testcase for .cvswrappers files.  */

static int wrap_saved_count=0;

static int wrap_saved_tempcount=0;

#define WRAPPER_GROW	8

void wrap_add_entry PROTO((WrapperEntry *e,int temp));
void wrap_kill PROTO((void));
void wrap_kill_temp PROTO((void));
void wrap_free_entry PROTO((WrapperEntry *e));
void wrap_free_entry_internal PROTO((WrapperEntry *e));
void wrap_restore_saved PROTO((void));

void wrap_setup()
{
    struct passwd *pw;

#ifdef CLIENT_SUPPORT
    if (!client_active)
#endif
    {
	char *file;

	file = xmalloc (strlen (CVSroot_directory)
			+ sizeof (CVSROOTADM)
			+ sizeof (CVSROOTADM_WRAPPER)
			+ 10);
	/* Then add entries found in repository, if it exists.  */
	(void) sprintf (file, "%s/%s/%s", CVSroot_directory, CVSROOTADM,
			CVSROOTADM_WRAPPER);
	if (isfile (file))
	{
	    wrap_add_file(file,0);
	}
	free (file);
    }

    /* Then add entries found in home dir, (if user has one) and file
       exists.  (FIXME: I think this probably should be using
       get_homedir, i.e. $HOME).  */
    if ((pw = (struct passwd *) getpwuid (getuid ())) && pw->pw_dir)
    {
	char *file;

	file = xmalloc (strlen (pw->pw_dir) + sizeof (CVSDOTWRAPPER) + 10);
	(void) sprintf (file, "%s/%s", pw->pw_dir, CVSDOTWRAPPER);
	if (isfile (file))
	{
	    wrap_add_file (file, 0);
	}
	free (file);
    }

    /* Then add entries found in CVSWRAPPERS environment variable. */
    wrap_add (getenv (WRAPPER_ENV), 0);
}

#ifdef CLIENT_SUPPORT
/* Send -W arguments for the wrappers to the server.  The command must
   be one that accepts them (e.g. update, import).  */
void
wrap_send ()
{
    int i;

    for (i = 0; i < wrap_count + wrap_tempcount; ++i)
    {
	if (wrap_list[i]->tocvsFilter != NULL
	    || wrap_list[i]->fromcvsFilter != NULL)
	    /* For greater studliness we would print the offending option
	       and (more importantly) where we found it.  */
	    error (0, 0, "\
-t and -f wrapper options are not supported remotely; ignored");
	if (wrap_list[i]->mergeMethod == WRAP_COPY)
	    /* For greater studliness we would print the offending option
	       and (more importantly) where we found it.  */
	    error (0, 0, "\
-m wrapper option is not supported remotely; ignored");
	if (wrap_list[i]->rcsOption != NULL)
	{
	    send_to_server ("Argument -W\012Argument ", 0);
	    send_to_server (wrap_list[i]->wildCard, 0);
	    send_to_server (" -k '", 0);
	    send_to_server (wrap_list[i]->rcsOption, 0);
	    send_to_server ("'\012", 0);
	}
    }
}
#endif /* CLIENT_SUPPORT */

/*
 * Open a file and read lines, feeding each line to a line parser. Arrange
 * for keeping a temporary list of wrappers at the end, if the "temp"
 * argument is set.
 */
void
wrap_add_file (file, temp)
    const char *file;
    int temp;
{
    FILE *fp;
    char *line = NULL;
    size_t line_allocated = 0;

    wrap_restore_saved ();
    wrap_kill_temp ();

    /* Load the file.  */
    fp = CVS_FOPEN (file, "r");
    if (fp == NULL)
    {
	if (!existence_error (errno))
	    error (0, errno, "cannot open %s", file);
	return;
    }
    while (getline (&line, &line_allocated, fp) >= 0)
	wrap_add (line, temp);
    if (line)
        free (line);
    if (ferror (fp))
	error (0, errno, "cannot read %s", file);
    if (fclose (fp) == EOF)
	error (0, errno, "cannot close %s", file);
}

void
wrap_kill()
{
    wrap_kill_temp();
    while(wrap_count)
	wrap_free_entry(wrap_list[--wrap_count]);
}

void
wrap_kill_temp()
{
    WrapperEntry **temps=wrap_list+wrap_count;

    while(wrap_tempcount)
	wrap_free_entry(temps[--wrap_tempcount]);
}

void
wrap_free_entry(e)
     WrapperEntry *e;
{
    wrap_free_entry_internal(e);
    free(e);
}

void
wrap_free_entry_internal(e)
    WrapperEntry *e;
{
    free (e->wildCard);
    if (e->tocvsFilter)
	free (e->tocvsFilter);
    if (e->fromcvsFilter)
	free (e->fromcvsFilter);
    if (e->rcsOption)
	free (e->rcsOption);
}

void
wrap_restore_saved()
{
    if(!wrap_saved_list)
	return;

    wrap_kill();

    free(wrap_list);

    wrap_list=wrap_saved_list;
    wrap_count=wrap_saved_count;
    wrap_tempcount=wrap_saved_tempcount;

    wrap_saved_list=NULL;
    wrap_saved_count=0;
    wrap_saved_tempcount=0;
}

void
wrap_add (line, isTemp)
   char *line;
   int         isTemp;
{
    char *temp;
    char ctemp;
    WrapperEntry e;
    char opt;

    if (!line || line[0] == '#')
	return;

    memset (&e, 0, sizeof(e));

	/* Search for the wild card */
    while(*line && isspace(*line))
	++line;
    for(temp=line;*line && !isspace(*line);++line)
	;
    if(temp==line)
	return;

    ctemp=*line;
    *line='\0';

    e.wildCard=xstrdup(temp);
    *line=ctemp;

    while(*line){
	    /* Search for the option */
	while(*line && *line!='-')
	    ++line;
	if(!*line)
	    break;
	++line;
	if(!*line)
	    break;
	opt=*line;

	    /* Search for the filter commandline */
	for(++line;*line && *line!='\'';++line);
	if(!*line)
	    break;

	for(temp=++line;*line && (*line!='\'' || line[-1]=='\\');++line)
	    ;

	/* This used to "break;" (ignore the option) if there was a
	   single character between the single quotes (I'm guessing
	   that was accidental).  Now it "break;"s if there are no
	   characters.  I'm not sure either behavior is particularly
	   necessary--the current options might not require ''
	   arguments, but surely some future option legitimately
	   might.  Also I'm not sure that ignoring the option is a
	   swift way to handle syntax errors in general.  */
	if (line==temp)
	    break;

	ctemp=*line;
	*line='\0';
	switch(opt){
	case 'f':
	    if(e.fromcvsFilter)
		free(e.fromcvsFilter);
	    /* FIXME: error message should say where the bad value
	       came from.  */
	    e.fromcvsFilter=expand_path (temp, "<wrapper>", 0);
            if (!e.fromcvsFilter)
		error (1, 0, "Correct above errors first");
	    break;
	case 't':
	    if(e.tocvsFilter)
		free(e.tocvsFilter);
	    /* FIXME: error message should say where the bad value
	       came from.  */
	    e.tocvsFilter=expand_path (temp, "<wrapper>", 0);
            if (!e.tocvsFilter)
		error (1, 0, "Correct above errors first");
	    break;
	case 'm':
	    /* FIXME: look into whether this option is still relevant given
	       the 24 Jun 96 change to merge_file.  */
	    if(*temp=='C' || *temp=='c')
		e.mergeMethod=WRAP_COPY;
	    else
		e.mergeMethod=WRAP_MERGE;
	    break;
	case 'k':
	    if (e.rcsOption)
		free (e.rcsOption);
	    e.rcsOption = xstrdup (temp);
	    break;
	default:
	    break;
	}
	*line=ctemp;
	if(!*line)break;
	++line;
    }

    wrap_add_entry(&e, isTemp);
}

void
wrap_add_entry(e, temp)
    WrapperEntry *e;
    int temp;
{
    int x;
    if(wrap_count+wrap_tempcount>=wrap_size){
	wrap_size += WRAPPER_GROW;
	wrap_list = (WrapperEntry **) xrealloc ((char *) wrap_list,
						wrap_size *
						sizeof (WrapperEntry *));
    }

    if(!temp && wrap_tempcount){
	for(x=wrap_count+wrap_tempcount-1;x>=wrap_count;--x)
	    wrap_list[x+1]=wrap_list[x];
    }

    x=(temp ? wrap_count+(wrap_tempcount++):(wrap_count++));
    wrap_list[x]=(WrapperEntry *)xmalloc(sizeof(WrapperEntry));
    wrap_list[x]->wildCard=e->wildCard;
    wrap_list[x]->fromcvsFilter=e->fromcvsFilter;
    wrap_list[x]->tocvsFilter=e->tocvsFilter;
    wrap_list[x]->mergeMethod=e->mergeMethod;
    wrap_list[x]->rcsOption = e->rcsOption;
}

/* Return 1 if the given filename is a wrapper filename */
int
wrap_name_has (name,has)
    const char   *name;
    WrapMergeHas  has;
{
    int x,count=wrap_count+wrap_saved_count;
    char *temp;

    for(x=0;x<count;++x)
	if (CVS_FNMATCH (wrap_list[x]->wildCard, name, 0) == 0){
	    switch(has){
	    case WRAP_TOCVS:
		temp=wrap_list[x]->tocvsFilter;
		break;
	    case WRAP_FROMCVS:
		temp=wrap_list[x]->fromcvsFilter;
		break;
	    case WRAP_RCSOPTION:
		temp = wrap_list[x]->rcsOption;
		break;
	    default:
	        abort ();
	    }
	    if(temp==NULL)
		return (0);
	    else
		return (1);
	}
    return (0);
}

static WrapperEntry *wrap_matching_entry PROTO ((const char *));

static WrapperEntry *
wrap_matching_entry (name)
    const char *name;
{
    int x,count=wrap_count+wrap_saved_count;

    for(x=0;x<count;++x)
	if (CVS_FNMATCH (wrap_list[x]->wildCard, name, 0) == 0)
	    return wrap_list[x];
    return (WrapperEntry *)NULL;
}

/* Return the RCS options for FILENAME in a newly malloc'd string.  If
   ASFLAG, then include "-k" at the beginning (e.g. "-kb"), otherwise
   just give the option itself (e.g. "b").  */
char *
wrap_rcsoption (filename, asflag)
    const char *filename;
    int asflag;
{
    WrapperEntry *e = wrap_matching_entry (filename);
    char *buf;

    if (e == NULL || e->rcsOption == NULL || (*e->rcsOption == '\0'))
	return NULL;

    buf = xmalloc (strlen (e->rcsOption) + 3);
    if (asflag)
    {
	strcpy (buf, "-k");
	strcat (buf, e->rcsOption);
    }
    else
    {
	strcpy (buf, e->rcsOption);
    }
    return buf;
}

char *
wrap_tocvs_process_file(fileName)
    const char *fileName;
{
    WrapperEntry *e=wrap_matching_entry(fileName);
    static char *buf = NULL;

    if(e==NULL || e->tocvsFilter==NULL)
	return NULL;

    if (buf != NULL)
	free (buf);
    buf = cvs_temp_name ();

    run_setup(e->tocvsFilter,fileName,buf);
    run_exec(RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL|RUN_REALLY );

    return buf;
}

int
wrap_merge_is_copy (fileName)
    const char *fileName;
{
    WrapperEntry *e=wrap_matching_entry(fileName);
    if(e==NULL || e->mergeMethod==WRAP_MERGE)
	return 0;

    return 1;
}

void
wrap_fromcvs_process_file(fileName)
    const char *fileName;
{
    WrapperEntry *e=wrap_matching_entry(fileName);

    if(e==NULL || e->fromcvsFilter==NULL)
	return;

    run_setup(e->fromcvsFilter,fileName);
    run_exec(RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL );
    return;
}
