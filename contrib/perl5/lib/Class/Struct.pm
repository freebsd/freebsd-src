package Class::Struct;

## See POD after __END__

require 5.002;

use strict;
use vars qw(@ISA @EXPORT);

use Carp;

require Exporter;
@ISA = qw(Exporter);
@EXPORT = qw(struct);

## Tested on 5.002 and 5.003 without class membership tests:
my $CHECK_CLASS_MEMBERSHIP = ($] >= 5.003_95);

my $print = 0;
sub printem {
    if (@_) { $print = shift }
    else    { $print++ }
}

{
    package Class::Struct::Tie_ISA;

    sub TIEARRAY {
        my $class = shift;
        return bless [], $class;
    }

    sub STORE {
        my ($self, $index, $value) = @_;
        Class::Struct::_subclass_error();
    }

    sub FETCH {
        my ($self, $index) = @_;
        $self->[$index];
    }

    sub FETCHSIZE {
        my $self = shift;
        return scalar(@$self);
    }

    sub DESTROY { }
}

sub struct {

    # Determine parameter list structure, one of:
    #   struct( class => [ element-list ])
    #   struct( class => { element-list })
    #   struct( element-list )
    # Latter form assumes current package name as struct name.

    my ($class, @decls);
    my $base_type = ref $_[1];
    if ( $base_type eq 'HASH' ) {
        $class = shift;
        @decls = %{shift()};
        _usage_error() if @_;
    }
    elsif ( $base_type eq 'ARRAY' ) {
        $class = shift;
        @decls = @{shift()};
        _usage_error() if @_;
    }
    else {
        $base_type = 'ARRAY';
        $class = (caller())[0];
        @decls = @_;
    }
    _usage_error() if @decls % 2 == 1;

    # Ensure we are not, and will not be, a subclass.

    my $isa = do {
        no strict 'refs';
        \@{$class . '::ISA'};
    };
    _subclass_error() if @$isa;
    tie @$isa, 'Class::Struct::Tie_ISA';

    # Create constructor.

    croak "function 'new' already defined in package $class"
        if do { no strict 'refs'; defined &{$class . "::new"} };

    my @methods = ();
    my %refs = ();
    my %arrays = ();
    my %hashes = ();
    my %classes = ();
    my $got_class = 0;
    my $out = '';

    $out = "{\n  package $class;\n  use Carp;\n  sub new {\n";

    my $cnt = 0;
    my $idx = 0;
    my( $cmt, $name, $type, $elem );

    if( $base_type eq 'HASH' ){
        $out .= "    my(\$r) = {};\n";
        $cmt = '';
    }
    elsif( $base_type eq 'ARRAY' ){
        $out .= "    my(\$r) = [];\n";
    }
    while( $idx < @decls ){
        $name = $decls[$idx];
        $type = $decls[$idx+1];
        push( @methods, $name );
        if( $base_type eq 'HASH' ){
            $elem = "{'$name'}";
        }
        elsif( $base_type eq 'ARRAY' ){
            $elem = "[$cnt]";
            ++$cnt;
            $cmt = " # $name";
        }
        if( $type =~ /^\*(.)/ ){
            $refs{$name}++;
            $type = $1;
        }
        if( $type eq '@' ){
            $out .= "    \$r->$elem = [];$cmt\n";
            $arrays{$name}++;
        }
        elsif( $type eq '%' ){
            $out .= "    \$r->$elem = {};$cmt\n";
            $hashes{$name}++;
        }
        elsif ( $type eq '$') {
            $out .= "    \$r->$elem = undef;$cmt\n";
        }
        elsif( $type =~ /^\w+(?:::\w+)*$/ ){
            $out .= "    \$r->$elem = '${type}'->new();$cmt\n";
            $classes{$name} = $type;
            $got_class = 1;
        }
        else{
            croak "'$type' is not a valid struct element type";
        }
        $idx += 2;
    }
    $out .= "    bless \$r;\n  }\n";

    # Create accessor methods.

    my( $pre, $pst, $sel );
    $cnt = 0;
    foreach $name (@methods){
        if ( do { no strict 'refs'; defined &{$class . "::$name"} } ) {
            carp "function '$name' already defined, overrides struct accessor method"
                if $^W;
        }
        else {
            $pre = $pst = $cmt = $sel = '';
            if( defined $refs{$name} ){
                $pre = "\\(";
                $pst = ")";
                $cmt = " # returns ref";
            }
            $out .= "  sub $name {$cmt\n    my \$r = shift;\n";
            if( $base_type eq 'ARRAY' ){
                $elem = "[$cnt]";
                ++$cnt;
            }
            elsif( $base_type eq 'HASH' ){
                $elem = "{'$name'}";
            }
            if( defined $arrays{$name} ){
                $out .= "    my \$i;\n";
                $out .= "    \@_ ? (\$i = shift) : return $pre\$r->$elem$pst;\n";
                $sel = "->[\$i]";
            }
            elsif( defined $hashes{$name} ){
                $out .= "    my \$i;\n";
                $out .= "    \@_ ? (\$i = shift) : return $pre\$r->$elem$pst;\n";
                $sel = "->{\$i}";
            }
            elsif( defined $classes{$name} ){
                if ( $CHECK_CLASS_MEMBERSHIP ) {
                    $out .= "    croak '$name argument is wrong class' if \@_ && ! UNIVERSAL::isa(\$_[0], '$classes{$name}');\n";
                }
            }
            $out .= "    croak 'Too many args to $name' if \@_ > 1;\n";
            $out .= "    \@_ ? ($pre\$r->$elem$sel = shift$pst) : $pre\$r->$elem$sel$pst;\n";
            $out .= "  }\n";
        }
    }
    $out .= "}\n1;\n";

    print $out if $print;
    my $result = eval $out;
    carp $@ if $@;
}

