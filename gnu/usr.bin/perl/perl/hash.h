/* $RCSfile: hash.h,v $$Revision: 1.1.1.1 $$Date: 1993/08/23 21:29:37 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: hash.h,v $
 * Revision 1.1.1.1  1993/08/23  21:29:37  nate
 * PERL!
 *
 * Revision 4.0.1.2  91/11/05  17:24:31  lwall
 * patch11: random cleanup
 * 
 * Revision 4.0.1.1  91/06/07  11:10:33  lwall
 * patch4: new copyright notice
 * 
 * Revision 4.0  91/03/20  01:22:38  lwall
 * 4.0 baseline.
 * 
 */

#define FILLPCT 80		/* don't make greater than 99 */
#define DBM_CACHE_MAX 63	/* cache 64 entries for dbm file */
				/* (resident array acts as a write-thru cache)*/

#define COEFFSIZE (16 * 8)	/* size of coeff array */

typedef struct hentry HENT;

struct hentry {
    HENT	*hent_next;
    char	*hent_key;
    STR		*hent_val;
    int		hent_hash;
    int		hent_klen;
};

struct htbl {
    HENT	**tbl_array;
    int		tbl_max;	/* subscript of last element of tbl_array */
    int		tbl_dosplit;	/* how full to get before splitting */
    int		tbl_fill;	/* how full tbl_array currently is */
    int		tbl_riter;	/* current root of iterator */
    HENT	*tbl_eiter;	/* current entry of iterator */
    SPAT 	*tbl_spatroot;	/* list of spats for this package */
    char	*tbl_name;	/* name, if a symbol table */
#ifdef SOME_DBM
#ifdef HAS_GDBM
    GDBM_FILE	tbl_dbm;
#else
#ifdef HAS_NDBM
    DBM		*tbl_dbm;
#else
    int		tbl_dbm;
#endif
#endif
#endif
    unsigned char tbl_coeffsize;	/* is 0 for symbol tables */
};

STR *hfetch();
bool hstore();
STR *hdelete();
HASH *hnew();
void hclear();
void hentfree();
void hfree();
int hiterinit();
HENT *hiternext();
char *hiterkey();
STR *hiterval();
bool hdbmopen();
void hdbmclose();
bool hdbmstore();
