/*    hv.h
 *
 *    Copyright (c) 1991-1999, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

typedef struct he HE;
typedef struct hek HEK;

struct he {
    HE		*hent_next;
    HEK		*hent_hek;
    SV		*hent_val;
};

struct hek {
    U32		hek_hash;
    I32		hek_len;
    char	hek_key[1];
};

/* This structure must match the beginning of struct xpvmg in sv.h. */
struct xpvhv {
    char *	xhv_array;	/* pointer to malloced string */
    STRLEN	xhv_fill;	/* how full xhv_array currently is */
    STRLEN	xhv_max;	/* subscript of last element of xhv_array */
    IV		xhv_keys;	/* how many elements in the array */
    double	xnv_nv;		/* numeric value, if any */
    MAGIC*	xmg_magic;	/* magic for scalar array */
    HV*		xmg_stash;	/* class package */

    I32		xhv_riter;	/* current root of iterator */
    HE		*xhv_eiter;	/* current entry of iterator */
    PMOP	*xhv_pmroot;	/* list of pm's for this package */
    char	*xhv_name;	/* name, if a symbol table */
};

#define PERL_HASH(hash,str,len) \
     STMT_START	{ \
	register char *s_PeRlHaSh = str; \
	register I32 i_PeRlHaSh = len; \
	register U32 hash_PeRlHaSh = 0; \
	while (i_PeRlHaSh--) \
	    hash_PeRlHaSh = hash_PeRlHaSh * 33 + *s_PeRlHaSh++; \
	(hash) = hash_PeRlHaSh; \
    } STMT_END


/* these hash entry flags ride on hent_klen (for use only in magic/tied HVs) */
#define HEf_SVKEY	-2	/* hent_key is a SV* */


#define Nullhv Null(HV*)
#define HvARRAY(hv)	((HE**)((XPVHV*)  SvANY(hv))->xhv_array)
#define HvFILL(hv)	((XPVHV*)  SvANY(hv))->xhv_fill
#define HvMAX(hv)	((XPVHV*)  SvANY(hv))->xhv_max
#define HvKEYS(hv)	((XPVHV*)  SvANY(hv))->xhv_keys
#define HvRITER(hv)	((XPVHV*)  SvANY(hv))->xhv_riter
#define HvEITER(hv)	((XPVHV*)  SvANY(hv))->xhv_eiter
#define HvPMROOT(hv)	((XPVHV*)  SvANY(hv))->xhv_pmroot
#define HvNAME(hv)	((XPVHV*)  SvANY(hv))->xhv_name

#define HvSHAREKEYS(hv)		(SvFLAGS(hv) & SVphv_SHAREKEYS)
#define HvSHAREKEYS_on(hv)	(SvFLAGS(hv) |= SVphv_SHAREKEYS)
#define HvSHAREKEYS_off(hv)	(SvFLAGS(hv) &= ~SVphv_SHAREKEYS)

#define HvLAZYDEL(hv)		(SvFLAGS(hv) & SVphv_LAZYDEL)
#define HvLAZYDEL_on(hv)	(SvFLAGS(hv) |= SVphv_LAZYDEL)
#define HvLAZYDEL_off(hv)	(SvFLAGS(hv) &= ~SVphv_LAZYDEL)

#ifdef OVERLOAD

/* Maybe amagical: */
/* #define HV_AMAGICmb(hv)      (SvFLAGS(hv) & (SVpgv_badAM | SVpgv_AM)) */

#define HV_AMAGIC(hv)        (SvFLAGS(hv) &   SVpgv_AM)
#define HV_AMAGIC_on(hv)     (SvFLAGS(hv) |=  SVpgv_AM)
#define HV_AMAGIC_off(hv)    (SvFLAGS(hv) &= ~SVpgv_AM)

/*
#define HV_AMAGICbad(hv)     (SvFLAGS(hv) & SVpgv_badAM)
#define HV_badAMAGIC_on(hv)  (SvFLAGS(hv) |= SVpgv_badAM)
#define HV_badAMAGIC_off(hv) (SvFLAGS(hv) &= ~SVpgv_badAM)
*/

#endif /* OVERLOAD */

#define Nullhe Null(HE*)
#define HeNEXT(he)		(he)->hent_next
#define HeKEY_hek(he)		(he)->hent_hek
#define HeKEY(he)		HEK_KEY(HeKEY_hek(he))
#define HeKEY_sv(he)		(*(SV**)HeKEY(he))
#define HeKLEN(he)		HEK_LEN(HeKEY_hek(he))
#define HeVAL(he)		(he)->hent_val
#define HeHASH(he)		HEK_HASH(HeKEY_hek(he))
#define HePV(he,lp)		((HeKLEN(he) == HEf_SVKEY) ?		\
				 SvPV(HeKEY_sv(he),lp) :		\
				 (((lp = HeKLEN(he)) >= 0) ?		\
				  HeKEY(he) : Nullch))

#define HeSVKEY(he)		((HeKEY(he) && 				\
				  HeKLEN(he) == HEf_SVKEY) ?		\
				 HeKEY_sv(he) : Nullsv)

#define HeSVKEY_force(he)	(HeKEY(he) ?				\
				 ((HeKLEN(he) == HEf_SVKEY) ?		\
				  HeKEY_sv(he) :			\
				  sv_2mortal(newSVpv(HeKEY(he),		\
						     HeKLEN(he)))) :	\
				 &PL_sv_undef)
#define HeSVKEY_set(he,sv)	((HeKLEN(he) = HEf_SVKEY), (HeKEY_sv(he) = sv))

#define Nullhek Null(HEK*)
#define HEK_BASESIZE		STRUCT_OFFSET(HEK, hek_key[0])
#define HEK_HASH(hek)		(hek)->hek_hash
#define HEK_LEN(hek)		(hek)->hek_len
#define HEK_KEY(hek)		(hek)->hek_key
