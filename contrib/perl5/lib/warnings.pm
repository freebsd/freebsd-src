
# This file was created by warnings.pl
# Any changes made here will be lost.
#

package warnings;

=head1 NAME

warnings - Perl pragma to control optional warnings

=head1 SYNOPSIS

    use warnings;
    no warnings;

    use warnings "all";
    no warnings "all";

    use warnings::register;
    if (warnings::enabled()) {
        warnings::warn("some warning");
    }

    if (warnings::enabled("void")) {
        warnings::warn("void", "some warning");
    }

    if (warnings::enabled($object)) {
        warnings::warn($object, "some warning");
    }

    warnif("some warning");
    warnif("void", "some warning");
    warnif($object, "some warning");

=head1 DESCRIPTION

If no import list is supplied, all possible warnings are either enabled
or disabled.

A number of functions are provided to assist module authors. 

=over 4

=item use warnings::register

Creates a new warnings category with the same name as the package where
the call to the pragma is used.

=item warnings::enabled()

Use the warnings category with the same name as the current package.

Return TRUE if that warnings category is enabled in the calling module.
Otherwise returns FALSE.

=item warnings::enabled($category)

Return TRUE if the warnings category, C<$category>, is enabled in the
calling module.
Otherwise returns FALSE.

=item warnings::enabled($object)

Use the name of the class for the object reference, C<$object>, as the
warnings category.

Return TRUE if that warnings category is enabled in the first scope
where the object is used.
Otherwise returns FALSE.

=item warnings::warn($message)

Print C<$message> to STDERR.

Use the warnings category with the same name as the current package.

If that warnings category has been set to "FATAL" in the calling module
then die. Otherwise return.

=item warnings::warn($category, $message)

Print C<$message> to STDERR.

If the warnings category, C<$category>, has been set to "FATAL" in the
calling module then die. Otherwise return.

=item warnings::warn($object, $message)

Print C<$message> to STDERR.

Use the name of the class for the object reference, C<$object>, as the
warnings category.

If that warnings category has been set to "FATAL" in the scope where C<$object>
is first used then die. Otherwise return.


=item warnings::warnif($message)

Equivalent to:

    if (warnings::enabled())
      { warnings::warn($message) }

=item warnings::warnif($category, $message)

Equivalent to:

    if (warnings::enabled($category))
      { warnings::warn($category, $message) }

=item warnings::warnif($object, $message)

Equivalent to:

    if (warnings::enabled($object))
      { warnings::warn($object, $message) }

=back

See L<perlmodlib/Pragmatic Modules> and L<perllexwarn>.

=cut

use Carp ;

%Offsets = (
    'all'		=> 0,
    'chmod'		=> 2,
    'closure'		=> 4,
    'exiting'		=> 6,
    'glob'		=> 8,
    'io'		=> 10,
    'closed'		=> 12,
    'exec'		=> 14,
    'newline'		=> 16,
    'pipe'		=> 18,
    'unopened'		=> 20,
    'misc'		=> 22,
    'numeric'		=> 24,
    'once'		=> 26,
    'overflow'		=> 28,
    'pack'		=> 30,
    'portable'		=> 32,
    'recursion'		=> 34,
    'redefine'		=> 36,
    'regexp'		=> 38,
    'severe'		=> 40,
    'debugging'		=> 42,
    'inplace'		=> 44,
    'internal'		=> 46,
    'malloc'		=> 48,
    'signal'		=> 50,
    'substr'		=> 52,
    'syntax'		=> 54,
    'ambiguous'		=> 56,
    'bareword'		=> 58,
    'deprecated'	=> 60,
    'digit'		=> 62,
    'parenthesis'	=> 64,
    'precedence'	=> 66,
    'printf'		=> 68,
    'prototype'		=> 70,
    'qw'		=> 72,
    'reserved'		=> 74,
    'semicolon'		=> 76,
    'taint'		=> 78,
    'umask'		=> 80,
    'uninitialized'	=> 82,
    'unpack'		=> 84,
    'untie'		=> 86,
    'utf8'		=> 88,
    'void'		=> 90,
    'y2k'		=> 92,
  );

