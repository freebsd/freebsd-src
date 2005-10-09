#!/usr/bin/perl -w
#-
# Copyright (c) 2002-2003 Networks Associates Technology, Inc.
# All rights reserved.
#
# This software was developed for the FreeBSD Project by ThinkSec AS and
# Network Associates Laboratories, the Security Research Division of
# Network Associates, Inc.  under DARPA/SPAWAR contract N66001-01-C-8035
# ("CBOSS"), as part of the DARPA CHATS research program.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote
#    products derived from this software without specific prior written
#    permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $P4: //depot/projects/openpam/misc/gendoc.pl#30 $
#

use strict;
use locale;
use Fcntl;
use Getopt::Std;
use POSIX qw(locale_h strftime);
use vars qw($COPYRIGHT $TODAY %FUNCTIONS %PAMERR);

$COPYRIGHT = ".\\\"-
.\\\" Copyright (c) 2001-2003 Networks Associates Technology, Inc.
.\\\" All rights reserved.
.\\\"
.\\\" This software was developed for the FreeBSD Project by ThinkSec AS and
.\\\" Network Associates Laboratories, the Security Research Division of
.\\\" Network Associates, Inc. under DARPA/SPAWAR contract N66001-01-C-8035
.\\\" (\"CBOSS\"), as part of the DARPA CHATS research program.
.\\\"
.\\\" Redistribution and use in source and binary forms, with or without
.\\\" modification, are permitted provided that the following conditions
.\\\" are met:
.\\\" 1. Redistributions of source code must retain the above copyright
.\\\"    notice, this list of conditions and the following disclaimer.
.\\\" 2. Redistributions in binary form must reproduce the above copyright
.\\\"    notice, this list of conditions and the following disclaimer in the
.\\\"    documentation and/or other materials provided with the distribution.
.\\\" 3. The name of the author may not be used to endorse or promote
.\\\"    products derived from this software without specific prior written
.\\\"    permission.
.\\\"
.\\\" THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
.\\\" ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
.\\\" IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
.\\\" ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
.\\\" FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
.\\\" DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
.\\\" OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
.\\\" HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
.\\\" LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
.\\\" OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
.\\\" SUCH DAMAGE.
.\\\"
.\\\" \$" . "P4" . "\$
.\\\"";

%PAMERR = (
    PAM_SUCCESS			=> "Success",
    PAM_OPEN_ERR		=> "Failed to load module",
    PAM_SYMBOL_ERR		=> "Invalid symbol",
    PAM_SERVICE_ERR		=> "Error in service module",
    PAM_SYSTEM_ERR		=> "System error",
    PAM_BUF_ERR			=> "Memory buffer error",
    PAM_CONV_ERR		=> "Conversation failure",
    PAM_PERM_DENIED		=> "Permission denied",
    PAM_MAXTRIES		=> "Maximum number of tries exceeded",
    PAM_AUTH_ERR		=> "Authentication error",
    PAM_NEW_AUTHTOK_REQD	=> "New authentication token required",
    PAM_CRED_INSUFFICIENT	=> "Insufficient credentials",
    PAM_AUTHINFO_UNAVAIL	=> "Authentication information is unavailable",
    PAM_USER_UNKNOWN		=> "Unknown user",
    PAM_CRED_UNAVAIL		=> "Failed to retrieve user credentials",
    PAM_CRED_EXPIRED		=> "User credentials have expired",
    PAM_CRED_ERR		=> "Failed to set user credentials",
    PAM_ACCT_EXPIRED		=> "User account has expired",
    PAM_AUTHTOK_EXPIRED		=> "Password has expired",
    PAM_SESSION_ERR		=> "Session failure",
    PAM_AUTHTOK_ERR		=> "Authentication token failure",
    PAM_AUTHTOK_RECOVERY_ERR	=> "Failed to recover old authentication token",
    PAM_AUTHTOK_LOCK_BUSY	=> "Authentication token lock busy",
    PAM_AUTHTOK_DISABLE_AGING	=> "Authentication token aging disabled",
    PAM_NO_MODULE_DATA		=> "Module data not found",
    PAM_IGNORE			=> "Ignore this module",
    PAM_ABORT			=> "General failure",
    PAM_TRY_AGAIN		=> "Try again",
    PAM_MODULE_UNKNOWN		=> "Unknown module type",
    PAM_DOMAIN_UNKNOWN		=> "Unknown authentication domain",
);

