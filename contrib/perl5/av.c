/*    av.c
 *
 *    Copyright (c) 1991-1999, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "...for the Entwives desired order, and plenty, and peace (by which they
 * meant that things should remain where they had set them)." --Treebeard
 */

#include "EXTERN.h"
#include "perl.h"

void
av_reify(AV *av)
{
    I32 key;
    SV* sv;

    if (AvREAL(av))
	return;
#ifdef DEBUGGING
    if (SvTIED_mg((SV*)av, 'P'))
	warn("av_reify called on tied array");
#endif
    key = AvMAX(av) + 1;
    while (key > AvFILLp(av) + 1)
	AvARRAY(av)[--key] = &PL_sv_undef;
    while (key) {
	sv = AvARRAY(av)[--key];
	assert(sv);
	if (sv != &PL_sv_undef) {
	    dTHR;
	    (void)SvREFCNT_inc(sv);
	}
    }
    key = AvARRAY(av) - AvALLOC(av);
    while (key)
	AvALLOC(av)[--key] = &PL_sv_undef;
    AvREIFY_off(av);
    AvREAL_on(av);
}

void
av_extend(AV *av, I32 key)
{
    dTHR;			/* only necessary if we have to extend stack */
    MAGIC *mg;
    if (mg = SvTIED_mg((SV*)av, 'P')) {
	dSP;
	ENTER;
	SAVETMPS;
	PUSHSTACKi(PERLSI_MAGIC);
	PUSHMARK(SP);
	EXTEND(SP,2);
	PUSHs(SvTIED_obj((SV*)av, mg));
	PUSHs(sv_2mortal(newSViv(key+1)));
        PUTBACK;
	perl_call_method("EXTEND", G_SCALAR|G_DISCARD);
	POPSTACK;
	FREETMPS;
	LEAVE;
	return;
    }
    if (key > AvMAX(av)) {
	SV** ary;
	I32 tmp;
	I32 newmax;

	if (AvALLOC(av) != AvARRAY(av)) {
	    ary = AvALLOC(av) + AvFILLp(av) + 1;
	    tmp = AvARRAY(av) - AvALLOC(av);
	    Move(AvARRAY(av), AvALLOC(av), AvFILLp(av)+1, SV*);
	    AvMAX(av) += tmp;
	    SvPVX(av) = (char*)AvALLOC(av);
	    if (AvREAL(av)) {
		while (tmp)
		    ary[--tmp] = &PL_sv_undef;
	    }
	    
	    if (key > AvMAX(av) - 10) {
		newmax = key + AvMAX(av);
		goto resize;
	    }
	}
	else {
	    if (AvALLOC(av)) {
#ifndef STRANGE_MALLOC
		U32 bytes;
#endif

#if defined(MYMALLOC) && !defined(PURIFY) && !defined(LEAKTEST)
		newmax = malloced_size((void*)AvALLOC(av))/sizeof(SV*) - 1;

		if (key <= newmax) 
		    goto resized;
#endif 
		newmax = key + AvMAX(av) / 5;
	      resize:
#if defined(STRANGE_MALLOC) || defined(MYMALLOC)
		Renew(AvALLOC(av),newmax+1, SV*);
#else
		bytes = (newmax + 1) * sizeof(SV*);
#define MALLOC_OVERHEAD 16
		tmp = MALLOC_OVERHEAD;
		while (tmp - MALLOC_OVERHEAD < bytes)
		    tmp += tmp;
		tmp -= MALLOC_OVERHEAD;
		tmp /= sizeof(SV*);
		assert(tmp > newmax);
		newmax = tmp - 1;
		New(2,ary, newmax+1, SV*);
		Copy(AvALLOC(av), ary, AvMAX(av)+1, SV*);
		if (AvMAX(av) > 64)
		    offer_nice_chunk(AvALLOC(av), (AvMAX(av)+1) * sizeof(SV*));
		else
		    Safefree(AvALLOC(av));
		AvALLOC(av) = ary;
#endif
	      resized:
		ary = AvALLOC(av) + AvMAX(av) + 1;
		tmp = newmax - AvMAX(av);
		if (av == PL_curstack) {	/* Oops, grew stack (via av_store()?) */
		    PL_stack_sp = AvALLOC(av) + (PL_stack_sp - PL_stack_base);
		    PL_stack_base = AvALLOC(av);
		    PL_stack_max = PL_stack_base + newmax;
		}
	    }
	    else {
		newmax = key < 3 ? 3 : key;
		New(2,AvALLOC(av), newmax+1, SV*);
		ary = AvALLOC(av) + 1;
		tmp = newmax;
		AvALLOC(av)[0] = &PL_sv_undef;	/* For the stacks */
	    }
	    if (AvREAL(av)) {
		while (tmp)
		    ary[--tmp] = &PL_sv_undef;
	    }
	    
	    SvPVX(av) = (char*)AvALLOC(av);
	    AvMAX(av) = newmax;
	}
    }
}

