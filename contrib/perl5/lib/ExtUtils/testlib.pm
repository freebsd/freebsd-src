package ExtUtils::testlib;
$VERSION = substr q$Revision: 1.1.1.1 $, 10;
# $Id: testlib.pm,v 1.1.1.1 1998/09/09 06:59:55 markm Exp $

use lib qw(blib/arch blib/lib);
1;
__END__

=head1 NAME

ExtUtils::testlib - add blib/* directories to @INC

=head1 SYNOPSIS

C<use ExtUtils::testlib;>

=head1 DESCRIPTION

After an extension has been built and before it is installed it may be
desirable to test it bypassing C<make test>. By adding

    use ExtUtils::testlib;

to a test program the intermediate directories used by C<make> are
added to @INC.

