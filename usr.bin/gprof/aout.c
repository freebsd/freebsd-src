#include <a.out.h>

#include "gprof.h"

static void getstrtab(FILE *, const char *);
static void getsymtab(FILE *, const char *);
static void gettextspace(FILE *);
static bool funcsymbol(struct nlist *);

static char	*strtab;		/* string table in core */
static long	ssiz;			/* size of the string table */
static struct	exec xbuf;		/* exec header of a.out */

/* Things which get -E excluded by default. */
static char	*excludes[] = { "mcount", "__mcleanup", NULL };

    /*
     * Set up string and symbol tables from a.out.
     *	and optionally the text space.
     * On return symbol table is sorted by value.
     *
     * Returns 0 on success, -1 on failure.
     */
int
aout_getnfile(const char *filename, char ***defaultEs)
{
    FILE	*nfile;
    int		valcmp();

    nfile = fopen( filename ,"r");
    if (nfile == NULL) {
	perror( filename );
	done();
    }
    fread(&xbuf, 1, sizeof(xbuf), nfile);
    if (N_BADMAG(xbuf)) {
	fclose(nfile);
	return -1;
    }
    getstrtab(nfile, filename);
    getsymtab(nfile, filename);
    gettextspace( nfile );
    fclose(nfile);
#   ifdef DEBUG
	if ( debug & AOUTDEBUG ) {
	    register int j;

	    for (j = 0; j < nname; j++){
		printf("[getnfile] 0X%08x\t%s\n", nl[j].value, nl[j].name);
	    }
	}
#   endif DEBUG
    *defaultEs = excludes;
    return 0;
}

static void
getstrtab(FILE *nfile, const char *filename)
{

    fseek(nfile, (long)(N_SYMOFF(xbuf) + xbuf.a_syms), 0);
    if (fread(&ssiz, sizeof (ssiz), 1, nfile) == 0) {
	warnx("%s: no string table (old format?)" , filename );
	done();
    }
    strtab = calloc(ssiz, 1);
    if (strtab == NULL) {
	warnx("%s: no room for %d bytes of string table", filename , ssiz);
	done();
    }
    if (fread(strtab+sizeof(ssiz), ssiz-sizeof(ssiz), 1, nfile) != 1) {
	warnx("%s: error reading string table", filename );
	done();
    }
}

    /*
     * Read in symbol table
     */
static void
getsymtab(FILE *nfile, const char *filename)
{
    register long	i;
    int			askfor;
    struct nlist	nbuf;

    /* pass1 - count symbols */
    fseek(nfile, (long)N_SYMOFF(xbuf), 0);
    nname = 0;
    for (i = xbuf.a_syms; i > 0; i -= sizeof(struct nlist)) {
	fread(&nbuf, sizeof(nbuf), 1, nfile);
	if ( ! funcsymbol( &nbuf ) ) {
	    continue;
	}
	nname++;
    }
    if (nname == 0) {
	warnx("%s: no symbols", filename );
	done();
    }
    askfor = nname + 1;
    nl = (nltype *) calloc( askfor , sizeof(nltype) );
    if (nl == 0) {
	warnx("no room for %d bytes of symbol table", askfor * sizeof(nltype) );
	done();
    }

    /* pass2 - read symbols */
    fseek(nfile, (long)N_SYMOFF(xbuf), 0);
    npe = nl;
    nname = 0;
    for (i = xbuf.a_syms; i > 0; i -= sizeof(struct nlist)) {
	fread(&nbuf, sizeof(nbuf), 1, nfile);
	if ( ! funcsymbol( &nbuf ) ) {
#	    ifdef DEBUG
		if ( debug & AOUTDEBUG ) {
		    printf( "[getsymtab] rejecting: 0x%x %s\n" ,
			    nbuf.n_type , strtab + nbuf.n_un.n_strx );
		}
#	    endif DEBUG
	    continue;
	}
	npe->value = nbuf.n_value;
	npe->name = strtab+nbuf.n_un.n_strx;
#	ifdef DEBUG
	    if ( debug & AOUTDEBUG ) {
		printf( "[getsymtab] %d %s 0x%08x\n" ,
			nname , npe -> name , npe -> value );
	    }
#	endif DEBUG
	npe++;
	nname++;
    }
    npe->value = -1;
}

    /*
     *	read in the text space of an a.out file
     */
static void
gettextspace(FILE *nfile)
{

    if ( cflag == 0 ) {
	return;
    }
    textspace = (u_char *) malloc( xbuf.a_text );
    if ( textspace == 0 ) {
	warnx("ran out room for %d bytes of text space: can't do -c" ,
		  xbuf.a_text );
	return;
    }
    (void) fseek( nfile , N_TXTOFF( xbuf ) , 0 );
    if ( fread( textspace , 1 , xbuf.a_text , nfile ) != xbuf.a_text ) {
	warnx("couldn't read text space: can't do -c");
	free( textspace );
	textspace = 0;
	return;
    }
}

static bool
funcsymbol(struct nlist *nlistp)
{
    char	*name, c;

	/*
	 *	must be a text symbol,
	 *	and static text symbols don't qualify if aflag set.
	 */
    if ( ! (  ( nlistp -> n_type == ( N_TEXT | N_EXT ) )
	   || ( ( nlistp -> n_type == N_TEXT ) && ( aflag == 0 ) ) ) ) {
	return FALSE;
    }
	/*
	 *	name must start with an underscore if uflag is set.
	 *	can't have any `funny' characters in name,
	 *	where `funny' means `.' (.o file names)
	 *	need to make an exception for sparc .mul & co.
	 *	perhaps we should just drop this code entirely...
	 */
    name = strtab + nlistp -> n_un.n_strx;
    if ( uflag && *name != '_' )
	return FALSE;
#ifdef sparc
    if ( *name == '.' ) {
	char *p = name + 1;
	if ( *p == 'u' )
	    p++;
	if ( strcmp ( p, "mul" ) == 0 || strcmp ( p, "div" ) == 0 ||
	     strcmp ( p, "rem" ) == 0 )
		return TRUE;
    }
#endif
    while ( c = *name++ ) {
	if ( c == '.' ) {
	    return FALSE;
	}
    }
    return TRUE;
}
