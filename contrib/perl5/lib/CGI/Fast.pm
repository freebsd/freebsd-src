package CGI::Fast;

# See the bottom of this file for the POD documentation.  Search for the
# string '=head'.

# You can run this file through either pod2man or pod2html to produce pretty
# documentation in manual or html file format (these utilities are part of the
# Perl 5 distribution).

# Copyright 1995,1996, Lincoln D. Stein.  All rights reserved.
# It may be used and modified freely, but I do request that this copyright
# notice remain attached to the file.  You may modify this module as you 
# wish, but if you redistribute a modified version, please attach a note
# listing the modifications you have made.

# The most recent version and complete docs are available at:
#   http://www.genome.wi.mit.edu/ftp/pub/software/WWW/cgi_docs.html
#   ftp://ftp-genome.wi.mit.edu/pub/software/WWW/
$CGI::Fast::VERSION='1.02';

use CGI;
use FCGI;
@ISA = ('CGI');

# workaround for known bug in libfcgi
while (($ignore) = each %ENV) { }

# override the initialization behavior so that
# state is NOT maintained between invocations 
sub save_request {
    # no-op
}

# New is slightly different in that it calls FCGI's
# accept() method.
sub new {
     my ($self, $initializer, @param) = @_;
     unless (defined $initializer) {
         return undef unless FCGI::accept() >= 0;
     }
     return $CGI::Q = $self->SUPER::new($initializer, @param);
}

1;

=head1 NAME

CGI::Fast - CGI Interface for Fast CGI

=head1 SYNOPSIS

    use CGI::Fast qw(:standard);
    $COUNTER = 0;
    while (new CGI::Fast) {
	print header;
	print start_html("Fast CGI Rocks");
	print
	    h1("Fast CGI Rocks"),
	    "Invocation number ",b($COUNTER++),
            " PID ",b($$),".",
	    hr;
        print end_html;
    }

=head1 DESCRIPTION

CGI::Fast is a subclass of the CGI object created by
CGI.pm.  It is specialized to work well with the Open Market
FastCGI standard, which greatly speeds up CGI scripts by
turning them into persistently running server processes.  Scripts
that perform time-consuming initialization processes, such as
loading large modules or opening persistent database connections,
will see large performance improvements.

=head1 OTHER PIECES OF THE PUZZLE

In order to use CGI::Fast you'll need a FastCGI-enabled Web
server.  Open Market's server is FastCGI-savvy.  There are also
freely redistributable FastCGI modules for NCSA httpd 1.5 and Apache.
FastCGI-enabling modules for Microsoft Internet Information Server and
Netscape Communications Server have been announced.

In addition, you'll need a version of the Perl interpreter that has
been linked with the FastCGI I/O library.  Precompiled binaries are
available for several platforms, including DEC Alpha, HP-UX and 
SPARC/Solaris, or you can rebuild Perl from source with patches
provided in the FastCGI developer's kit.  The FastCGI Perl interpreter
can be used in place of your normal Perl without ill consequences.

You can find FastCGI modules for Apache and NCSA httpd, precompiled
Perl interpreters, and the FastCGI developer's kit all at URL:

  http://www.fastcgi.com/

=head1 WRITING FASTCGI PERL SCRIPTS

FastCGI scripts are persistent: one or more copies of the script 
are started up when the server initializes, and stay around until
the server exits or they die a natural death.  After performing
whatever one-time initialization it needs, the script enters a 
loop waiting for incoming connections, processing the request, and
waiting some more.

A typical FastCGI script will look like this:

    #!/usr/local/bin/perl    # must be a FastCGI version of perl!
    use CGI::Fast;
    &do_some_initialization();
    while ($q = new CGI::Fast) {
	&process_request($q);
    }

Each time there's a new request, CGI::Fast returns a
CGI object to your loop.  The rest of the time your script
waits in the call to new().  When the server requests that
your script be terminated, new() will return undef.  You can
of course exit earlier if you choose.  A new version of the
script will be respawned to take its place (this may be
necessary in order to avoid Perl memory leaks in long-running
scripts).

CGI.pm's default CGI object mode also works.  Just modify the loop
this way:

    while (new CGI::Fast) {
	&process_request;
    }

Calls to header(), start_form(), etc. will all operate on the
current request.

=head1 INSTALLING FASTCGI SCRIPTS

See the FastCGI developer's kit documentation for full details.  On
the Apache server, the following line must be added to srm.conf:

    AddType application/x-httpd-fcgi .fcgi

FastCGI scripts must end in the extension .fcgi.  For each script you
install, you must add something like the following to srm.conf:

   AppClass /usr/etc/httpd/fcgi-bin/file_upload.fcgi -processes 2

This instructs Apache to launch two copies of file_upload.fcgi at 
startup time.

=head1 USING FASTCGI SCRIPTS AS CGI SCRIPTS

Any script that works correctly as a FastCGI script will also work
correctly when installed as a vanilla CGI script.  However it will
not see any performance benefit.

=head1 CAVEATS

I haven't tested this very much.

=head1 AUTHOR INFORMATION

Copyright 1996-1998, Lincoln D. Stein.  All rights reserved.  

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

Address bug reports and comments to: lstein@cshl.org

=head1 BUGS

This section intentionally left blank.

=head1 SEE ALSO

L<CGI::Carp>, L<CGI>

=cut
