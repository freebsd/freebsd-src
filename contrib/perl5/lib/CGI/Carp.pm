package CGI::Carp;

=head1 NAME

B<CGI::Carp> - CGI routines for writing to the HTTPD (or other) error log

=head1 SYNOPSIS

    use CGI::Carp;

    croak "We're outta here!";
    confess "It was my fault: $!";
    carp "It was your fault!";   
    warn "I'm confused";
    die  "I'm dying.\n";

    use CGI::Carp qw(cluck);
    cluck "I wouldn't do that if I were you";

    use CGI::Carp qw(fatalsToBrowser);
    die "Fatal error messages are now sent to browser";

=head1 DESCRIPTION

CGI scripts have a nasty habit of leaving warning messages in the error
logs that are neither time stamped nor fully identified.  Tracking down
the script that caused the error is a pain.  This fixes that.  Replace
the usual

    use Carp;

with

    use CGI::Carp

And the standard warn(), die (), croak(), confess() and carp() calls
will automagically be replaced with functions that write out nicely
time-stamped messages to the HTTP server error log.

For example:

   [Fri Nov 17 21:40:43 1995] test.pl: I'm confused at test.pl line 3.
   [Fri Nov 17 21:40:43 1995] test.pl: Got an error message: Permission denied.
   [Fri Nov 17 21:40:43 1995] test.pl: I'm dying.

=head1 REDIRECTING ERROR MESSAGES

By default, error messages are sent to STDERR.  Most HTTPD servers
direct STDERR to the server's error log.  Some applications may wish
to keep private error logs, distinct from the server's error log, or
they may wish to direct error messages to STDOUT so that the browser
will receive them.

The C<carpout()> function is provided for this purpose.  Since
carpout() is not exported by default, you must import it explicitly by
saying

   use CGI::Carp qw(carpout);

The carpout() function requires one argument, which should be a
reference to an open filehandle for writing errors.  It should be
called in a C<BEGIN> block at the top of the CGI application so that
compiler errors will be caught.  Example:

   BEGIN {
     use CGI::Carp qw(carpout);
     open(LOG, ">>/usr/local/cgi-logs/mycgi-log") or
       die("Unable to open mycgi-log: $!\n");
     carpout(LOG);
   }

carpout() does not handle file locking on the log for you at this point.

The real STDERR is not closed -- it is moved to SAVEERR.  Some
servers, when dealing with CGI scripts, close their connection to the
browser when the script closes STDOUT and STDERR.  SAVEERR is used to
prevent this from happening prematurely.

You can pass filehandles to carpout() in a variety of ways.  The "correct"
way according to Tom Christiansen is to pass a reference to a filehandle 
GLOB:

    carpout(\*LOG);

