#!./perl

# $Header: /home/cvs/386BSD/ports/lang/perl/t/io/dup.t,v 1.1.1.1 1993/08/23 21:30:06 nate Exp $

print "1..6\n";

print "ok 1\n";

open(dupout,">&STDOUT");
open(duperr,">&STDERR");

open(STDOUT,">Io.dup") || die "Can't open stdout";
open(STDERR,">&STDOUT") || die "Can't open stderr";

select(STDERR); $| = 1;
select(STDOUT); $| = 1;

print STDOUT "ok 2\n";
print STDERR "ok 3\n";
system 'echo ok 4';
system 'echo ok 5 1>&2';

close(STDOUT);
close(STDERR);

open(STDOUT,">&dupout");
open(STDERR,">&duperr");

system 'cat Io.dup';
unlink 'Io.dup';

print STDOUT "ok 6\n";