sub _usage_error {
    confess "struct usage error";
}

sub _subclass_error {
    croak 'struct class cannot be a subclass (@ISA not allowed)';
}

1; # for require


__END__

=head1 NAME

Class::Struct - declare struct-like datatypes as Perl classes

=head1 SYNOPSIS

    use Class::Struct;
            # declare struct, based on array:
    struct( CLASS_NAME => [ ELEMENT_NAME => ELEMENT_TYPE, ... ]);
            # declare struct, based on hash:
    struct( CLASS_NAME => { ELEMENT_NAME => ELEMENT_TYPE, ... });

    package CLASS_NAME;
    use Class::Struct;
            # declare struct, based on array, implicit class name:
    struct( ELEMENT_NAME => ELEMENT_TYPE, ... );


    package Myobj;
    use Class::Struct;
            # declare struct with four types of elements:
    struct( s => '$', a => '@', h => '%', c => 'My_Other_Class' );

    $obj = new Myobj;               # constructor

                                    # scalar type accessor:
    $element_value = $obj->s;           # element value
    $obj->s('new value');               # assign to element

                                    # array type accessor:
    $ary_ref = $obj->a;                 # reference to whole array
    $ary_element_value = $obj->a(2);    # array element value
    $obj->a(2, 'new value');            # assign to array element

                                    # hash type accessor:
    $hash_ref = $obj->h;                # reference to whole hash
    $hash_element_value = $obj->h('x'); # hash element value
    $obj->h('x', 'new value');        # assign to hash element

                                    # class type accessor:
    $element_value = $obj->c;           # object reference
    $obj->c->method(...);               # call method of object
    $obj->c(new My_Other_Class);        # assign a new object


=head1 DESCRIPTION

C<Class::Struct> exports a single function, C<struct>.
Given a list of element names and types, and optionally
a class name, C<struct> creates a Perl 5 class that implements
a "struct-like" data structure.

The new class is given a constructor method, C<new>, for creating
struct objects.

Each element in the struct data has an accessor method, which is
used to assign to the element and to fetch its value.  The
default accessor can be overridden by declaring a C<sub> of the
same name in the package.  (See Example 2.)

Each element's type can be scalar, array, hash, or class.


=head2 The C<struct()> function

The C<struct> function has three forms of parameter-list.

    struct( CLASS_NAME => [ ELEMENT_LIST ]);
    struct( CLASS_NAME => { ELEMENT_LIST });
    struct( ELEMENT_LIST );

The first and second forms explicitly identify the name of the
class being created.  The third form assumes the current package
name as the class name.

An object of a class created by the first and third forms is
based on an array, whereas an object of a class created by the
second form is based on a hash. The array-based forms will be
somewhat faster and smaller; the hash-based forms are more
flexible.

The class created by C<struct> must not be a subclass of another
class other than C<UNIVERSAL>.

A function named C<new> must not be explicitly defined in a class
created by C<struct>.

The I<ELEMENT_LIST> has the form

    NAME => TYPE, ...

Each name-type pair declares one element of the struct. Each
element name will be defined as an accessor method unless a
method by that name is explicitly defined; in the latter case, a
warning is issued if the warning flag (B<-w>) is set.


=head2 Element Types and Accessor Methods

The four element types -- scalar, array, hash, and class -- are
represented by strings -- C<'$'>, C<'@'>, C<'%'>, and a class name --
optionally preceded by a C<'*'>.

The accessor method provided by C<struct> for an element depends
on the declared type of the element.

