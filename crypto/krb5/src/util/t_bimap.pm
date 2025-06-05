package t_bimap;

use strict;
use vars qw(@ISA);

require t_template;
require t_array;

@ISA=qw(t_template);

my @parms = qw(NAME LEFT RIGHT LEFTCMP RIGHTCMP LEFTPRINT RIGHTPRINT);
my %defaults = ();
my $headertemplate = "/*
 * bidirectional mapping table, add-only
 *
 * Parameters:
 * NAME
 * LEFT, RIGHT - types
 * LEFTCMP, RIGHTCMP - comparison functions
 *
 * Methods:
 * int init() - nonzero is error code, if any possible
 * long size()
 * void foreach(int (*)(LEFT, RIGHT, void*), void*)
 * int add(LEFT, RIGHT) - 0 = success, -1 = allocation failure
 * const <RIGHT> *findleft(<LEFT>) - null iff not found
 * const <LEFT> *findright(<RIGHT>)
 * void destroy() - destroys container, doesn't delete elements
 *
 * initial implementation: flat array of (left,right) pairs
 */

struct <NAME>__pair {
    <LEFT> l;
    <RIGHT> r;
};
";
my $bodytemplate = join "", <DATA>;

sub new { # no args
    my $self = {};
    bless $self;
    $self->init(\@parms, \%defaults, []);
    return $self;
}

sub output {
    my ($self, $fh) = @_;

    my $a = new t_array;
    $a->setparm("NAME", $self->{values}{"NAME"} . "__pairarray");
    $a->setparm("TYPE", "struct " . $self->{values}{"NAME"} . "__pair");

    print $fh "/* start of ", ref($self), " header template */\n";
    print $fh $self->substitute($headertemplate);
    print $fh "/* end of ", ref($self), " header template */\n";
    $a->output($fh);
    print $fh "/* start of ", ref($self), " body template */\n";
    print $fh $self->substitute($bodytemplate);
    print $fh "/* end of ", ref($self), " body template */\n";
}

1;

__DATA__

/* for use in cases where text substitutions may not work, like putting
   "const" before a type that turns out to be "char *"  */
typedef <LEFT> <NAME>__left_t;
typedef <RIGHT> <NAME>__right_t;

typedef struct {
    <NAME>__pairarray a;
    long nextidx;
} <NAME>;

static inline int
<NAME>_init (<NAME> *m)
{
    m->nextidx = 0;
    return <NAME>__pairarray_init (&m->a);
}

static inline long
<NAME>_size (<NAME> *m)
{
    return <NAME>__pairarray_size (&m->a);
}

static inline void
<NAME>_foreach (<NAME> *m, int (*fn)(<LEFT>, <RIGHT>, void *), void *p)
{
    long i, sz;
    sz = m->nextidx;
    for (i = 0; i < sz; i++) {
	struct <NAME>__pair *pair;
	pair = <NAME>__pairarray_getaddr (&m->a, i);
	if ((*fn)(pair->l, pair->r, p) != 0)
	    break;
    }
}

static inline int
<NAME>_add (<NAME> *m, <LEFT> l, <RIGHT> r)
{
    long i, sz;
    struct <NAME>__pair newpair;
    int err;

    sz = m->nextidx;
    /* Make sure we're not duplicating.  */
    for (i = 0; i < sz; i++) {
	struct <NAME>__pair *pair;
	pair = <NAME>__pairarray_getaddr (&m->a, i);
	assert ((*<LEFTCMP>)(l, pair->l) != 0);
	if ((*<LEFTCMP>)(l, pair->l) == 0)
	    abort();
	assert ((*<RIGHTCMP>)(r, pair->r) != 0);
	if ((*<RIGHTCMP>)(r, pair->r) == 0)
	    abort();
    }
    newpair.l = l;
    newpair.r = r;
    if (sz >= LONG_MAX - 1)
	return ENOMEM;
    err = <NAME>__pairarray_grow (&m->a, sz+1);
    if (err)
	return err;
    <NAME>__pairarray_set (&m->a, sz, newpair);
    m->nextidx++;
    return 0;
}

static inline const <NAME>__right_t *
<NAME>_findleft (<NAME> *m, <LEFT> l)
{
    long i, sz;
    sz = <NAME>_size (m);
    for (i = 0; i < sz; i++) {
	struct <NAME>__pair *pair;
	pair = <NAME>__pairarray_getaddr (&m->a, i);
	if ((*<LEFTCMP>)(l, pair->l) == 0)
	    return &pair->r;
    }
    return 0;
}

static inline const <NAME>__left_t *
<NAME>_findright (<NAME> *m, <RIGHT> r)
{
    long i, sz;
    sz = <NAME>_size (m);
    for (i = 0; i < sz; i++) {
	struct <NAME>__pair *pair;
	pair = <NAME>__pairarray_getaddr (&m->a, i);
	if ((*<RIGHTCMP>)(r, pair->r) == 0)
	    return &pair->l;
    }
    return 0;
}

struct <NAME>__printstat {
    FILE *f;
    int comma;
};
static inline int
<NAME>__printone (<LEFT> l, <RIGHT> r, void *p)
{
    struct <NAME>__printstat *ps = p;
    fprintf(ps->f, ps->comma ? ", (" : "(");
    ps->comma = 1;
    (*<LEFTPRINT>)(l, ps->f);
    fprintf(ps->f, ",");
    (*<RIGHTPRINT>)(r, ps->f);
    fprintf(ps->f, ")");
    return 0;
}

static inline void
<NAME>_printmap (<NAME> *m, FILE *f)
{
    struct <NAME>__printstat ps;
    ps.comma = 0;
    ps.f = f;
    fprintf(f, "(");
    <NAME>_foreach (m, <NAME>__printone, &ps);
    fprintf(f, ")");
}

static inline void
<NAME>_destroy (<NAME> *m)
{
    <NAME>__pairarray_destroy (&m->a);
}
