#!./perl

BEGIN {
    chdir 't' if -d 't';
    $dir = "self-$$";
    @INC = $dir;
    push @INC, '../lib';

    print "1..19\n";

    # First we must set up some selfloader files
    mkdir $dir, 0755            or die "Can't mkdir $dir: $!";

    open(FOO, ">$dir/Foo.pm") or die;
    print FOO <<'EOT';
package Foo;
use SelfLoader;

sub new { bless {}, shift }
sub foo;
sub bar;
sub bazmarkhianish;
sub a;
sub never;    # declared but definition should never be read
1;
__DATA__

sub foo { shift; shift || "foo" };

sub bar { shift; shift || "bar" }

sub bazmarkhianish { shift; shift || "baz" }

package sheep;
sub bleat { shift; shift || "baa" }

__END__
sub never { die "D'oh" }
EOT

    close(FOO);

    open(BAR, ">$dir/Bar.pm") or die;
    print BAR <<'EOT';
package Bar;
use SelfLoader;

@ISA = 'Baz';

sub new { bless {}, shift }
sub a;

1;
__DATA__

sub a { 'a Bar'; }
sub b { 'b Bar' }

__END__ DATA
sub never { die "D'oh" }
EOT

    close(BAR);
};


package Baz;

sub a { 'a Baz' }
sub b { 'b Baz' }
sub c { 'c Baz' }


package main;
use Foo;
use Bar;

$foo = new Foo;

print "not " unless $foo->foo eq 'foo';  # selfloaded first time
print "ok 1\n";

print "not " unless $foo->foo eq 'foo';  # regular call
print "ok 2\n";

# Try an undefined method
eval {
    $foo->will_fail;
};
if ($@ =~ /^Undefined subroutine/) {
    print "ok 3\n";
} else {
    print "not ok 3 $@\n";
}

# Used to be trouble with this
eval {
    my $foo = new Foo;
    die "oops";
};
if ($@ =~ /oops/) {
    print "ok 4\n";
} else {
    print "not ok 4 $@\n";
}

# Pass regular expression variable to autoloaded function.  This used
# to go wrong in AutoLoader because it used regular expressions to generate
# autoloaded filename.
"foo" =~ /(\w+)/;
print "not " unless $1 eq 'foo';
print "ok 5\n";

print "not " unless $foo->bar($1) eq 'foo';
print "ok 6\n";

print "not " unless $foo->bar($1) eq 'foo';
print "ok 7\n";

print "not " unless $foo->bazmarkhianish($1) eq 'foo';
print "ok 8\n";

print "not " unless $foo->bazmarkhianish($1) eq 'foo';
print "ok 9\n";

# Check nested packages inside __DATA__
print "not " unless sheep::bleat()  eq 'baa';
print "ok 10\n";

# Now check inheritance:

$bar = new Bar;

# Before anything is SelfLoaded there is no declaration of Foo::b so we should
# get Baz::b
print "not " unless $bar->b() eq 'b Baz';
print "ok 11\n";

# There is no Bar::c so we should get Baz::c
print "not " unless $bar->c() eq 'c Baz';
print "ok 12\n";

# This selfloads Bar::a because it is stubbed. It also stubs Bar::b as a side
# effect
print "not " unless $bar->a() eq 'a Bar';
print "ok 13\n";

print "not " unless $bar->b() eq 'b Bar';
print "ok 14\n";

print "not " unless $bar->c() eq 'c Baz';
print "ok 15\n";



# Check that __END__ is honoured
# Try an subroutine that should never be noticed by selfloader
eval {
    $foo->never;
};
if ($@ =~ /^Undefined subroutine/) {
    print "ok 16\n";
} else {
    print "not ok 16 $@\n";
}

# Try to read from the data file handle
my $foodata = <Foo::DATA>;
close Foo::DATA;
if (defined $foodata) {
    print "not ok 17 # $foodata\n";
} else {
    print "ok 17\n";
}

# Check that __END__ DATA is honoured
# Try an subroutine that should never be noticed by selfloader
eval {
    $bar->never;
};
if ($@ =~ /^Undefined subroutine/) {
    print "ok 18\n";
} else {
    print "not ok 18 $@\n";
}

# Try to read from the data file handle
my $bardata = <Bar::DATA>;
close Bar::DATA;
if ($bardata ne "sub never { die \"D'oh\" }\n") {
    print "not ok 19 # $bardata\n";
} else {
    print "ok 19\n";
}

# cleanup
END {
return unless $dir && -d $dir;
unlink "$dir/Foo.pm", "$dir/Bar.pm";
rmdir "$dir";
}
