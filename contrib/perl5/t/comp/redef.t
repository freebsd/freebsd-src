#!./perl -w
#
# Contributed by Graham Barr <Graham.Barr@tiuk.ti.com>

BEGIN {
    $warn = "";
    $SIG{__WARN__} = sub { $warn .= join("",@_) }
}

sub ok ($$) { 
    print $_[1] ? "ok " : "not ok ", $_[0], "\n";
}

print "1..18\n";

my $NEWPROTO = 'Prototype mismatch:';

sub sub0 { 1 }
sub sub0 { 2 }

ok 1, $warn =~ s/Subroutine sub0 redefined[^\n]+\n//s;

sub sub1    { 1 }
sub sub1 () { 2 }

ok 2, $warn =~ s/$NEWPROTO \Qsub main::sub1 vs ()\E[^\n]+\n//s;
ok 3, $warn =~ s/Subroutine sub1 redefined[^\n]+\n//s;

sub sub2     { 1 }
sub sub2 ($) { 2 }

ok 4, $warn =~ s/$NEWPROTO \Qsub main::sub2 vs ($)\E[^\n]+\n//s;
ok 5, $warn =~ s/Subroutine sub2 redefined[^\n]+\n//s;

sub sub3 () { 1 }
sub sub3    { 2 }

ok 6, $warn =~ s/$NEWPROTO \Qsub main::sub3 () vs none\E[^\n]+\n//s;
ok 7, $warn =~ s/Constant subroutine sub3 redefined[^\n]+\n//s;

sub sub4 () { 1 }
sub sub4 () { 2 }

ok 8, $warn =~ s/Constant subroutine sub4 redefined[^\n]+\n//s;

sub sub5 ()  { 1 }
sub sub5 ($) { 2 }

ok  9, $warn =~ s/$NEWPROTO \Qsub main::sub5 () vs ($)\E[^\n]+\n//s;
ok 10, $warn =~ s/Constant subroutine sub5 redefined[^\n]+\n//s;

sub sub6 ($) { 1 }
sub sub6     { 2 }

ok 11, $warn =~ s/$NEWPROTO \Qsub main::sub6 ($) vs none\E[^\n]+\n//s;
ok 12, $warn =~ s/Subroutine sub6 redefined[^\n]+\n//s;

sub sub7 ($) { 1 }
sub sub7 ()  { 2 }

ok 13, $warn =~ s/$NEWPROTO \Qsub main::sub7 ($) vs ()\E[^\n]+\n//s;
ok 14, $warn =~ s/Subroutine sub7 redefined[^\n]+\n//s;

sub sub8 ($) { 1 }
sub sub8 ($) { 2 }

ok 15, $warn =~ s/Subroutine sub8 redefined[^\n]+\n//s;

sub sub9 ($@) { 1 }
sub sub9 ($)  { 2 }

ok 16, $warn =~ s/$NEWPROTO sub main::sub9 \(\$\Q@) vs ($)\E[^\n]+\n//s;
ok 17, $warn =~ s/Subroutine sub9 redefined[^\n]+\n//s;

ok 18, $_ eq '';

# If we got any errors that we were not expecting, then print them
print $_ if length $_;


