/*    av.c
 *
 *    Copyright (c) 1991-2001, Larry Wall
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
#define PERL_IN_AV_C
#include "perl.h"

void
Perl_av_reify(pTHX_ AV *av)
{
    I32 key;
    SV* sv;

    if (AvREAL(av))
	return;
#ifdef DEBUGGING
    if (SvTIED_mg((SV*)av, 'P') && ckWARN_d(WARN_DEBUGGING))
	Perl_warner(aTHX_ WARN_DEBUGGING, "av_reify called on tied array");
#endif
    key = AvMAX(av) + 1;
    while (key > AvFILLp(av) + 1)
	AvARRAY(av)[--key] = &PL_sv_undef;
    while (key) {
	sv = AvARRAY(av)[--key];
	assert(sv);
	if (sv != &PL_sv_undef)
	    (void)SvREFCNT_inc(sv);
    }
    key = AvARRAY(av) - AvALLOC(av);
    while (key)
	AvALLOC(av)[--key] = &PL_sv_undef;
    AvREIFY_off(av);
    AvREAL_on(av);
}

/*
=for apidoc av_extend

Pre-extend an array.  The C<key> is the index to which the array should be
extended.

=cut
*/

void
Perl_av_extend(pTHX_ AV *av, I32 key)
{
    MAGIC *mg;
    if ((mg = SvTIED_mg((SV*)av, 'P'))) {
	dSP;
	ENTER;
	SAVETMPS;
	PUSHSTACKi(PERLSI_MAGIC);
	PUSHMARK(SP);
	EXTEND(SP,2);
	PUSHs(SvTIED_obj((SV*)av, mg));
	PUSHs(sv_2mortal(newSViv(key+1)));
        PUTBACK;
	call_method("EXTEND", G_SCALAR|G_DISCARD);
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
		MEM_SIZE bytes;
		IV itmp;
#endif

#if defined(MYMALLOC) && !defined(LEAKTEST)
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
		itmp = MALLOC_OVERHEAD;
		while (itmp - MALLOC_OVERHEAD < bytes)
		    itmp += itmp;
		itmp -= MALLOC_OVERHEAD;
		itmp /= sizeof(SV*);
		assert(itmp > newmax);
		newmax = itmp - 1;
		assert(newmax >= AvMAX(av));
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

/*
=for apidoc av_fetch

Returns the SV at the specified index in the array.  The C<key> is the
index.  If C<lval> is set then the fetch will be part of a store.  Check
that the return value is non-null before dereferencing it to a C<SV*>.

See L<perlguts/"Understanding the Magic of Tied Hashes and Arrays"> for
more information on how to use this function on tied arrays. 

=cut
*/

SV**
Perl_av_fetch(pTHX_ register AV *av, I32 key, I32 lval)
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
	if (mg_find((SV*)av,'P') || mg_find((SV*)av,'D')) {
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

/*
=for apidoc av_store

Stores an SV in an array.  The array index is specified as C<key>.  The
return value will be NULL if the operation failed or if the value did not
need to be actually stored within the array (as in the case of tied
arrays). Otherwise it can be dereferenced to get the original C<SV*>.  Note
that the caller is responsible for suitably incrementing the reference
count of C<val> before the call, and decrementing it if the function
returned NULL.

See L<perlguts/"Understanding the Magic of Tied Hashes and Arrays"> for
more information on how to use this function on tied arrays.

=cut
*/

SV**
Perl_av_store(pTHX_ register AV *av, I32 key, SV *val)
{
    SV** ary;

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
	Perl_croak(aTHX_ PL_no_modify);

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

/*
=for apidoc newAV

Creates a new AV.  The reference count is set to 1.

=cut
*/

AV *
Perl_newAV(pTHX)
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

/*
=for apidoc av_make

Creates a new AV and populates it with a list of SVs.  The SVs are copied
into the array, so they may be freed after the call to av_make.  The new AV
will have a reference count of 1.

=cut
*/

AV *
Perl_av_make(pTHX_ register I32 size, register SV **strp)
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
Perl_av_fake(pTHX_ register I32 size, register SV **strp)
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

/*
=for apidoc av_clear

Clears an array, making it empty.  Does not free the memory used by the
array itself.

=cut
*/

void
Perl_av_clear(pTHX_ register AV *av)
{
    register I32 key;
    SV** ary;

#ifdef DEBUGGING
    if (SvREFCNT(av) == 0 && ckWARN_d(WARN_DEBUGGING)) {
	Perl_warner(aTHX_ WARN_DEBUGGING, "Attempt to clear deleted array");
    }
#endif
    if (!av)
	return;
    /*SUPPRESS 560*/

    if (SvREADONLY(av))
	Perl_croak(aTHX_ PL_no_modify);

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
    if ((key = AvARRAY(av) - AvALLOC(av))) {
	AvMAX(av) += key;
	SvPVX(av) = (char*)AvALLOC(av);
    }
    AvFILLp(av) = -1;

}

/*
=for apidoc av_undef

Undefines the array.  Frees the memory used by the array itself.

=cut
*/

void
Perl_av_undef(pTHX_ register AV *av)
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

/*
=for apidoc av_push

Pushes an SV onto the end of the array.  The array will grow automatically
to accommodate the addition.

=cut
*/

void
Perl_av_push(pTHX_ register AV *av, SV *val)
{             
    MAGIC *mg;
    if (!av)
	return;
    if (SvREADONLY(av))
	Perl_croak(aTHX_ PL_no_modify);

    if ((mg = SvTIED_mg((SV*)av, 'P'))) {
	dSP;
	PUSHSTACKi(PERLSI_MAGIC);
	PUSHMARK(SP);
	EXTEND(SP,2);
	PUSHs(SvTIED_obj((SV*)av, mg));
	PUSHs(val);
	PUTBACK;
	ENTER;
	call_method("PUSH", G_SCALAR|G_DISCARD);
	LEAVE;
	POPSTACK;
	return;
    }
    av_store(av,AvFILLp(av)+1,val);
}

/*
=for apidoc av_pop

Pops an SV off the end of the array.  Returns C<&PL_sv_undef> if the array
is empty.

=cut
*/

SV *
Perl_av_pop(pTHX_ register AV *av)
{
    SV *retval;
    MAGIC* mg;

    if (!av || AvFILL(av) < 0)
	return &PL_sv_undef;
    if (SvREADONLY(av))
	Perl_croak(aTHX_ PL_no_modify);
    if ((mg = SvTIED_mg((SV*)av, 'P'))) {
	dSP;    
	PUSHSTACKi(PERLSI_MAGIC);
	PUSHMARK(SP);
	XPUSHs(SvTIED_obj((SV*)av, mg));
	PUTBACK;
	ENTER;
	if (call_method("POP", G_SCALAR)) {
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

/*
=for apidoc av_unshift

Unshift the given number of C<undef> values onto the beginning of the
array.  The array will grow automatically to accommodate the addition.  You
must then use C<av_store> to assign values to these new elements.

=cut
*/

void
Perl_av_unshift(pTHX_ register AV *av, register I32 num)
{
    register I32 i;
    register SV **ary;
    MAGIC* mg;
    I32 slide;

    if (!av || num <= 0)
	return;
    if (SvREADONLY(av))
	Perl_croak(aTHX_ PL_no_modify);

    if ((mg = SvTIED_mg((SV*)av, 'P'))) {
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
	call_method("UNSHIFT", G_SCALAR|G_DISCARD);
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
	/* Create extra elements */
	slide = i > 0 ? i : 0;
	num += slide;
	av_extend(av, i + num);
	AvFILLp(av) += num;
	ary = AvARRAY(av);
	Move(ary, ary + num, i + 1, SV*);
	do {
	    ary[--num] = &PL_sv_undef;
	} while (num);
	/* Make extra elements into a buffer */
	AvMAX(av) -= slide;
	AvFILLp(av) -= slide;
	SvPVX(av) = (char*)(AvARRAY(av) + slide);
    }
}

/*
=for apidoc av_shift

Shifts an SV off the beginning of the array.

=cut
*/

SV *
Perl_av_shift(pTHX_ register AV *av)
{
    SV *retval;
    MAGIC* mg;

    if (!av || AvFILL(av) < 0)
	return &PL_sv_undef;
    if (SvREADONLY(av))
	Perl_croak(aTHX_ PL_no_modify);
    if ((mg = SvTIED_mg((SV*)av, 'P'))) {
	dSP;
	PUSHSTACKi(PERLSI_MAGIC);
	PUSHMARK(SP);
	XPUSHs(SvTIED_obj((SV*)av, mg));
	PUTBACK;
	ENTER;
	if (call_method("SHIFT", G_SCALAR)) {
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

/*
=for apidoc av_len

Returns the highest index in the array.  Returns -1 if the array is
empty.

=cut
*/

I32
Perl_av_len(pTHX_ register AV *av)
{
    return AvFILL(av);
}

/*
=for apidoc av_fill

Ensure than an array has a given number of elements, equivalent to
Perl's C<$#array = $fill;>.

=cut
*/
void
Perl_av_fill(pTHX_ register AV *av, I32 fill)
{
    MAGIC *mg;
    if (!av)
	Perl_croak(aTHX_ "panic: null array");
    if (fill < 0)
	fill = -1;
    if ((mg = SvTIED_mg((SV*)av, 'P'))) {
	dSP;            
	ENTER;
	SAVETMPS;
	PUSHSTACKi(PERLSI_MAGIC);
	PUSHMARK(SP);
	EXTEND(SP,2);
	PUSHs(SvTIED_obj((SV*)av, mg));
	PUSHs(sv_2mortal(newSViv(fill+1)));
	PUTBACK;
	call_method("STORESIZE", G_SCALAR|G_DISCARD);
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

/*
=for apidoc av_delete

Deletes the element indexed by C<key> from the array.  Returns the
deleted element. C<flags> is currently ignored.

=cut
*/
SV *
Perl_av_delete(pTHX_ AV *av, I32 key, I32 flags)
{
    SV *sv;

    if (!av)
	return Nullsv;
    if (SvREADONLY(av))
	Perl_croak(aTHX_ PL_no_modify);
    if (key < 0) {
	key += AvFILL(av) + 1;
	if (key < 0)
	    return Nullsv;
    }
    if (SvRMAGICAL(av)) {
	SV **svp;
	if ((mg_find((SV*)av,'P') || mg_find((SV*)av,'D'))
	    && (svp = av_fetch(av, key, TRUE)))
	{
	    sv = *svp;
	    mg_clear(sv);
	    if (mg_find(sv, 'p')) {
		sv_unmagic(sv, 'p');		/* No longer an element */
		return sv;
	    }
	    return Nullsv;			/* element cannot be deleted */
	}
    }
    if (key > AvFILLp(av))
	return Nullsv;
    else {
	sv = AvARRAY(av)[key];
	if (key == AvFILLp(av)) {
	    do {
		AvFILLp(av)--;
	    } while (--key >= 0 && AvARRAY(av)[key] == &PL_sv_undef);
	}
	else
	    AvARRAY(av)[key] = &PL_sv_undef;
	if (SvSMAGICAL(av))
	    mg_set((SV*)av);
    }
    if (flags & G_DISCARD) {
	SvREFCNT_dec(sv);
	sv = Nullsv;
    }
    return sv;
}

/*
=for apidoc av_exists

Returns true if the element indexed by C<key> has been initialized.

This relies on the fact that uninitialized array elements are set to
C<&PL_sv_undef>.

=cut
*/
bool
Perl_av_exists(pTHX_ AV *av, I32 key)
{
    if (!av)
	return FALSE;
    if (key < 0) {
	key += AvFILL(av) + 1;
	if (key < 0)
	    return FALSE;
    }
    if (SvRMAGICAL(av)) {
	if (mg_find((SV*)av,'P') || mg_find((SV*)av,'D')) {
	    SV *sv = sv_newmortal();
	    MAGIC *mg;

	    mg_copy((SV*)av, sv, 0, key);
	    mg = mg_find(sv, 'p');
	    if (mg) {
		magic_existspack(sv, mg);
		return SvTRUE(sv);
	    }
	}
    }
    if (key <= AvFILLp(av) && AvARRAY(av)[key] != &PL_sv_undef
	&& AvARRAY(av)[key])
    {
	return TRUE;
    }
    else
	return FALSE;
}

/* AVHV: Support for treating arrays as if they were hashes.  The
 * first element of the array should be a hash reference that maps
 * hash keys to array indices.
 */

STATIC I32
S_avhv_index_sv(pTHX_ SV* sv)
{
    I32 index = SvIV(sv);
    if (index < 1)
	Perl_croak(aTHX_ "Bad index while coercing array into hash");
    return index;    
}

STATIC I32
S_avhv_index(pTHX_ AV *av, SV *keysv, U32 hash)
{
    HV *keys;
    HE *he;
    STRLEN n_a;

    keys = avhv_keys(av);
    he = hv_fetch_ent(keys, keysv, FALSE, hash);
    if (!he)
        Perl_croak(aTHX_ "No such pseudo-hash field \"%s\"", SvPV(keysv,n_a));
    return avhv_index_sv(HeVAL(he));
}

HV*
Perl_avhv_keys(pTHX_ AV *av)
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
    Perl_croak(aTHX_ "Can't coerce array into hash");
    return Nullhv;
}

SV**
Perl_avhv_store_ent(pTHX_ AV *av, SV *keysv, SV *val, U32 hash)
{
    return av_store(av, avhv_index(av, keysv, hash), val);
}

SV**
Perl_avhv_fetch_ent(pTHX_ AV *av, SV *keysv, I32 lval, U32 hash)
{
    return av_fetch(av, avhv_index(av, keysv, hash), lval);
}

SV *
Perl_avhv_delete_ent(pTHX_ AV *av, SV *keysv, I32 flags, U32 hash)
{
    HV *keys = avhv_keys(av);
    HE *he;
	
    he = hv_fetch_ent(keys, keysv, FALSE, hash);
    if (!he || !SvOK(HeVAL(he)))
	return Nullsv;

    return av_delete(av, avhv_index_sv(HeVAL(he)), flags);
}

/* Check for the existence of an element named by a given key.
 *
 */
bool
Perl_avhv_exists_ent(pTHX_ AV *av, SV *keysv, U32 hash)
{
    HV *keys = avhv_keys(av);
    HE *he;
	
    he = hv_fetch_ent(keys, keysv, FALSE, hash);
    if (!he || !SvOK(HeVAL(he)))
	return FALSE;

    return av_exists(av, avhv_index_sv(HeVAL(he)));
}

HE *
Perl_avhv_iternext(pTHX_ AV *av)
{
    HV *keys = avhv_keys(av);
    return hv_iternext(keys);
}

SV *
Perl_avhv_iterval(pTHX_ AV *av, register HE *entry)
{
    SV *sv = hv_iterval(avhv_keys(av), entry);
    return *av_fetch(av, avhv_index_sv(sv), TRUE);
}