=over

=item Scalar (C<'$'> or C<'*$'>)

The element is a scalar, and is initialized to C<undef>.

The accessor's argument, if any, is assigned to the element.

If the element type is C<'$'>, the value of the element (after
assignment) is returned. If the element type is C<'*$'>, a reference
to the element is returned.

=item Array (C<'@'> or C<'*@'>)

The element is an array, initialized to C<()>.

With no argument, the accessor returns a reference to the
element's whole array.

With one or two arguments, the first argument is an index
specifying one element of the array; the second argument, if
present, is assigned to the array element.  If the element type
is C<'@'>, the accessor returns the array element value.  If the
element type is C<'*@'>, a reference to the array element is
returned.

=item Hash (C<'%'> or C<'*%'>)

The element is a hash, initialized to C<()>.

With no argument, the accessor returns a reference to the
element's whole hash.

With one or two arguments, the first argument is a key specifying
one element of the hash; the second argument, if present, is
assigned to the hash element.  If the element type is C<'%'>, the
accessor returns the hash element value.  If the element type is
C<'*%'>, a reference to the hash element is returned.

=item Class (C<'Class_Name'> or C<'*Class_Name'>)

The element's value must be a reference blessed to the named
class or to one of its subclasses. The element is initialized to
the result of calling the C<new> constructor of the named class.

The accessor's argument, if any, is assigned to the element. The
accessor will C<croak> if this is not an appropriate object
reference.

If the element type does not start with a C<'*'>, the accessor
returns the element value (after assignment). If the element type
starts with a C<'*'>, a reference to the element itself is returned.

=back

=head1 EXAMPLES

=over

=item Example 1

Giving a struct element a class type that is also a struct is how
structs are nested.  Here, C<timeval> represents a time (seconds and
microseconds), and C<rusage> has two elements, each of which is of
type C<timeval>.

    use Class::Struct;

    struct( rusage => {
        ru_utime => timeval,  # seconds
        ru_stime => timeval,  # microseconds
    });

    struct( timeval => [
        tv_secs  => '$',
        tv_usecs => '$',
    ]);

        # create an object:
    my $t = new rusage;
    	# $t->ru_utime and $t->ru_stime are objects of type timeval.

        # set $t->ru_utime to 100.0 sec and $t->ru_stime to 5.0 sec.
    $t->ru_utime->tv_secs(100);
    $t->ru_utime->tv_usecs(0);
    $t->ru_stime->tv_secs(5);
    $t->ru_stime->tv_usecs(0);


=item Example 2

An accessor function can be redefined in order to provide
additional checking of values, etc.  Here, we want the C<count>
element always to be nonnegative, so we redefine the C<count>
accessor accordingly.

    package MyObj;
    use Class::Struct;

    		# declare the struct
    struct ( 'MyObj', { count => '$', stuff => '%' } );

    		# override the default accessor method for 'count'
    sub count {
        my $self = shift;
        if ( @_ ) {
            die 'count must be nonnegative' if $_[0] < 0;
            $self->{'count'} = shift;
            warn "Too many args to count" if @_;
        }
        return $self->{'count'};
    }

    package main;
    $x = new MyObj;
    print "\$x->count(5) = ", $x->count(5), "\n";
                            # prints '$x->count(5) = 5'

    print "\$x->count = ", $x->count, "\n";
                            # prints '$x->count = 5'

    print "\$x->count(-5) = ", $x->count(-5), "\n";
                            # dies due to negative argument!


=head1 Author and Modification History


Renamed to C<Class::Struct> and modified by Jim Miner, 1997-04-02.

    members() function removed.
    Documentation corrected and extended.
    Use of struct() in a subclass prohibited.
    User definition of accessor allowed.
    Treatment of '*' in element types corrected.
    Treatment of classes as element types corrected.
    Class name to struct() made optional.
    Diagnostic checks added.


Originally C<Class::Template> by Dean Roehrich.

    # Template.pm   --- struct/member template builder
    #   12mar95
    #   Dean Roehrich
    #
    # changes/bugs fixed since 28nov94 version:
    #  - podified
    # changes/bugs fixed since 21nov94 version:
    #  - Fixed examples.
    # changes/bugs fixed since 02sep94 version:
    #  - Moved to Class::Template.
    # changes/bugs fixed since 20feb94 version:
    #  - Updated to be a more proper module.
    #  - Added "use strict".
    #  - Bug in build_methods, was using @var when @$var needed.
    #  - Now using my() rather than local().
    #
    # Uses perl5 classes to create nested data types.
    # This is offered as one implementation of Tom Christiansen's "structs.pl"
    # idea.

=cut