SV**
av_fetch(register AV *av, I32 key, I32 lval)
{
    SV *sv;

    if (!av)
	return 0;

    if (key < 0) {
	key += AvFILL(av) + 1;
	if (key < 0)
	    return 0;
    }

    if (SvRMAGICAL(av)) {
	if (mg_find((SV*)av,'P')) {
	    dTHR;
	    sv = sv_newmortal();
	    mg_copy((SV*)av, sv, 0, key);
	    PL_av_fetch_sv = sv;
	    return &PL_av_fetch_sv;
	}
    }

    if (key > AvFILLp(av)) {
	if (!lval)
	    return 0;
	sv = NEWSV(5,0);
	return av_store(av,key,sv);
    }
    if (AvARRAY(av)[key] == &PL_sv_undef) {
    emptyness:
	if (lval) {
	    sv = NEWSV(6,0);
	    return av_store(av,key,sv);
	}
	return 0;
    }
    else if (AvREIFY(av)
	     && (!AvARRAY(av)[key]	/* eg. @_ could have freed elts */
		 || SvTYPE(AvARRAY(av)[key]) == SVTYPEMASK)) {
	AvARRAY(av)[key] = &PL_sv_undef;	/* 1/2 reify */
	goto emptyness;
    }
    return &AvARRAY(av)[key];
}

SV**
av_store(register AV *av, I32 key, SV *val)
{
    SV** ary;
    U32  fill;


    if (!av)
	return 0;
    if (!val)
	val = &PL_sv_undef;

    if (key < 0) {
	key += AvFILL(av) + 1;
	if (key < 0)
	    return 0;
    }

    if (SvREADONLY(av) && key >= AvFILL(av))
	croak(no_modify);

    if (SvRMAGICAL(av)) {
	if (mg_find((SV*)av,'P')) {
	    if (val != &PL_sv_undef) {
		mg_copy((SV*)av, val, 0, key);
	    }
	    return 0;
	}
    }

    if (!AvREAL(av) && AvREIFY(av))
	av_reify(av);
    if (key > AvMAX(av))
	av_extend(av,key);
    ary = AvARRAY(av);
    if (AvFILLp(av) < key) {
	if (!AvREAL(av)) {
	    dTHR;
	    if (av == PL_curstack && key > PL_stack_sp - PL_stack_base)
		PL_stack_sp = PL_stack_base + key;	/* XPUSH in disguise */
	    do
		ary[++AvFILLp(av)] = &PL_sv_undef;
	    while (AvFILLp(av) < key);
	}
	AvFILLp(av) = key;
    }
    else if (AvREAL(av))
	SvREFCNT_dec(ary[key]);
    ary[key] = val;
    if (SvSMAGICAL(av)) {
	if (val != &PL_sv_undef) {
	    MAGIC* mg = SvMAGIC(av);
	    sv_magic(val, (SV*)av, toLOWER(mg->mg_type), 0, key);
	}
	mg_set((SV*)av);
    }
    return &ary[key];
}

AV *
newAV(void)
{
    register AV *av;

    av = (AV*)NEWSV(3,0);
    sv_upgrade((SV *)av, SVt_PVAV);
    AvREAL_on(av);
    AvALLOC(av) = 0;
    SvPVX(av) = 0;
    AvMAX(av) = AvFILLp(av) = -1;
    return av;
}

AV *
av_make(register I32 size, register SV **strp)
{
    register AV *av;
    register I32 i;
    register SV** ary;

    av = (AV*)NEWSV(8,0);
    sv_upgrade((SV *) av,SVt_PVAV);
    AvFLAGS(av) = AVf_REAL;
    if (size) {		/* `defined' was returning undef for size==0 anyway. */
	New(4,ary,size,SV*);
	AvALLOC(av) = ary;
	SvPVX(av) = (char*)ary;
	AvFILLp(av) = size - 1;
	AvMAX(av) = size - 1;
	for (i = 0; i < size; i++) {
	    assert (*strp);
	    ary[i] = NEWSV(7,0);
	    sv_setsv(ary[i], *strp);
	    strp++;
	}
    }
    return av;
}

AV *
av_fake(register I32 size, register SV **strp)
{
    register AV *av;
    register SV** ary;

    av = (AV*)NEWSV(9,0);
    sv_upgrade((SV *)av, SVt_PVAV);
    New(4,ary,size+1,SV*);
    AvALLOC(av) = ary;
    Copy(strp,ary,size,SV*);
    AvFLAGS(av) = AVf_REIFY;
    SvPVX(av) = (char*)ary;
    AvFILLp(av) = size - 1;
    AvMAX(av) = size - 1;
    while (size--) {
	assert (*strp);
	SvTEMP_off(*strp);
	strp++;
    }
    return av;
}

