package Shell;

use Config;

sub import {
    my $self = shift;
    my ($callpack, $callfile, $callline) = caller;
    my @EXPORT;
    if (@_) {
	@EXPORT = @_;
    }
    else {
	@EXPORT = 'AUTOLOAD';
    }
    foreach $sym (@EXPORT) {
        *{"${callpack}::$sym"} = \&{"Shell::$sym"};
    }
};

AUTOLOAD {
    my $cmd = $AUTOLOAD;
    $cmd =~ s/^.*:://;
    eval qq {
	*$AUTOLOAD = sub {
	    if (\@_ < 1) {
		`$cmd`;
	    }
	    elsif (\$Config{'archname'} eq 'os2') {
		local(\*SAVEOUT, \*READ, \*WRITE);

		open SAVEOUT, '>&STDOUT' or die;
		pipe READ, WRITE or die;
		open STDOUT, '>&WRITE' or die;
		close WRITE;

		my \$pid = system(1, \$cmd, \@_);
		die "Can't execute $cmd: \$!\n" if \$pid < 0;

		open STDOUT, '>&SAVEOUT' or die;
		close SAVEOUT;

		if (wantarray) {
		    my \@ret = <READ>;
		    close READ;
		    waitpid \$pid, 0;
		    \@ret;
		}
		else {
		    local(\$/) = undef;
		    my \$ret = <READ>;
		    close READ;
		    waitpid \$pid, 0;
		    \$ret;
		}
	    }
	    else {
		open(SUBPROC, "-|")
			or exec '$cmd', \@_
			or die "Can't exec $cmd: \$!\n";
		if (wantarray) {
		    my \@ret = <SUBPROC>;
		    close SUBPROC;	# XXX Oughta use a destructor.
		    \@ret;
		}
		else {
		    local(\$/) = undef;
		    my \$ret = <SUBPROC>;
		    close SUBPROC;
		    \$ret;
		}
	    }
	}
    };
    goto &$AUTOLOAD;
}

1;
__END__

=head1 NAME

Shell - run shell commands transparently within perl

=head1 SYNOPSIS

See below.

=head1 DESCRIPTION

  Date: Thu, 22 Sep 94 16:18:16 -0700
  Message-Id: <9409222318.AA17072@scalpel.netlabs.com>
  To: perl5-porters@isu.edu
  From: Larry Wall <lwall@scalpel.netlabs.com>
  Subject: a new module I just wrote

Here's one that'll whack your mind a little out.

    #!/usr/bin/perl

    use Shell;

    $foo = echo("howdy", "<funny>", "world");
    print $foo;

    $passwd = cat("</etc/passwd");
    print $passwd;

    sub ps;
    print ps -ww;

    cp("/etc/passwd", "/tmp/passwd");

That's maybe too gonzo.  It actually exports an AUTOLOAD to the current
package (and uncovered a bug in Beta 3, by the way).  Maybe the usual
usage should be

    use Shell qw(echo cat ps cp);

Larry


=head1 AUTHOR

Larry Wall

=cut