%Bits = (
    'all'		=> "\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x15", # [0..46]
    'ambiguous'		=> "\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00", # [28]
    'bareword'		=> "\x00\x00\x00\x00\x00\x00\x00\x04\x00\x00\x00\x00", # [29]
    'chmod'		=> "\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [1]
    'closed'		=> "\x00\x10\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [6]
    'closure'		=> "\x10\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [2]
    'debugging'		=> "\x00\x00\x00\x00\x00\x04\x00\x00\x00\x00\x00\x00", # [21]
    'deprecated'	=> "\x00\x00\x00\x00\x00\x00\x00\x10\x00\x00\x00\x00", # [30]
    'digit'		=> "\x00\x00\x00\x00\x00\x00\x00\x40\x00\x00\x00\x00", # [31]
    'exec'		=> "\x00\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [7]
    'exiting'		=> "\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [3]
    'glob'		=> "\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [4]
    'inplace'		=> "\x00\x00\x00\x00\x00\x10\x00\x00\x00\x00\x00\x00", # [22]
    'internal'		=> "\x00\x00\x00\x00\x00\x40\x00\x00\x00\x00\x00\x00", # [23]
    'io'		=> "\x00\x54\x15\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [5..10]
    'malloc'		=> "\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00", # [24]
    'misc'		=> "\x00\x00\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [11]
    'newline'		=> "\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [8]
    'numeric'		=> "\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00", # [12]
    'once'		=> "\x00\x00\x00\x04\x00\x00\x00\x00\x00\x00\x00\x00", # [13]
    'overflow'		=> "\x00\x00\x00\x10\x00\x00\x00\x00\x00\x00\x00\x00", # [14]
    'pack'		=> "\x00\x00\x00\x40\x00\x00\x00\x00\x00\x00\x00\x00", # [15]
    'parenthesis'	=> "\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00", # [32]
    'pipe'		=> "\x00\x00\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [9]
    'portable'		=> "\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00", # [16]
    'precedence'	=> "\x00\x00\x00\x00\x00\x00\x00\x00\x04\x00\x00\x00", # [33]
    'printf'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\x00\x00", # [34]
    'prototype'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x40\x00\x00\x00", # [35]
    'qw'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00", # [36]
    'recursion'		=> "\x00\x00\x00\x00\x04\x00\x00\x00\x00\x00\x00\x00", # [17]
    'redefine'		=> "\x00\x00\x00\x00\x10\x00\x00\x00\x00\x00\x00\x00", # [18]
    'regexp'		=> "\x00\x00\x00\x00\x40\x00\x00\x00\x00\x00\x00\x00", # [19]
    'reserved'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x04\x00\x00", # [37]
    'semicolon'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\x00", # [38]
    'severe'		=> "\x00\x00\x00\x00\x00\x55\x01\x00\x00\x00\x00\x00", # [20..24]
    'signal'		=> "\x00\x00\x00\x00\x00\x00\x04\x00\x00\x00\x00\x00", # [25]
    'substr'		=> "\x00\x00\x00\x00\x00\x00\x10\x00\x00\x00\x00\x00", # [26]
    'syntax'		=> "\x00\x00\x00\x00\x00\x00\x40\x55\x55\x15\x00\x00", # [27..38]
    'taint'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x40\x00\x00", # [39]
    'umask'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00", # [40]
    'uninitialized'	=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x04\x00", # [41]
    'unopened'		=> "\x00\x00\x10\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [10]
    'unpack'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00", # [42]
    'untie'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x40\x00", # [43]
    'utf8'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01", # [44]
    'void'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x04", # [45]
    'y2k'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10", # [46]
  );