void
av_clear(register AV *av)
{
    register I32 key;
    SV** ary;

#ifdef DEBUGGING
    if (SvREFCNT(av) <= 0) {
	warn("Attempt to clear deleted array");
    }
#endif
    if (!av)
	return;
    /*SUPPRESS 560*/

    if (SvREADONLY(av))
	croak(no_modify);

    /* Give any tie a chance to cleanup first */
    if (SvRMAGICAL(av))
	mg_clear((SV*)av); 

    if (AvMAX(av) < 0)
	return;

    if (AvREAL(av)) {
	ary = AvARRAY(av);
	key = AvFILLp(av) + 1;
	while (key) {
	    SvREFCNT_dec(ary[--key]);
	    ary[key] = &PL_sv_undef;
	}
    }
    if (key = AvARRAY(av) - AvALLOC(av)) {
	AvMAX(av) += key;
	SvPVX(av) = (char*)AvALLOC(av);
    }
    AvFILLp(av) = -1;

}

void
av_undef(register AV *av)
{
    register I32 key;

    if (!av)
	return;
    /*SUPPRESS 560*/

    /* Give any tie a chance to cleanup first */
    if (SvTIED_mg((SV*)av, 'P')) 
	av_fill(av, -1);   /* mg_clear() ? */

    if (AvREAL(av)) {
	key = AvFILLp(av) + 1;
	while (key)
	    SvREFCNT_dec(AvARRAY(av)[--key]);
    }
    Safefree(AvALLOC(av));
    AvALLOC(av) = 0;
    SvPVX(av) = 0;
    AvMAX(av) = AvFILLp(av) = -1;
    if (AvARYLEN(av)) {
	SvREFCNT_dec(AvARYLEN(av));
	AvARYLEN(av) = 0;
    }
}

void
av_push(register AV *av, SV *val)
{             
    MAGIC *mg;
    if (!av)
	return;
    if (SvREADONLY(av))
	croak(no_modify);

    if (mg = SvTIED_mg((SV*)av, 'P')) {
	dSP;
	PUSHSTACKi(PERLSI_MAGIC);
	PUSHMARK(SP);
	EXTEND(SP,2);
	PUSHs(SvTIED_obj((SV*)av, mg));
	PUSHs(val);
	PUTBACK;
	ENTER;
	perl_call_method("PUSH", G_SCALAR|G_DISCARD);
	LEAVE;
	POPSTACK;
	return;
    }
    av_store(av,AvFILLp(av)+1,val);
}

SV *
av_pop(register AV *av)
{
    SV *retval;
    MAGIC* mg;

    if (!av || AvFILL(av) < 0)
	return &PL_sv_undef;
    if (SvREADONLY(av))
	croak(no_modify);
    if (mg = SvTIED_mg((SV*)av, 'P')) {
	dSP;    
	PUSHSTACKi(PERLSI_MAGIC);
	PUSHMARK(SP);
	XPUSHs(SvTIED_obj((SV*)av, mg));
	PUTBACK;
	ENTER;
	if (perl_call_method("POP", G_SCALAR)) {
	    retval = newSVsv(*PL_stack_sp--);    
	} else {    
	    retval = &PL_sv_undef;
	}
	LEAVE;
	POPSTACK;
	return retval;
    }
    retval = AvARRAY(av)[AvFILLp(av)];
    AvARRAY(av)[AvFILLp(av)--] = &PL_sv_undef;
    if (SvSMAGICAL(av))
	mg_set((SV*)av);
    return retval;
}

void
av_unshift(register AV *av, register I32 num)
{
    register I32 i;
    register SV **ary;
    MAGIC* mg;

    if (!av || num <= 0)
	return;
    if (SvREADONLY(av))
	croak(no_modify);

    if (mg = SvTIED_mg((SV*)av, 'P')) {
	dSP;
	PUSHSTACKi(PERLSI_MAGIC);
	PUSHMARK(SP);
	EXTEND(SP,1+num);
	PUSHs(SvTIED_obj((SV*)av, mg));
	while (num-- > 0) {
	    PUSHs(&PL_sv_undef);
	}
	PUTBACK;
	ENTER;
	perl_call_method("UNSHIFT", G_SCALAR|G_DISCARD);
	LEAVE;
	POPSTACK;
	return;
    }

    if (!AvREAL(av) && AvREIFY(av))
	av_reify(av);
    i = AvARRAY(av) - AvALLOC(av);
    if (i) {
	if (i > num)
	    i = num;
	num -= i;
    
	AvMAX(av) += i;
	AvFILLp(av) += i;
	SvPVX(av) = (char*)(AvARRAY(av) - i);
    }
    if (num) {
	i = AvFILLp(av);
	av_extend(av, i + num);
	AvFILLp(av) += num;
	ary = AvARRAY(av);
	Move(ary, ary + num, i + 1, SV*);
	do {
	    ary[--num] = &PL_sv_undef;
	} while (num);
    }
}

