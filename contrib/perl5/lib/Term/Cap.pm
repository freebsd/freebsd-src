package Term::Cap;
use Carp;

# Last updated: Thu Dec 14 20:02:42 CST 1995 by sanders@bsdi.com

# TODO:
# support Berkeley DB termcaps
# should probably be a .xs module
# force $FH into callers package?
# keep $FH in object at Tgetent time?

=head1 NAME

Term::Cap - Perl termcap interface

=head1 SYNOPSIS

    require Term::Cap;
    $terminal = Tgetent Term::Cap { TERM => undef, OSPEED => $ospeed };
    $terminal->Trequire(qw/ce ku kd/);
    $terminal->Tgoto('cm', $col, $row, $FH);
    $terminal->Tputs('dl', $count, $FH);
    $terminal->Tpad($string, $count, $FH);

=head1 DESCRIPTION

These are low-level functions to extract and use capabilities from
a terminal capability (termcap) database.

The B<Tgetent> function extracts the entry of the specified terminal
type I<TERM> (defaults to the environment variable I<TERM>) from the
database.

It will look in the environment for a I<TERMCAP> variable.  If
found, and the value does not begin with a slash, and the terminal
type name is the same as the environment string I<TERM>, the
I<TERMCAP> string is used instead of reading a termcap file.  If
it does begin with a slash, the string is used as a path name of
the termcap file to search.  If I<TERMCAP> does not begin with a
slash and name is different from I<TERM>, B<Tgetent> searches the
files F<$HOME/.termcap>, F</etc/termcap>, and F</usr/share/misc/termcap>,
in that order, unless the environment variable I<TERMPATH> exists,
in which case it specifies a list of file pathnames (separated by
spaces or colons) to be searched B<instead>.  Whenever multiple
files are searched and a tc field occurs in the requested entry,
the entry it names must be found in the same file or one of the
succeeding files.  If there is a C<:tc=...:> in the I<TERMCAP>
environment variable string it will continue the search in the
files as above.

I<OSPEED> is the terminal output bit rate (often mistakenly called
the baud rate).  I<OSPEED> can be specified as either a POSIX
termios/SYSV termio speeds (where 9600 equals 9600) or an old
BSD-style speeds (where 13 equals 9600).

B<Tgetent> returns a blessed object reference which the user can
then use to send the control strings to the terminal using B<Tputs>
and B<Tgoto>.  It calls C<croak> on failure.

B<Tgoto> decodes a cursor addressing string with the given parameters.

The output strings for B<Tputs> are cached for counts of 1 for performance.
B<Tgoto> and B<Tpad> do not cache.  C<$self-E<gt>{_xx}> is the raw termcap
data and C<$self-E<gt>{xx}> is the cached version.

    print $terminal->Tpad($self->{_xx}, 1);

B<Tgoto>, B<Tputs>, and B<Tpad> return the string and will also
output the string to $FH if specified.

The extracted termcap entry is available in the object
as C<$self-E<gt>{TERMCAP}>.

=head1 EXAMPLES

    # Get terminal output speed
    require POSIX;
    my $termios = new POSIX::Termios;
    $termios->getattr;
    my $ospeed = $termios->getospeed;

    # Old-style ioctl code to get ospeed:
    #     require 'ioctl.pl';
    #     ioctl(TTY,$TIOCGETP,$sgtty);
    #     ($ispeed,$ospeed) = unpack('cc',$sgtty);

    # allocate and initialize a terminal structure
    $terminal = Tgetent Term::Cap { TERM => undef, OSPEED => $ospeed };

    # require certain capabilities to be available
    $terminal->Trequire(qw/ce ku kd/);

    # Output Routines, if $FH is undefined these just return the string

    # Tgoto does the % expansion stuff with the given args
    $terminal->Tgoto('cm', $col, $row, $FH);

    # Tputs doesn't do any % expansion.
    $terminal->Tputs('dl', $count = 1, $FH);

=cut

