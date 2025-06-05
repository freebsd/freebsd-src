package t_enum;

use strict;
use vars qw(@ISA);

#require ktemplate;
require t_template;
require t_array;

@ISA=qw(t_template);

my @parms = qw(NAME TYPE COMPARE);
my %defaults = ( );
my @templatelines = <DATA>;

sub new { # no args
    my $self = {};
    bless $self;
    $self->init(\@parms, \%defaults, \@templatelines);
    return $self;
}

sub output {
    my ($self, $fh) = @_;
    my $a = new t_array;
    $a->setparm("NAME", $self->{values}{"NAME"} . "__enumerator_array");
    $a->setparm("TYPE", $self->{values}{"TYPE"});
    $a->output($fh);
    $self->SUPER::output($fh);
}

1;

__DATA__

/*
 * an enumerated collection type, generated from template
 *
 * Methods:
 * int init() -> returns nonzero on alloc failure
 * long size()
 * long find(match) -> -1 or index of any match
 * long append(value) -> -1 or new index
 * <TYPE> get(index) -> aborts if out of range
 * void destroy() -> frees array data
 *
 * Errors adding elements don't distinguish between "out of memory"
 * and "too big for size_t".
 *
 * Initial implementation: A flat array, reallocated as needed.  Our
 * uses probably aren't going to get very large.
 */

struct <NAME>__enumerator {
    <NAME>__enumerator_array a;
    size_t used;       /* number of entries used, idx used-1 is last */
};
typedef struct <NAME>__enumerator <NAME>;

static inline int
<NAME>_init(<NAME> *en)
{
    en->used = 0;
    return <NAME>__enumerator_array_init(&en->a);
}

static inline long
<NAME>_size(<NAME> *en)
{
    return en->used;
}

static inline long
<NAME>__s2l(size_t idx)
{
    long l;
    if (idx > LONG_MAX)
	abort();
    l = idx;
    if (l != idx)
	abort();
    return l;
}

static inline long
<NAME>_find(<NAME> *en, <TYPE> value)
{
    size_t i;
    for (i = 0; i < en->used; i++) {
	if (<COMPARE> (value, <NAME>__enumerator_array_get(&en->a, <NAME>__s2l(i))) == 0)
	    return i;
    }
    return -1;
}

static inline long
<NAME>_append(<NAME> *en, <TYPE> value)
{
    if (en->used >= LONG_MAX - 1)
	return -1;
    if (en->used >= SIZE_MAX - 1)
	return -1;
    if (<NAME>__enumerator_array_size(&en->a) == en->used) {
	if (<NAME>__enumerator_array_grow(&en->a, en->used + 1) < 0)
	    return -1;
    }
    <NAME>__enumerator_array_set(&en->a, <NAME>__s2l(en->used), value);
    en->used++;
    return en->used-1;
}

static inline <TYPE>
<NAME>_get(<NAME> *en, size_t idx)
{
    return <NAME>__enumerator_array_get(&en->a, <NAME>__s2l(idx));
}

static inline void
<NAME>_destroy(<NAME> *en)
{
    <NAME>__enumerator_array_destroy(&en->a);
    en->used = 0;
}

static inline void
<NAME>_foreach(<NAME> *en, int (*fn)(size_t i, <TYPE> t, void *p), void *p)
{
    size_t i;
    for (i = 0; i < en->used; i++) {
	if (fn (i, <NAME>__enumerator_array_get(&en->a, <NAME>__s2l(i)), p) != 0)
	    break;
    }
}