This looks weird to mere mortals however, so the following syntaxes are
accepted as well:

    carpout(LOG);
    carpout(main::LOG);
    carpout(main'LOG);
    carpout(\LOG);
    carpout(\'main::LOG');

    ... and so on

FileHandle and other objects work as well.

Use of carpout() is not great for performance, so it is recommended
for debugging purposes or for moderate-use applications.  A future
version of this module may delay redirecting STDERR until one of the
CGI::Carp methods is called to prevent the performance hit.

=head1 MAKING PERL ERRORS APPEAR IN THE BROWSER WINDOW

If you want to send fatal (die, confess) errors to the browser, ask to 
import the special "fatalsToBrowser" subroutine:

    use CGI::Carp qw(fatalsToBrowser);
    die "Bad error here";

Fatal errors will now be echoed to the browser as well as to the log.  CGI::Carp
arranges to send a minimal HTTP header to the browser so that even errors that
occur in the early compile phase will be seen.
Nonfatal errors will still be directed to the log file only (unless redirected
with carpout).

=head2 Changing the default message

By default, the software error message is followed by a note to
contact the Webmaster by e-mail with the time and date of the error.
If this message is not to your liking, you can change it using the
set_message() routine.  This is not imported by default; you should
import it on the use() line:

    use CGI::Carp qw(fatalsToBrowser set_message);
    set_message("It's not a bug, it's a feature!");

You may also pass in a code reference in order to create a custom
error message.  At run time, your code will be called with the text
of the error message that caused the script to die.  Example:

    use CGI::Carp qw(fatalsToBrowser set_message);
    BEGIN {
       sub handle_errors {
          my $msg = shift;
          print "<h1>Oh gosh</h1>";
          print "Got an error: $msg";
      }
      set_message(\&handle_errors);
    }

In order to correctly intercept compile-time errors, you should call
set_message() from within a BEGIN{} block.

=head1 CHANGE LOG

1.05 carpout() added and minor corrections by Marc Hedlund
     <hedlund@best.com> on 11/26/95.

1.06 fatalsToBrowser() no longer aborts for fatal errors within
     eval() statements.

1.08 set_message() added and carpout() expanded to allow for FileHandle
     objects.

1.09 set_message() now allows users to pass a code REFERENCE for 
     really custom error messages.  croak and carp are now
     exported by default.  Thanks to Gunther Birznieks for the
     patches.

1.10 Patch from Chris Dean (ctdean@cogit.com) to allow 
     module to run correctly under mod_perl.

1.11 Changed order of &gt; and &lt; escapes.

1.12 Changed die() on line 217 to CORE::die to avoid B<-w> warning.

1.13 Added cluck() to make the module orthogonal with Carp.
    More mod_perl related fixes.

=head1 AUTHORS

Copyright 1995-1998, Lincoln D. Stein.  All rights reserved.  

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

Address bug reports and comments to: lstein@cshl.org

=head1 SEE ALSO

Carp, CGI::Base, CGI::BasePlus, CGI::Request, CGI::MiniSvr, CGI::Form,
CGI::Response

=cut

require 5.000;
use Exporter;
use Carp;

@ISA = qw(Exporter);
@EXPORT = qw(confess croak carp);
@EXPORT_OK = qw(carpout fatalsToBrowser wrap set_message cluck);

$main::SIG{__WARN__}=\&CGI::Carp::warn;
$main::SIG{__DIE__}=\&CGI::Carp::die;
$CGI::Carp::VERSION = '1.13';
$CGI::Carp::CUSTOM_MSG = undef;

# fancy import routine detects and handles 'errorWrap' specially.
sub import {
    my $pkg = shift;
    my(%routines);
    grep($routines{$_}++,@_,@EXPORT);
    $WRAP++ if $routines{'fatalsToBrowser'} || $routines{'wrap'};
    my($oldlevel) = $Exporter::ExportLevel;
    $Exporter::ExportLevel = 1;
    Exporter::import($pkg,keys %routines);
    $Exporter::ExportLevel = $oldlevel;
}

# These are the originals
sub realwarn { CORE::warn(@_); }
sub realdie { CORE::die(@_); }

sub id {
    my $level = shift;
    my($pack,$file,$line,$sub) = caller($level);
    my($id) = $file=~m|([^/]+)$|;
    return ($file,$line,$id);
}

sub stamp {
    my $time = scalar(localtime);
    my $frame = 0;
    my ($id,$pack,$file);
    do {
	$id = $file;
	($pack,$file) = caller($frame++);
    } until !$file;
    ($id) = $id=~m|([^/]+)$|;
    return "[$time] $id: ";
}

sub warn {
    my $message = shift;
    my($file,$line,$id) = id(1);
    $message .= " at $file line $line.\n" unless $message=~/\n$/;
    my $stamp = stamp;
    $message=~s/^/$stamp/gm;
    realwarn $message;
}

# The mod_perl package Apache::Registry loads CGI programs by calling
# eval.  These evals don't count when looking at the stack backtrace.
sub _longmess {
    my $message = Carp::longmess();
    my $mod_perl = exists $ENV{MOD_PERL};
    $message =~ s,eval[^\n]+Apache/Registry\.pm.*,,s if $mod_perl;
    return( $message );    
}

sub die {
    my $message = shift;
    my $time = scalar(localtime);
    my($file,$line,$id) = id(1);
    $message .= " at $file line $line." unless $message=~/\n$/;
    &fatalsToBrowser($message) if $WRAP && _longmess() !~ /eval [{\']/m;
    my $stamp = stamp;
    $message=~s/^/$stamp/gm;
    realdie $message;
}

sub set_message {
    $CGI::Carp::CUSTOM_MSG = shift;
    return $CGI::Carp::CUSTOM_MSG;
}

# Avoid generating "subroutine redefined" warnings with the following
# hack:
{
    local $^W=0;
    eval <<EOF;
sub confess { CGI::Carp::die Carp::longmess \@_; }
sub croak   { CGI::Carp::die Carp::shortmess \@_; }
sub carp    { CGI::Carp::warn Carp::shortmess \@_; }
sub cluck   { CGI::Carp::warn Carp::longmess \@_; }
EOF
    ;
}

# We have to be ready to accept a filehandle as a reference
# or a string.
sub carpout {
    my($in) = @_;
    my($no) = fileno(to_filehandle($in));
    realdie("Invalid filehandle $in\n") unless defined $no;
    
    open(SAVEERR, ">&STDERR");
    open(STDERR, ">&$no") or 
	( print SAVEERR "Unable to redirect STDERR: $!\n" and exit(1) );
}

# headers
sub fatalsToBrowser {
    my($msg) = @_;
    $msg=~s/&/&amp;/g;
    $msg=~s/>/&gt;/g;
    $msg=~s/</&lt;/g;
    $msg=~s/\"/&quot;/g;
    my($wm) = $ENV{SERVER_ADMIN} ? 
	qq[the webmaster (<a href="mailto:$ENV{SERVER_ADMIN}">$ENV{SERVER_ADMIN}</a>)] :
	"this site's webmaster";
    my ($outer_message) = <<END;
For help, please send mail to $wm, giving this error message 
and the time and date of the error.
END
    ;
    my $mod_perl = exists $ENV{MOD_PERL};
    print STDOUT "Content-type: text/html\n\n" 
	unless $mod_perl;

    if ($CUSTOM_MSG) {
	if (ref($CUSTOM_MSG) eq 'CODE') {
	    &$CUSTOM_MSG($msg); # nicer to perl 5.003 users
	    return;
	} else {
	    $outer_message = $CUSTOM_MSG;
	}
    }
    
    my $mess = <<END;
<H1>Software error:</H1>
<CODE>$msg</CODE>
<P>
$outer_message
END
    ;

    if ($mod_perl) {
	my $r = Apache->request;
	# If bytes have already been sent, then
	# we print the message out directly.
	# Otherwise we make a custom error
	# handler to produce the doc for us.
	if ($r->bytes_sent) {
	    $r->print($mess);
	    $r->exit;
	} else {
	    $r->status(500);
	    $r->custom_response(500,$mess);
	}
    } else {
	print STDOUT $mess;
    }
}

# Cut and paste from CGI.pm so that we don't have the overhead of
# always loading the entire CGI module.
sub to_filehandle {
    my $thingy = shift;
    return undef unless $thingy;
    return $thingy if UNIVERSAL::isa($thingy,'GLOB');
    return $thingy if UNIVERSAL::isa($thingy,'FileHandle');
    if (!ref($thingy)) {
	my $caller = 1;
	while (my $package = caller($caller++)) {
	    my($tmp) = $thingy=~/[\':]/ ? $thingy : "$package\:\:$thingy"; 
	    return $tmp if defined(fileno($tmp));
	}
    }
    return undef;
}

1;
