package t_tsenum;

use strict;
use vars qw(@ISA);

require t_template;
require t_enum;

@ISA=qw(t_template);

my @parms = qw(NAME TYPE COMPARE COPY PRINT);
my %defaults = ( "COPY", "0", "PRINT", "0" );
my @templatelines = <DATA>;

sub new { # no args
    my $self = {};
    bless $self;
    $self->init(\@parms, \%defaults, \@templatelines);
    return $self;
}

sub output {
    my ($self, $fh) = @_;
    my $a = new t_enum;
    $a->setparm("NAME", $self->{values}{"NAME"} . "__unsafe_enumerator");
    $a->setparm("TYPE", $self->{values}{"TYPE"});
    $a->setparm("COMPARE", $self->{values}{"COMPARE"});
    $a->output($fh);
    $self->SUPER::output($fh);
}

1;

__DATA__

/*
 */
#include "k5-thread.h"
struct <NAME>__ts_enumerator {
    <NAME>__unsafe_enumerator e;
    k5_mutex_t m;
};
typedef struct <NAME>__ts_enumerator <NAME>;

static inline int
<NAME>_init(<NAME> *en)
{
    int err = k5_mutex_init(&en->m);
    if (err)
	return err;
    err = <NAME>__unsafe_enumerator_init(&en->e);
    if (err) {
	k5_mutex_destroy(&en->m);
	return err;
    }
    return 0;
}

static inline int
<NAME>_size(<NAME> *en, long *size)
{
    int err = k5_mutex_lock(&en->m);
    if (err) {
	*size = -48;
	return err;
    }
    *size = <NAME>__unsafe_enumerator_size(&en->e);
    k5_mutex_unlock(&en->m);
    return 0;
}

static inline int
<NAME>__do_copy (<TYPE> *dest, <TYPE> src)
{
    int (*copyfn)(<TYPE>*, <TYPE>) = <COPY>;
    if (copyfn)
	return copyfn(dest, src);
    *dest = src;
    return 0;
}

static inline int
<NAME>_find_or_append(<NAME> *en, <TYPE> value, long *idxp, int *added)
{
    int err;
    long idx;

    err = k5_mutex_lock(&en->m);
    if (err)
	return err;
    idx = <NAME>__unsafe_enumerator_find(&en->e, value);
    if (idx < 0) {
	<TYPE> newvalue;
	err = <NAME>__do_copy(&newvalue, value);
	if (err == 0)
	    idx = <NAME>__unsafe_enumerator_append(&en->e, newvalue);
	k5_mutex_unlock(&en->m);
	if (err != 0)
	    return err;
	if (idx < 0)
	    return ENOMEM;
	*idxp = idx;
	*added = 1;
	return 0;
    }
    k5_mutex_unlock(&en->m);
    *idxp = idx;
    *added = 0;
    return 0;
}

static inline int
<NAME>_get(<NAME> *en, size_t idx, <TYPE> *value)
{
    int err;
    err = k5_mutex_lock(&en->m);
    if (err)
	return err;
    *value = <NAME>__unsafe_enumerator_get(&en->e, idx);
    k5_mutex_unlock(&en->m);
    return 0;
}

static inline void
<NAME>_destroy(<NAME> *en)
{
    k5_mutex_destroy(&en->m);
    <NAME>__unsafe_enumerator_destroy(&en->e);
}

static inline int
<NAME>_foreach(<NAME> *en, int (*fn)(size_t i, <TYPE> t, void *p), void *p)
{
    int err = k5_mutex_lock(&en->m);
    if (err)
	return err;
    <NAME>__unsafe_enumerator_foreach(&en->e, fn, p);
    k5_mutex_unlock(&en->m);
    return 0;
}

static inline int
<NAME>__print_map_elt(size_t idx, <TYPE> val, void *p)
{
    void (*printfn)(<TYPE>, FILE *) = <PRINT>;
    FILE *f = (FILE *) p;
    if (printfn) {
	fprintf(f, " %lu=", (unsigned long) idx);
	printfn(val, f);
    }
    return 0;
}

static inline void
<NAME>_print(<NAME> *en, FILE *f)
{
    void (*printfn)(<TYPE>, FILE *) = <PRINT>;
    if (printfn) {
	fprintf(f, "{");
	<NAME>_foreach (en, <NAME>__print_map_elt, f);
	fprintf(f, " }");
    }
}