SV *
av_shift(register AV *av)
{
    SV *retval;
    MAGIC* mg;

    if (!av || AvFILL(av) < 0)
	return &PL_sv_undef;
    if (SvREADONLY(av))
	croak(no_modify);
    if (mg = SvTIED_mg((SV*)av, 'P')) {
	dSP;
	PUSHSTACKi(PERLSI_MAGIC);
	PUSHMARK(SP);
	XPUSHs(SvTIED_obj((SV*)av, mg));
	PUTBACK;
	ENTER;
	if (perl_call_method("SHIFT", G_SCALAR)) {
	    retval = newSVsv(*PL_stack_sp--);            
	} else {    
	    retval = &PL_sv_undef;
	}     
	LEAVE;
	POPSTACK;
	return retval;
    }
    retval = *AvARRAY(av);
    if (AvREAL(av))
	*AvARRAY(av) = &PL_sv_undef;
    SvPVX(av) = (char*)(AvARRAY(av) + 1);
    AvMAX(av)--;
    AvFILLp(av)--;
    if (SvSMAGICAL(av))
	mg_set((SV*)av);
    return retval;
}

I32
av_len(register AV *av)
{
    return AvFILL(av);
}

void
av_fill(register AV *av, I32 fill)
{
    MAGIC *mg;
    if (!av)
	croak("panic: null array");
    if (fill < 0)
	fill = -1;
    if (mg = SvTIED_mg((SV*)av, 'P')) {
	dSP;            
	ENTER;
	SAVETMPS;
	PUSHSTACKi(PERLSI_MAGIC);
	PUSHMARK(SP);
	EXTEND(SP,2);
	PUSHs(SvTIED_obj((SV*)av, mg));
	PUSHs(sv_2mortal(newSViv(fill+1)));
	PUTBACK;
	perl_call_method("STORESIZE", G_SCALAR|G_DISCARD);
	POPSTACK;
	FREETMPS;
	LEAVE;
	return;
    }
    if (fill <= AvMAX(av)) {
	I32 key = AvFILLp(av);
	SV** ary = AvARRAY(av);

	if (AvREAL(av)) {
	    while (key > fill) {
		SvREFCNT_dec(ary[key]);
		ary[key--] = &PL_sv_undef;
	    }
	}
	else {
	    while (key < fill)
		ary[++key] = &PL_sv_undef;
	}
	    
	AvFILLp(av) = fill;
	if (SvSMAGICAL(av))
	    mg_set((SV*)av);
    }
    else
	(void)av_store(av,fill,&PL_sv_undef);
}


/* AVHV: Support for treating arrays as if they were hashes.  The
 * first element of the array should be a hash reference that maps
 * hash keys to array indices.
 */

STATIC I32
avhv_index_sv(SV* sv)
{
    I32 index = SvIV(sv);
    if (index < 1)
	croak("Bad index while coercing array into hash");
    return index;    
}

HV*
avhv_keys(AV *av)
{
    SV **keysp = av_fetch(av, 0, FALSE);
    if (keysp) {
	SV *sv = *keysp;
	if (SvGMAGICAL(sv))
	    mg_get(sv);
	if (SvROK(sv)) {
	    sv = SvRV(sv);
	    if (SvTYPE(sv) == SVt_PVHV)
		return (HV*)sv;
	}
    }
    croak("Can't coerce array into hash");
    return Nullhv;
}

SV**
avhv_fetch_ent(AV *av, SV *keysv, I32 lval, U32 hash)
{
    SV **indsvp;
    HV *keys = avhv_keys(av);
    HE *he;
    
    he = hv_fetch_ent(keys, keysv, FALSE, hash);
    if (!he)
        croak("No such array field");
    return av_fetch(av, avhv_index_sv(HeVAL(he)), lval);
}

bool
avhv_exists_ent(AV *av, SV *keysv, U32 hash)
{
    HV *keys = avhv_keys(av);
    return hv_exists_ent(keys, keysv, hash);
}

HE *
avhv_iternext(AV *av)
{
    HV *keys = avhv_keys(av);
    return hv_iternext(keys);
}

SV *
avhv_iterval(AV *av, register HE *entry)
{
    SV *sv = hv_iterval(avhv_keys(av), entry);
    return *av_fetch(av, avhv_index_sv(sv), TRUE);
}