%DeadBits = (
    'all'		=> "\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\x2a", # [0..46]
    'ambiguous'		=> "\x00\x00\x00\x00\x00\x00\x00\x02\x00\x00\x00\x00", # [28]
    'bareword'		=> "\x00\x00\x00\x00\x00\x00\x00\x08\x00\x00\x00\x00", # [29]
    'chmod'		=> "\x08\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [1]
    'closed'		=> "\x00\x20\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [6]
    'closure'		=> "\x20\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [2]
    'debugging'		=> "\x00\x00\x00\x00\x00\x08\x00\x00\x00\x00\x00\x00", # [21]
    'deprecated'	=> "\x00\x00\x00\x00\x00\x00\x00\x20\x00\x00\x00\x00", # [30]
    'digit'		=> "\x00\x00\x00\x00\x00\x00\x00\x80\x00\x00\x00\x00", # [31]
    'exec'		=> "\x00\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [7]
    'exiting'		=> "\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [3]
    'glob'		=> "\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [4]
    'inplace'		=> "\x00\x00\x00\x00\x00\x20\x00\x00\x00\x00\x00\x00", # [22]
    'internal'		=> "\x00\x00\x00\x00\x00\x80\x00\x00\x00\x00\x00\x00", # [23]
    'io'		=> "\x00\xa8\x2a\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [5..10]
    'malloc'		=> "\x00\x00\x00\x00\x00\x00\x02\x00\x00\x00\x00\x00", # [24]
    'misc'		=> "\x00\x00\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [11]
    'newline'		=> "\x00\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [8]
    'numeric'		=> "\x00\x00\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00", # [12]
    'once'		=> "\x00\x00\x00\x08\x00\x00\x00\x00\x00\x00\x00\x00", # [13]
    'overflow'		=> "\x00\x00\x00\x20\x00\x00\x00\x00\x00\x00\x00\x00", # [14]
    'pack'		=> "\x00\x00\x00\x80\x00\x00\x00\x00\x00\x00\x00\x00", # [15]
    'parenthesis'	=> "\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\x00\x00", # [32]
    'pipe'		=> "\x00\x00\x08\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [9]
    'portable'		=> "\x00\x00\x00\x00\x02\x00\x00\x00\x00\x00\x00\x00", # [16]
    'precedence'	=> "\x00\x00\x00\x00\x00\x00\x00\x00\x08\x00\x00\x00", # [33]
    'printf'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x20\x00\x00\x00", # [34]
    'prototype'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x80\x00\x00\x00", # [35]
    'qw'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\x00", # [36]
    'recursion'		=> "\x00\x00\x00\x00\x08\x00\x00\x00\x00\x00\x00\x00", # [17]
    'redefine'		=> "\x00\x00\x00\x00\x20\x00\x00\x00\x00\x00\x00\x00", # [18]
    'regexp'		=> "\x00\x00\x00\x00\x80\x00\x00\x00\x00\x00\x00\x00", # [19]
    'reserved'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x08\x00\x00", # [37]
    'semicolon'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x20\x00\x00", # [38]
    'severe'		=> "\x00\x00\x00\x00\x00\xaa\x02\x00\x00\x00\x00\x00", # [20..24]
    'signal'		=> "\x00\x00\x00\x00\x00\x00\x08\x00\x00\x00\x00\x00", # [25]
    'substr'		=> "\x00\x00\x00\x00\x00\x00\x20\x00\x00\x00\x00\x00", # [26]
    'syntax'		=> "\x00\x00\x00\x00\x00\x00\x80\xaa\xaa\x2a\x00\x00", # [27..38]
    'taint'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x80\x00\x00", # [39]
    'umask'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00", # [40]
    'uninitialized'	=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x08\x00", # [41]
    'unopened'		=> "\x00\x00\x20\x00\x00\x00\x00\x00\x00\x00\x00\x00", # [10]
    'unpack'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x20\x00", # [42]
    'untie'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x80\x00", # [43]
    'utf8'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02", # [44]
    'void'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x08", # [45]
    'y2k'		=> "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x20", # [46]
  );

$NONE     = "\0\0\0\0\0\0\0\0\0\0\0\0";
$LAST_BIT = 94 ;
$BYTES    = 12 ;

$All = "" ; vec($All, $Offsets{'all'}, 2) = 3 ;