# Returns a list of termcap files to check.
sub termcap_path { ## private
    my @termcap_path;
    # $TERMCAP, if it's a filespec
    push(@termcap_path, $ENV{TERMCAP})
	if ((exists $ENV{TERMCAP}) &&
	    (($^O eq 'os2' || $^O eq 'MSWin32' || $^O eq 'dos')
	     ? $ENV{TERMCAP} =~ /^[a-z]:[\\\/]/i
	     : $ENV{TERMCAP} =~ /^\//));
    if ((exists $ENV{TERMPATH}) && ($ENV{TERMPATH})) {
	# Add the users $TERMPATH
	push(@termcap_path, split(/(:|\s+)/, $ENV{TERMPATH}))
    }
    else {
	# Defaults
	push(@termcap_path,
	    $ENV{'HOME'} . '/.termcap',
	    '/etc/termcap',
	    '/usr/share/misc/termcap',
	);
    }
    # return the list of those termcaps that exist
    grep(-f, @termcap_path);
}

sub Tgetent { ## public -- static method
    my $class = shift;
    my $self = bless shift, $class;
    my($term,$cap,$search,$field,$max,$tmp_term,$TERMCAP);
    local($termpat,$state,$first,$entry);	# used inside eval
    local $_;

    # Compute PADDING factor from OSPEED (to be used by Tpad)
    if (! $self->{OSPEED}) {
	carp "OSPEED was not set, defaulting to 9600";
	$self->{OSPEED} = 9600;
    }
    if ($self->{OSPEED} < 16) {
	# delays for old style speeds
	my @pad = (0,200,133.3,90.9,74.3,66.7,50,33.3,16.7,8.3,5.5,4.1,2,1,.5,.2);
	$self->{PADDING} = $pad[$self->{OSPEED}];
    }
    else {
	$self->{PADDING} = 10000 / $self->{OSPEED};
    }

    $self->{TERM} = ($self->{TERM} || $ENV{TERM} || croak "TERM not set");
    $term = $self->{TERM};	# $term is the term type we are looking for

    # $tmp_term is always the next term (possibly :tc=...:) we are looking for
    $tmp_term = $self->{TERM};
    # protect any pattern metacharacters in $tmp_term 
    $termpat = $tmp_term; $termpat =~ s/(\W)/\\$1/g;

    my $foo = (exists $ENV{TERMCAP} ? $ENV{TERMCAP} : '');

    # $entry is the extracted termcap entry
    if (($foo !~ m:^/:) && ($foo =~ m/(^|\|)${termpat}[:|]/)) {
	$entry = $foo;
    }

    my @termcap_path = termcap_path;
    croak "Can't find a valid termcap file" unless @termcap_path || $entry;

    $state = 1;					# 0 == finished
						# 1 == next file
						# 2 == search again

    $first = 0;					# first entry (keeps term name)

    $max = 32;					# max :tc=...:'s

    if ($entry) {
	# ok, we're starting with $TERMCAP
	$first++;				# we're the first entry
	# do we need to continue?
	if ($entry =~ s/:tc=([^:]+):/:/) {
	    $tmp_term = $1;
	    # protect any pattern metacharacters in $tmp_term 
	    $termpat = $tmp_term; $termpat =~ s/(\W)/\\$1/g;
	}
	else {
	    $state = 0;				# we're already finished
	}
    }

    # This is eval'ed inside the while loop for each file
    $search = q{
	while (<TERMCAP>) {
	    next if /^\\t/ || /^#/;
	    if ($_ =~ m/(^|\\|)${termpat}[:|]/o) {
		chomp;
		s/^[^:]*:// if $first++;
		$state = 0;
		while ($_ =~ s/\\\\$//) {
		    defined(my $x = <TERMCAP>) or last;
		    $_ .= $x; chomp;
		}
		last;
	    }
	}
	defined $entry or $entry = '';
	$entry .= $_;
    };

    while ($state != 0) {
	if ($state == 1) {
	    # get the next TERMCAP
	    $TERMCAP = shift @termcap_path
		|| croak "failed termcap lookup on $tmp_term";
	}
	else {
	    # do the same file again
	    # prevent endless recursion
	    $max-- || croak "failed termcap loop at $tmp_term";
	    $state = 1;		# ok, maybe do a new file next time
	}

	open(TERMCAP,"< $TERMCAP\0") || croak "open $TERMCAP: $!";
	eval $search;
	die $@ if $@;
	close TERMCAP;

	# If :tc=...: found then search this file again
	$entry =~ s/:tc=([^:]+):/:/ && ($tmp_term = $1, $state = 2);
	# protect any pattern metacharacters in $tmp_term 
	$termpat = $tmp_term; $termpat =~ s/(\W)/\\$1/g;
    }

    croak "Can't find $term" if $entry eq '';
    $entry =~ s/:+\s*:+/:/g;				# cleanup $entry
    $entry =~ s/:+/:/g;					# cleanup $entry
    $self->{TERMCAP} = $entry;				# save it
    # print STDERR "DEBUG: $entry = ", $entry, "\n";

    # Precompile $entry into the object
    $entry =~ s/^[^:]*://;
    foreach $field (split(/:[\s:\\]*/,$entry)) {
	if ($field =~ /^(\w\w)$/) {
	    $self->{'_' . $field} = 1 unless defined $self->{'_' . $1};
	    # print STDERR "DEBUG: flag $1\n";
	}
	elsif ($field =~ /^(\w\w)\@/) {
	    $self->{'_' . $1} = "";
	    # print STDERR "DEBUG: unset $1\n";
	}
	elsif ($field =~ /^(\w\w)#(.*)/) {
	    $self->{'_' . $1} = $2 unless defined $self->{'_' . $1};
	    # print STDERR "DEBUG: numeric $1 = $2\n";
	}
	elsif ($field =~ /^(\w\w)=(.*)/) {
	    # print STDERR "DEBUG: string $1 = $2\n";
	    next if defined $self->{'_' . ($cap = $1)};
	    $_ = $2;
	    s/\\E/\033/g;
	    s/\\(\d\d\d)/pack('c',oct($1) & 0177)/eg;
	    s/\\n/\n/g;
	    s/\\r/\r/g;
	    s/\\t/\t/g;
	    s/\\b/\b/g;
	    s/\\f/\f/g;
	    s/\\\^/\377/g;
	    s/\^\?/\177/g;
	    s/\^(.)/pack('c',ord($1) & 31)/eg;
	    s/\\(.)/$1/g;
	    s/\377/^/g;
	    $self->{'_' . $cap} = $_;
	}
	# else { carp "junk in $term ignored: $field"; }
    }
    $self->{'_pc'} = "\0" unless defined $self->{'_pc'};
    $self->{'_bc'} = "\b" unless defined $self->{'_bc'};
    $self;
}

# $terminal->Tpad($string, $cnt, $FH);
sub Tpad { ## public
    my $self = shift;
    my($string, $cnt, $FH) = @_;
    my($decr, $ms);

    if ($string =~ /(^[\d.]+)(\*?)(.*)$/) {
	$ms = $1;
	$ms *= $cnt if $2;
	$string = $3;
	$decr = $self->{PADDING};
	if ($decr > .1) {
	    $ms += $decr / 2;
	    $string .= $self->{'_pc'} x ($ms / $decr);
	}
    }
    print $FH $string if $FH;
    $string;
}

# $terminal->Tputs($cap, $cnt, $FH);
sub Tputs { ## public
    my $self = shift;
    my($cap, $cnt, $FH) = @_;
    my $string;

    if ($cnt > 1) {
	$string = Tpad($self, $self->{'_' . $cap}, $cnt);
    } else {
	# cache result because Tpad can be slow
	$string = defined $self->{$cap} ? $self->{$cap} :
	    ($self->{$cap} = Tpad($self, $self->{'_' . $cap}, 1));
    }
    print $FH $string if $FH;
    $string;
}

# %%   output `%'
# %d   output value as in printf %d
# %2   output value as in printf %2d
# %3   output value as in printf %3d
# %.   output value as in printf %c
# %+x  add x to value, then do %.
#
# %>xy if value > x then add y, no output
# %r   reverse order of two parameters, no output
# %i   increment by one, no output
# %B   BCD (16*(value/10)) + (value%10), no output
#
# %n   exclusive-or all parameters with 0140 (Datamedia 2500)
# %D   Reverse coding (value - 2*(value%16)), no output (Delta Data)
#
# $terminal->Tgoto($cap, $col, $row, $FH);
sub Tgoto { ## public
    my $self = shift;
    my($cap, $code, $tmp, $FH) = @_;
    my $string = $self->{'_' . $cap};
    my $result = '';
    my $after = '';
    my $online = 0;
    my @tmp = ($tmp,$code);
    my $cnt = $code;

    while ($string =~ /^([^%]*)%(.)(.*)/) {
	$result .= $1;
	$code = $2;
	$string = $3;
	if ($code eq 'd') {
	    $result .= sprintf("%d",shift(@tmp));
	}
	elsif ($code eq '.') {
	    $tmp = shift(@tmp);
	    if ($tmp == 0 || $tmp == 4 || $tmp == 10) {
		if ($online) {
		    ++$tmp, $after .= $self->{'_up'} if $self->{'_up'};
		}
		else {
		    ++$tmp, $after .= $self->{'_bc'};
		}
	    }
	    $result .= sprintf("%c",$tmp);
	    $online = !$online;
	}
	elsif ($code eq '+') {
	    $result .= sprintf("%c",shift(@tmp)+ord($string));
	    $string = substr($string,1,99);
	    $online = !$online;
	}
	elsif ($code eq 'r') {
	    ($code,$tmp) = @tmp;
	    @tmp = ($tmp,$code);
	    $online = !$online;
	}
	elsif ($code eq '>') {
	    ($code,$tmp,$string) = unpack("CCa99",$string);
	    if ($tmp[$[] > $code) {
		$tmp[$[] += $tmp;
	    }
	}
	elsif ($code eq '2') {
	    $result .= sprintf("%02d",shift(@tmp));
	    $online = !$online;
	}
	elsif ($code eq '3') {
	    $result .= sprintf("%03d",shift(@tmp));
	    $online = !$online;
	}
	elsif ($code eq 'i') {
	    ($code,$tmp) = @tmp;
	    @tmp = ($code+1,$tmp+1);
	}
	else {
	    return "OOPS";
	}
    }
    $string = Tpad($self, $result . $string . $after, $cnt);
    print $FH $string if $FH;
    $string;
}

# $terminal->Trequire(qw/ce ku kd/);
sub Trequire { ## public
    my $self = shift;
    my($cap,@undefined);
    foreach $cap (@_) {
	push(@undefined, $cap)
	    unless defined $self->{'_' . $cap} && $self->{'_' . $cap};
    }
    croak "Terminal does not support: (@undefined)" if @undefined;
}

1;

