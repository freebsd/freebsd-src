#!./perl

#
# various typeglob tests
#

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}   

use warnings;

print "1..40\n";

# type coersion on assignment
$foo = 'foo';
$bar = *main::foo;
$bar = $foo;
print ref(\$bar) eq 'SCALAR' ? "ok 1\n" : "not ok 1\n";
$foo = *main::bar;

# type coersion (not) on misc ops

if ($foo) {
  print ref(\$foo) eq 'GLOB' ? "ok 2\n" : "not ok 2\n";
}

unless ($foo =~ /abcd/) {
  print ref(\$foo) eq 'GLOB' ? "ok 3\n" : "not ok 3\n";
}

if ($foo eq '*main::bar') {
  print ref(\$foo) eq 'GLOB' ? "ok 4\n" : "not ok 4\n";
}

# type coersion on substitutions that match
$a = *main::foo;
$b = $a;
$a =~ s/^X//;
print ref(\$a) eq 'GLOB' ? "ok 5\n" : "not ok 5\n";
$a =~ s/^\*//;
print $a eq 'main::foo' ? "ok 6\n" : "not ok 6\n";
print ref(\$b) eq 'GLOB' ? "ok 7\n" : "not ok 7\n";

# typeglobs as lvalues
substr($foo, 0, 1) = "XXX";
print ref(\$foo) eq 'SCALAR' ? "ok 8\n" : "not ok 8\n";
print $foo eq 'XXXmain::bar' ? "ok 9\n" : "not ok 9\n";

# returning glob values
sub foo {
  local($bar) = *main::foo;
  $foo = *main::bar;
  return ($foo, $bar);
}

($fuu, $baa) = foo();
if (defined $fuu) {
  print ref(\$fuu) eq 'GLOB' ? "ok 10\n" : "not ok 10\n";
}

if (defined $baa) {
  print ref(\$baa) eq 'GLOB' ? "ok 11\n" : "not ok 11\n";
}

# nested package globs
# NOTE:  It's probably OK if these semantics change, because the
#        fact that %X::Y:: is stored in %X:: isn't documented.
#        (I hope.)

{ package Foo::Bar; no warnings 'once'; $test=1; }
print exists $Foo::{'Bar::'} ? "ok 12\n" : "not ok 12\n";
print $Foo::{'Bar::'} eq '*Foo::Bar::' ? "ok 13\n" : "not ok 13\n";

# test undef operator clearing out entire glob
$foo = 'stuff';
@foo = qw(more stuff);
%foo = qw(even more random stuff);
undef *foo;
print +($foo || @foo || %foo) ? "not ok" : "ok", " 14\n";

# test warnings from assignment of undef to glob
{
    my $msg;
    local $SIG{__WARN__} = sub { $msg = $_[0] };
    use warnings;
    *foo = 'bar';
    print $msg ? "not ok" : "ok", " 15\n";
    *foo = undef;
    print $msg ? "ok" : "not ok", " 16\n";
}

# test *glob{THING} syntax
$x = "ok 17\n";
@x = ("ok 18\n");
%x = ("ok 19" => "\n");
sub x { "ok 20\n" }
print ${*x{SCALAR}}, @{*x{ARRAY}}, %{*x{HASH}}, &{*x{CODE}};
*x = *STDOUT;
print *{*x{GLOB}} eq "*main::STDOUT" ? "ok 21\n" : "not ok 21\n";
print {*x{IO}} "ok 22\n";
print {*x{FILEHANDLE}} "ok 23\n";

# test if defined() doesn't create any new symbols

{
    my $test = 23;

    my $a = "SYM000";
    print "not " if defined *{$a};
    ++$test; print "ok $test\n";

    print "not " if defined @{$a} or defined *{$a};
    ++$test; print "ok $test\n";

    print "not " if defined %{$a} or defined *{$a};
    ++$test; print "ok $test\n";

    print "not " if defined ${$a} or defined *{$a};
    ++$test; print "ok $test\n";

    print "not " if defined &{$a} or defined *{$a};
    ++$test; print "ok $test\n";

    *{$a} = sub { print "ok $test\n" };
    print "not " unless defined &{$a} and defined *{$a};
    ++$test; &{$a};
}

# although it *should* if you're talking about magicals

{
    my $test = 29;

    my $a = "]";
    print "not " unless defined ${$a};
    ++$test; print "ok $test\n";
    print "not " unless defined *{$a};
    ++$test; print "ok $test\n";

    $a = "1";
    "o" =~ /(o)/;
    print "not " unless ${$a};
    ++$test; print "ok $test\n";
    print "not " unless defined *{$a};
    ++$test; print "ok $test\n";
    $a = "2";
    print "not " if ${$a};
    ++$test; print "ok $test\n";
    print "not " unless defined *{$a};
    ++$test; print "ok $test\n";
    $a = "1x";
    print "not " if defined ${$a};
    ++$test; print "ok $test\n";
    print "not " if defined *{$a};
    ++$test; print "ok $test\n";
    $a = "11";
    "o" =~ /(((((((((((o)))))))))))/;
    print "not " unless ${$a};
    ++$test; print "ok $test\n";
    print "not " unless defined *{$a};
    ++$test; print "ok $test\n";
}


# does pp_readline() handle glob-ness correctly?

{
    my $g = *foo;
    $g = <DATA>;
    print $g;
}

__END__
ok 40
