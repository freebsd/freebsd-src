#include "cvs.h"

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

  and value is a single-quote delimited value.

  E.g:
  *.nib		-f 'gunzipuntar' -t 'targzip' -m 'COPY'
*/


typedef struct {
    char *wildCard;
    char *tocvsFilter;
    char *fromcvsFilter;
    char *conflictHook;
    WrapMergeMethod mergeMethod;
} WrapperEntry;

static WrapperEntry **wrap_list=NULL;
static WrapperEntry **wrap_saved_list=NULL;

static int wrap_size=0;
static int wrap_count=0;
static int wrap_tempcount=0;
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
    char file[PATH_MAX];
    struct passwd *pw;

	/* Then add entries found in repository, if it exists */
    (void) sprintf (file, "%s/%s/%s", CVSroot, CVSROOTADM, CVSROOTADM_WRAPPER);
    if (isfile (file)){
	wrap_add_file(file,0);
    }

	/* Then add entries found in home dir, (if user has one) and file exists */
    if ((pw = (struct passwd *) getpwuid (getuid ())) && pw->pw_dir){
	(void) sprintf (file, "%s/%s", pw->pw_dir, CVSDOTWRAPPER);
	if (isfile (file)){
	    wrap_add_file (file, 0);
	}
    }

	/* Then add entries found in CVSWRAPPERS environment variable. */
    wrap_add (getenv (WRAPPER_ENV), 0);
}

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
    char line[1024];

    wrap_restore_saved();
    wrap_kill_temp();

	/* load the file */
    if (!(fp = fopen (file, "r")))
	return;
    while (fgets (line, sizeof (line), fp))
	wrap_add (line, temp);
    (void) fclose (fp);
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
    free(e->wildCard);
    if(e->tocvsFilter)
	free(e->tocvsFilter);
    if(e->fromcvsFilter)
	free(e->fromcvsFilter);
    if(e->conflictHook)
	free(e->conflictHook);
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

	if(line==temp+1)
	    break;

	ctemp=*line;
	*line='\0';
	switch(opt){
	case 'f':
	    if(e.fromcvsFilter)
		free(e.fromcvsFilter);
	    e.fromcvsFilter=expand_path (temp);
            if (!e.fromcvsFilter)
		error (1, 0,
		       "Invalid environmental variable string '%s'",temp);
	    break;
	case 't':
	    if(e.tocvsFilter)
		free(e.tocvsFilter);
	    e.tocvsFilter=expand_path (temp);
            if (!e.tocvsFilter)
		error (1, 0,
		       "Invalid environmental variable string '%s'",temp);
	    break;
	case 'c':
	    if(e.conflictHook)
		free(e.conflictHook);
	    e.conflictHook=expand_path (temp);
            if (!e.conflictHook)
		error (1, 0,
		       "Invalid environmental variable string '%s'",temp);
	    break;
	case 'm':
	    if(*temp=='C' || *temp=='c')
		e.mergeMethod=WRAP_COPY;
	    else
		e.mergeMethod=WRAP_MERGE;
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
    wrap_list[x]->conflictHook=e->conflictHook;
    wrap_list[x]->mergeMethod=e->mergeMethod;
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
	if (fnmatch (wrap_list[x]->wildCard, name, 0) == 0){
	    switch(has){
	    case WRAP_TOCVS:
		temp=wrap_list[x]->tocvsFilter;
		break;
	    case WRAP_FROMCVS:
		temp=wrap_list[x]->fromcvsFilter;
		break;
	    case WRAP_CONFLICT:
		temp=wrap_list[x]->conflictHook;
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

WrapperEntry *
wrap_matching_entry (name)
    const char *name;
{
    int x,count=wrap_count+wrap_saved_count;

    for(x=0;x<count;++x)
	if (fnmatch (wrap_list[x]->wildCard, name, 0) == 0)
	    return wrap_list[x];
    return (WrapperEntry *)NULL;
}

char *
wrap_tocvs_process_file(fileName)
    const char *fileName;
{
    WrapperEntry *e=wrap_matching_entry(fileName);
    static char buf[L_tmpnam+1];

    if(e==NULL || e->tocvsFilter==NULL)
	return NULL;

    tmpnam(buf);

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

char *
wrap_fromcvs_process_file(fileName)
    const char *fileName;
{
    WrapperEntry *e=wrap_matching_entry(fileName);
    static char buf[PATH_MAX];

    if(e==NULL || e->fromcvsFilter==NULL)
	return NULL;

    run_setup(e->fromcvsFilter,fileName);
    run_exec(RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL );
    return buf;
}
