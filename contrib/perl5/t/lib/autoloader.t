#!./perl

BEGIN {
    chdir 't' if -d 't';
    $dir = "auto-$$";
    @INC = $dir;
    push @INC, '../lib';
}

print "1..11\n";

# First we must set up some autoloader files
mkdir $dir, 0755            or die "Can't mkdir $dir: $!";
mkdir "$dir/auto", 0755     or die "Can't mkdir: $!";
mkdir "$dir/auto/Foo", 0755 or die "Can't mkdir: $!";

open(FOO, ">$dir/auto/Foo/foo.al") or die;
print FOO <<'EOT';
package Foo;
sub foo { shift; shift || "foo" }
1;
EOT
close(FOO);

open(BAR, ">$dir/auto/Foo/bar.al") or die;
print BAR <<'EOT';
package Foo;
sub bar { shift; shift || "bar" }
1;
EOT
close(BAR);

open(BAZ, ">$dir/auto/Foo/bazmarkhian.al") or die;
print BAZ <<'EOT';
package Foo;
sub bazmarkhianish { shift; shift || "baz" }
1;
EOT
close(BAZ);

# Let's define the package
package Foo;
require AutoLoader;
@ISA=qw(AutoLoader);

sub new { bless {}, shift };

package main;

$foo = new Foo;

print "not " unless $foo->foo eq 'foo';  # autoloaded first time
print "ok 1\n";

print "not " unless $foo->foo eq 'foo';  # regular call
print "ok 2\n";

# Try an undefined method
eval {
    $foo->will_fail;
};
print "not " unless $@ =~ /^Can't locate/;
print "ok 3\n";

# Used to be trouble with this
eval {
    my $foo = new Foo;
    die "oops";
};
print "not " unless $@ =~ /oops/;
print "ok 4\n";

# Pass regular expression variable to autoloaded function.  This used
# to go wrong because AutoLoader used regular expressions to generate
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

# test recursive autoloads
open(F, ">$dir/auto/Foo/a.al") or die;
print F <<'EOT';
package Foo;
BEGIN { b() }
sub a { print "ok 11\n"; }
1;
EOT
close(F);

open(F, ">$dir/auto/Foo/b.al") or die;
print F <<'EOT';
package Foo;
sub b { print "ok 10\n"; }
1;
EOT
close(F);
Foo::a();

# cleanup
END {
return unless $dir && -d $dir;
unlink "$dir/auto/Foo/foo.al";
unlink "$dir/auto/Foo/bar.al";
unlink "$dir/auto/Foo/bazmarkhian.al";
unlink "$dir/auto/Foo/a.al";
unlink "$dir/auto/Foo/b.al";
rmdir "$dir/auto/Foo";
rmdir "$dir/auto";
rmdir "$dir";
}