sub bits {
    my $mask ;
    my $catmask ;
    my $fatal = 0 ;
    foreach my $word (@_) {
	if  ($word eq 'FATAL') {
	    $fatal = 1;
	}
	elsif ($catmask = $Bits{$word}) {
	    $mask |= $catmask ;
	    $mask |= $DeadBits{$word} if $fatal ;
	}
	else
          { croak("unknown warnings category '$word'")}  
    }

    return $mask ;
}

sub import {
    shift;
    my $mask = ${^WARNING_BITS} ;
    if (vec($mask, $Offsets{'all'}, 1)) {
        $mask |= $Bits{'all'} ;
        $mask |= $DeadBits{'all'} if vec($mask, $Offsets{'all'}+1, 1);
    }
    ${^WARNING_BITS} = $mask | bits(@_ ? @_ : 'all') ;
}

sub unimport {
    shift;
    my $mask = ${^WARNING_BITS} ;
    if (vec($mask, $Offsets{'all'}, 1)) {
        $mask |= $Bits{'all'} ;
        $mask |= $DeadBits{'all'} if vec($mask, $Offsets{'all'}+1, 1);
    }
    ${^WARNING_BITS} = $mask & ~ (bits(@_ ? @_ : 'all') | $All) ;
}

sub __chk
{
    my $category ;
    my $offset ;
    my $isobj = 0 ;

    if (@_) {
        # check the category supplied.
        $category = shift ;
        if (ref $category) {
            croak ("not an object")
                if $category !~ /^([^=]+)=/ ;+
	    $category = $1 ;
            $isobj = 1 ;
        }
        $offset = $Offsets{$category};
        croak("unknown warnings category '$category'")
	    unless defined $offset;
    }
    else {
        $category = (caller(1))[0] ; 
        $offset = $Offsets{$category};
        croak("package '$category' not registered for warnings")
	    unless defined $offset ;
    }

    my $this_pkg = (caller(1))[0] ; 
    my $i = 2 ;
    my $pkg ;

    if ($isobj) {
        while (do { { package DB; $pkg = (caller($i++))[0] } } ) {
            last unless @DB::args && $DB::args[0] =~ /^$category=/ ;
        }
	$i -= 2 ;
    }
    else {
        for ($i = 2 ; $pkg = (caller($i))[0] ; ++ $i) {
            last if $pkg ne $this_pkg ;
        }
        $i = 2 
            if !$pkg || $pkg eq $this_pkg ;
    }

    my $callers_bitmask = (caller($i))[9] ; 
    return ($callers_bitmask, $offset, $i) ;
}

sub enabled
{
    croak("Usage: warnings::enabled([category])")
	unless @_ == 1 || @_ == 0 ;

    my ($callers_bitmask, $offset, $i) = __chk(@_) ;

    return 0 unless defined $callers_bitmask ;
    return vec($callers_bitmask, $offset, 1) ||
           vec($callers_bitmask, $Offsets{'all'}, 1) ;
}


sub warn
{
    croak("Usage: warnings::warn([category,] 'message')")
	unless @_ == 2 || @_ == 1 ;

    my $message = pop ;
    my ($callers_bitmask, $offset, $i) = __chk(@_) ;
    local $Carp::CarpLevel = $i ;
    croak($message) 
	if vec($callers_bitmask, $offset+1, 1) ||
	   vec($callers_bitmask, $Offsets{'all'}+1, 1) ;
    carp($message) ;
}

sub warnif
{
    croak("Usage: warnings::warnif([category,] 'message')")
	unless @_ == 2 || @_ == 1 ;

    my $message = pop ;
    my ($callers_bitmask, $offset, $i) = __chk(@_) ;
    local $Carp::CarpLevel = $i ;

    return 
        unless defined $callers_bitmask &&
            	(vec($callers_bitmask, $offset, 1) ||
            	vec($callers_bitmask, $Offsets{'all'}, 1)) ;

    croak($message) 
	if vec($callers_bitmask, $offset+1, 1) ||
	   vec($callers_bitmask, $Offsets{'all'}+1, 1) ;

    carp($message) ;
}
1;