sub parse_source($) {
    my $fn = shift;

    local *FILE;
    my $source;
    my $func;
    my $descr;
    my $type;
    my $args;
    my $argnames;
    my $man;
    my $inlist;
    my $inliteral;
    my %xref;
    my @errors;

    if ($fn !~ m,\.c$,) {
	warn("$fn: not C source, ignoring\n");
	return undef;
    }

    sysopen(FILE, $fn, O_RDONLY)
	or die("$fn: open(): $!\n");
    $source = join('', <FILE>);
    close(FILE);

    return undef
	if ($source =~ m/^ \* NOPARSE\s*$/m);

    $func = $fn;
    $func =~ s,^(?:.*/)?([^/]+)\.c$,$1,;
    if ($source !~ m,\n \* ([\S ]+)\n \*/\n\n([\S ]+)\n$func\((.*?)\)\n\{,s) {
	warn("$fn: can't find $func\n");
	return undef;
    }
    ($descr, $type, $args) = ($1, $2, $3);
    $descr =~ s,^([A-Z][a-z]),lc($1),e;
    $descr =~ s,[\.\s]*$,,;
    while ($args =~ s/^((?:[^\(]|\([^\)]*\))*),\s*/$1\" \"/g) {
	# nothing
    }
    $args =~ s/,\s+/, /gs;
    $args = "\"$args\"";

    %xref = (
	3 => { 'pam' => 1 },
    );

    if ($type eq "int") {
	foreach (split("\n", $source)) {
	    next unless (m/^ \*\s+(!?PAM_[A-Z_]+|=[a-z_]+)\s*$/);
	    push(@errors, $1);
	}
	++$xref{3}->{'pam_strerror'};
    }

    $argnames = $args;
    # extract names of regular arguments
    $argnames =~ s/\"[^\"]+\*?\b(\w+)\"/\"$1\"/g;
    # extract names of function pointer arguments
    $argnames =~ s/\"([\w\s\*]+)\(\*?(\w+)\)\([^\)]+\)\"/\"$2\"/g;
    # escape metacharacters (there shouldn't be any, but...)
    $argnames =~ s/([\|\[\]\(\)\.\*\+\?])/\\$1/g;
    # separate argument names with |
    $argnames =~ s/\" \"/|/g;
    # and surround with ()
    $argnames =~ s/^\"(.*)\"$/($1)/;
    # $argnames is now a regexp that matches argument names
    $inliteral = $inlist = 0;
    foreach (split("\n", $source)) {
	s/\s*$//;
	if (!defined($man)) {
	    if (m/^\/\*\*$/) {
		$man = "";
	    }
	    next;
	}
	last if (m/^ \*\/$/);
	s/^ \* ?//;
	s/\\(.)/$1/gs;
	if (m/^$/) {
	    if ($man ne "" && $man !~ m/\.Pp\n$/s) {
		if ($inliteral) {
		    $man .= "\0\n";
		} elsif ($inlist) {
		    $man .= ".El\n.Pp\n";
		    $inlist = 0;
		} else {
		    $man .= ".Pp\n";
		}
	    }
	    next;
	}
	if (m/^>(\w+)(\s+\d)?$/) {
	    my ($page, $sect) = ($1, $2 ? int($2) : 3);
	    ++$xref{$sect}->{$page};
	    next;
	}
	if (s/^\s+(=?\w+):\s*/.It $1/) {
	    if ($inliteral) {
		$man .= ".Ed\n";
		$inliteral = 0;
	    }
	    if (!$inlist) {
		$man =~ s/\.Pp\n$//s;
		$man .= ".Bl -tag -width 18n\n";
		$inlist = 1;
	    }
	    s/^\.It =([A-Z][A-Z_]+)$/.It Dv $1/gs;
	    $man .= "$_\n";
	    next;
	} elsif ($inlist && m/^\S/) {
	    $man .= ".El\n.Pp\n";
	    $inlist = 0;
	} elsif ($inliteral && m/^\S/) {
	    $man .= ".Ed\n";
	    $inliteral = 0;
	} elsif ($inliteral) {
	    $man .= "$_\n";
	    next;
	} elsif ($inlist) {
	    s/^\s+//;
	} elsif (m/^\s+/) {
	    $man .= ".Bd -literal\n";
	    $inliteral = 1;
	    $man .= "$_\n";
	    next;
	}
	s/\s*=$func\b\s*/\n.Nm\n/gs;
	s/\s*=$argnames\b\s*/\n.Fa $1\n/gs;
	s/\s*=(struct \w+(?: \*)?)\b\s*/\n.Vt $1\n/gs;
	s/\s*:([a-z_]+)\b\s*/\n.Va $1\n/gs;
	s/\s*;([a-z_]+)\b\s*/\n.Dv $1\n/gs;
	while (s/\s*=([a-z_]+)\b\s*/\n.Xr $1 3\n/s) {
	    ++$xref{3}->{$1};
	}
	s/\s*\"(?=\w)/\n.Do\n/gs;
	s/\"(?!\w)\s*/\n.Dc\n/gs;
	s/\s*=([A-Z][A-Z_]+)\b\s*(?![\.,:;])/\n.Dv $1\n/gs;
	s/\s*=([A-Z][A-Z_]+)\b([\.,:;]+)\s*/\n.Dv $1 $2\n/gs;
	s/\s*{([A-Z][a-z] .*?)}\s*/\n.$1\n/gs;
	$man .= "$_\n";
    }
    if (defined($man)) {
	if ($inlist) {
	    $man .= ".El\n";
	}
	if ($inliteral) {
	    $man .= ".Ed\n";
	}
	$man =~ s/(\n\.[A-Z][a-z] [\w ]+)\n([\.,:;-]\S*)\s*/$1 $2\n/gs;
	$man =~ s/\s*$/\n/gm;
	$man =~ s/\n+/\n/gs;
	$man =~ s/\0//gs;
	$man =~ s/\n\n\./\n\./gs;
	chomp($man);
    } else {
	$man = "No description available.";
    }

    $FUNCTIONS{$func} = {
	'source'	=> $fn,
	'name'		=> $func,
	'descr'		=> $descr,
	'type'		=> $type,
	'args'		=> $args,
	'man'		=> $man,
	'xref'		=> \%xref,
	'errors'	=> \@errors,
    };
    if ($source =~ m/^ \* NODOC\s*$/m) {
	$FUNCTIONS{$func}->{'nodoc'} = 1;
    }
    if ($source !~ m/^ \* XSSO \d/m) {
	$FUNCTIONS{$func}->{'openpam'} = 1;
    }
    expand_errors($FUNCTIONS{$func});
    return $FUNCTIONS{$func};
}

sub expand_errors($);
sub expand_errors($) {
    my $func = shift;		# Ref to function hash

    my %errors;
    my $ref;
    my $fn;

    if (defined($func->{'recursed'})) {
	warn("$func->{'name'}(): loop in error spec\n");
	return qw();
    }
    $func->{'recursed'} = 1;

    foreach (@{$func->{'errors'}}) {
	if (m/^(PAM_[A-Z_]+)$/) {
	    if (!defined($PAMERR{$1})) {
		warn("$func->{'name'}(): unrecognized error: $1\n");
		next;
	    }
	    $errors{$1} = 1;
	} elsif (m/^!(PAM_[A-Z_]+)$/) {
	    # treat negations separately
	} elsif (m/^=([a-z_]+)$/) {
	    $ref = $1;
	    if (!defined($FUNCTIONS{$ref})) {
		$fn = $func->{'source'};
		$fn =~ s/$func->{'name'}/$ref/;
		parse_source($fn);
	    }
	    if (!defined($FUNCTIONS{$ref})) {
		warn("$func->{'name'}(): reference to unknown $ref()\n");
		next;
	    }
	    foreach (@{$FUNCTIONS{$ref}->{'errors'}}) {
		$errors{$_} = 1;
	    }
	} else {
	    warn("$func->{'name'}(): invalid error specification: $_\n");
	}
    }
    foreach (@{$func->{'errors'}}) {
	if (m/^!(PAM_[A-Z_]+)$/) {
	    delete($errors{$1});
	}
    }
    delete($func->{'recursed'});
    $func->{'errors'} = [ sort(keys(%errors)) ];
}

sub dictionary_order($$) {
    my ($a, $b) = @_;

    $a =~ s/[^[:alpha:]]//g;
    $b =~ s/[^[:alpha:]]//g;
    $a cmp $b;
}

sub genxref($) {
    my $xref = shift;		# References

    my $mdoc = '';
    my @refs = ();
    foreach my $sect (sort(keys(%{$xref}))) {
	foreach my $page (sort(dictionary_order keys(%{$xref->{$sect}}))) {
	    push(@refs, "$page $sect");
	}
    }
    while ($_ = shift(@refs)) {
	$mdoc .= ".Xr $_" .
	    (@refs ? " ,\n" : "\n");
    }
    return $mdoc;
}

sub gendoc($) {
    my $func = shift;		# Ref to function hash

    local *FILE;
    my $mdoc;
    my $fn;

    return if defined($func->{'nodoc'});

    $mdoc = "$COPYRIGHT
.Dd $TODAY
.Dt " . uc($func->{'name'}) . " 3
.Os
.Sh NAME
.Nm $func->{'name'}
.Nd $func->{'descr'}
.Sh LIBRARY
.Lb libpam
.Sh SYNOPSIS
.In sys/types.h
.In security/pam_appl.h
";
    if ($func->{'name'} =~ m/_sm_/) {
	$mdoc .= ".In security/pam_modules.h\n"
    }
    if ($func->{'name'} =~ m/openpam/) {
	$mdoc .= ".In security/openpam.h\n"
    }
    $mdoc .= ".Ft \"$func->{'type'}\"
.Fn $func->{'name'} $func->{'args'}
.Sh DESCRIPTION
$func->{'man'}
";
    if ($func->{'type'} eq "int") {
	$mdoc .= ".Sh RETURN VALUES
The
.Nm
function returns one of the following values:
.Bl -tag -width 18n
";
	my @errors = @{$func->{'errors'}};
	warn("$func->{'name'}(): no error specification\n")
	    unless(@errors);
	foreach (@errors) {
	    $mdoc .= ".It Bq Er $_\n$PAMERR{$_}.\n";
	}
	$mdoc .= ".El\n";
    } else {
	if ($func->{'type'} =~ m/\*$/) {
	    $mdoc .= ".Sh RETURN VALUES
The
.Nm
function returns
.Dv NULL
on failure.
";
	}
    }
    $mdoc .= ".Sh SEE ALSO\n" . genxref($func->{'xref'});
    $mdoc .= ".Sh STANDARDS\n";
    if ($func->{'openpam'}) {
	$mdoc .= "The
.Nm
function is an OpenPAM extension.
";
    } else {
	$mdoc .= ".Rs
.%T \"X/Open Single Sign-On Service (XSSO) - Pluggable Authentication Modules\"
.%D \"June 1997\"
.Re
";
    }
    $mdoc .= ".Sh AUTHORS
The
.Nm
function and this manual page were developed for the
.Fx
Project by ThinkSec AS and Network Associates Laboratories, the
Security Research Division of Network Associates, Inc.\\& under
DARPA/SPAWAR contract N66001-01-C-8035
.Pq Dq CBOSS ,
as part of the DARPA CHATS research program.
";

    $fn = "$func->{'name'}.3";
    if (sysopen(FILE, $fn, O_RDWR|O_CREAT|O_TRUNC)) {
	print(FILE $mdoc);
	close(FILE);
    } else {
	warn("$fn: open(): $!\n");
    }
}

sub readproto($) {
    my $fn = shift;		# File name

    local *FILE;
    my %func;

    sysopen(FILE, $fn, O_RDONLY)
	or die("$fn: open(): $!\n");
    while (<FILE>) {
	if (m/^\.Nm ((?:open)?pam_.*?)\s*$/) {
	    $func{'Nm'} = $func{'Nm'} || $1;
	} elsif (m/^\.Ft (\S.*?)\s*$/) {
	    $func{'Ft'} = $func{'Ft'} || $1;
	} elsif (m/^\.Fn (\S.*?)\s*$/) {
	    $func{'Fn'} = $func{'Fn'} || $1;
	}
    }
    close(FILE);
    if ($func{'Nm'}) {
	$FUNCTIONS{$func{'Nm'}} = \%func;
    } else {
	warn("No function found\n");
    }
}

sub gensummary($) {
    my $page = shift;		# Which page to produce

    local *FILE;
    my $upage;
    my $func;
    my %xref;

    sysopen(FILE, "$page.3", O_RDWR|O_CREAT|O_TRUNC)
	or die("$page.3: $!\n");

    $upage = uc($page);
    print FILE "$COPYRIGHT
.Dd $TODAY
.Dt $upage 3
.Os
.Sh NAME
";
    my @funcs = sort(keys(%FUNCTIONS));
    while ($func = shift(@funcs)) {
	print FILE ".Nm $FUNCTIONS{$func}->{'Nm'}";
	print FILE " ,"
		if (@funcs);
	print FILE "\n";
    }
    print FILE ".Nd Pluggable Authentication Modules Library
.Sh LIBRARY
.Lb libpam
.Sh SYNOPSIS\n";
    if ($page eq 'pam') {
	print FILE ".In security/pam_appl.h\n";
    } else {
	print FILE ".In security/openpam.h\n";
    }
    foreach $func (sort(keys(%FUNCTIONS))) {
	print FILE ".Ft $FUNCTIONS{$func}->{'Ft'}\n";
	print FILE ".Fn $FUNCTIONS{$func}->{'Fn'}\n";
    }
    while (<STDIN>) {
	if (m/^\.Xr (\S+)\s*(\d)\s*$/) {
	    ++$xref{int($2)}->{$1};
	}
	print FILE $_;
    }

    if ($page eq 'pam') {
	print FILE ".Sh RETURN VALUES
The following return codes are defined by
.In security/pam_constants.h :
.Bl -tag -width 18n
";
	foreach (sort(keys(%PAMERR))) {
	    print FILE ".It Bq Er $_\n$PAMERR{$_}.\n";
	}
	print FILE ".El\n";
    }
    print FILE ".Sh SEE ALSO
";
    if ($page eq 'pam') {
	++$xref{3}->{'openpam'};
    }
    foreach $func (keys(%FUNCTIONS)) {
	++$xref{3}->{$func};
    }
    print FILE genxref(\%xref);
    print FILE ".Sh STANDARDS
.Rs
.%T \"X/Open Single Sign-On Service (XSSO) - Pluggable Authentication Modules\"
.%D \"June 1997\"
.Re
.Sh AUTHORS
The OpenPAM library and this manual page were developed for the
.Fx
Project by ThinkSec AS and Network Associates Laboratories, the
Security Research Division of Network Associates, Inc.\\& under
DARPA/SPAWAR contract N66001-01-C-8035
.Pq Dq CBOSS ,
as part of the DARPA CHATS research program.
";
    close(FILE);
}

sub usage() {

    print(STDERR "usage: gendoc [-s] source [...]\n");
    exit(1);
}

MAIN:{
    my %opts;

    usage()
	unless (@ARGV && getopts("op", \%opts));
    setlocale(LC_ALL, "en_US.ISO8859-1");
    $TODAY = strftime("%B %e, %Y", localtime(time()));
    $TODAY =~ s,\s+, ,g;
    if ($opts{'o'} || $opts{'p'}) {
	foreach my $fn (@ARGV) {
	    readproto($fn);
	}
	gensummary('openpam')
	    if ($opts{'o'});
	gensummary('pam')
	    if ($opts{'p'});
    } else {
	foreach my $fn (@ARGV) {
	    my $func = parse_source($fn);
	    gendoc($func)
		if (defined($func));
	}
    }
    exit(0);
}
