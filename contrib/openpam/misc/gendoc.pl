#!/usr/bin/perl -w
#-
# Copyright (c) 2002 Networks Associates Technologies, Inc.
# All rights reserved.
#
# This software was developed for the FreeBSD Project by ThinkSec AS and
# NAI Labs, the Security Research Division of Network Associates, Inc.
# under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
# DARPA CHATS research program.
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
# $Id$
#

use strict;
use Fcntl;
use POSIX qw(strftime);
use vars qw($COPYRIGHT $TODAY %FUNCTIONS %PAMERR);

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
    PAM_ACCT_EXPIRED		=> "User accound has expired",
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
	return;
    }

    sysopen(FILE, $fn, O_RDONLY)
	or die("$fn: open(): $!\n");
    $source = join('', <FILE>);
    close(FILE);

    return if ($source =~ m/^ \* NOPARSE\s*$/m);

    if (!defined($COPYRIGHT) && $source =~ m,^(/\*-\n.*?)\s*\*/,s) {
	$COPYRIGHT = $1;
	$COPYRIGHT =~ s,^.\*,.\\\",gm;
	$COPYRIGHT =~ s,(\$Id).*?\$,$1\$,;
	$COPYRIGHT .= "\n.\\\"";
    }
    $func = $fn;
    $func =~ s,^(?:.*/)?([^/]+)\.c$,$1,;
    if ($source !~ m,\n \* ([\S ]+)\n \*/\n\n([\S ]+)\n$func\((.*?)\)\n\{,s) {
	warn("$fn: can't find $func\n");
	return;
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
	"pam 3" => 1
    );

    if ($type eq "int") {
	foreach (split("\n", $source)) {
	    next unless (m/^ \*\s+(!?PAM_[A-Z_]+|=[a-z_]+)\s*$/);
	    push(@errors, $1);
	}
	$xref{"pam_strerror 3"} = 1;
    }

    $argnames = $args;
    $argnames =~ s/\"[^\"]+\*?\b(\w+)\"/\"$1\"/g;
    $argnames =~ s/([\|\[\]\(\)\.\*\+\?])/\\$1/g;
    $argnames =~ s/\" \"/|/g;
    $argnames =~ s/^\"(.*)\"$/($1)/;
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
		    $man .= ".El\n";
		    $inlist = 0;
		} else {
		    $man .= ".Pp\n";
		}
	    }
	    next;
	}
	if (m/^>(\w+)(?:\s+(\d))?$/) {
	    ++$xref{$2 ? "$1 $2" : "$1 3"};
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
	    $man .= ".El\n";
	    $inlist = 0;
	} elsif ($inliteral && m/^\S/) {
	    $man .= ".Ed\n";
	    $inlist = 0;
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
	s/\s*=$argnames\b\s*/\n.Va $1\n/gs;
	s/\s*=(struct \w+(?: \*)?)\b\s*/\n.Vt $1\n/gs;
	if (s/\s*=([a-z_]+)\b\s*/\n.Xr $1 3\n/gs) {
	    ++$xref{"$1 3"};
	}
	s/\s*\"(?=\w)/\n.Do\n/gs;
	s/\"(?!\w)\s*/\n.Dc\n/gs;
	s/\s*=([A-Z][A-Z_]+)\b\s*(?![\.,:;])/\n.Dv $1\n/gs;
	s/\s*=([A-Z][A-Z_]+)\b([\.,:;]+)\s*/\n.Dv $1 $2\n/gs;
	s/\s*{([A-Z][a-z] .*?)}\s*/\n.$1\n/gs;
	$man .= "$_\n";
    }
    if (defined($man)) {
	$man =~ s/(\n\.[A-Z][a-z] [\w ]+)\n([\.,:;-]\S*)\s*/$1 $2\n/gs;
	$man =~ s/\s*$/\n/gm;
	$man =~ s/\n+/\n/gs;
	$man =~ s/\0//gs;
	chomp($man);
    } else {
	$man = "No description available.";
    }

    $FUNCTIONS{$func} = {
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
	$FUNCTIONS{$func}->{'nolist'} = 1;
    }
    if ($source =~ m/^ \* NOLIST\s*$/m) {
	$FUNCTIONS{$func}->{'nolist'} = 1;
    }
    if ($source !~ m/^ \* XSSO \d/m) {
	$FUNCTIONS{$func}->{'openpam'} = 1;
    }
}

sub expand_errors($);
sub expand_errors($) {
    my $func = shift;		# Ref to function hash

    my %errors;

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
	    if (!defined($FUNCTIONS{$1})) {
		warn("$func->{'name'}(): reference to unknown $1()\n");
		next;
	    }
	    foreach (expand_errors($FUNCTIONS{$1})) {
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
    return (sort(keys(%errors)));
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
.In security/pam_appl.h
";
    if ($func->{'name'} =~ m/_sm_/) {
	$mdoc .= ".In security/pam_modules.h\n"
    }
    $mdoc .= ".Ft $func->{'type'}
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
	my @errors = expand_errors($func);
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
    $mdoc .= ".Sh SEE ALSO\n";
    my @xref = sort(keys(%{$func->{'xref'}}));
    while (@xref) {
	$mdoc .= ".Xr " . shift(@xref) . (@xref ? " ,\n" : "\n");
    }
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
function and this manual page were developed for the FreeBSD Project
by ThinkSec AS and NAI Labs, the Security Research Division of Network
Associates, Inc.  under DARPA/SPAWAR contract N66001-01-C-8035
.Pq Dq CBOSS ,
as part of the DARPA CHATS research program.
";

    $fn = "$func->{'name'}.3";
    sysopen(FILE, $fn, O_RDWR|O_CREAT|O_TRUNC)
	or die("$fn: open(): $!\n");
    print(FILE $mdoc);
    close(FILE);
}

sub gensummary() {

    my $func;

    print "$COPYRIGHT
.Dd $TODAY
.Dt PAM 3
.Os
.Sh NAME
";
    my @funcs = sort(keys(%FUNCTIONS));
    while ($func = shift(@funcs)) {
	next if (defined($FUNCTIONS{$func}->{'nolist'}));
	print ".Nm $func". (@funcs ? " ,\n" : "\n");
    }
    print ".Nd Pluggable Authentication Modules Library
.Sh LIBRARY
.Lb libpam
.Sh SYNOPSIS
.In security/pam_appl.h
";
    foreach $func (sort(keys(%FUNCTIONS))) {
	next if (defined($FUNCTIONS{$func}->{'nolist'}));
	print ".Ft $FUNCTIONS{$func}->{'type'}\n";
	print ".Fn $func $FUNCTIONS{$func}->{'args'}\n";
    }
    print ".Sh DESCRIPTION
.Sh RETURN VALUES
The following return codes are defined in the
.In security/pam_constants.h
header:
.Bl -tag -width 18n
";
    foreach (sort(keys(%PAMERR))) {
	print ".It Bq Er $_\n$PAMERR{$_}.\n";
    }
    print ".El
.Sh SEE ALSO
";
    foreach $func (sort(keys(%FUNCTIONS))) {
	next if (defined($FUNCTIONS{$func}->{'nolist'}));
	print ".Xr $func 3 ,\n";
    }
    print ".Xr pam.conf 5
.Sh STANDARDS
.Rs
.%T \"X/Open Single Sign-On Service (XSSO) - Pluggable Authentication Modules\"
.%D \"June 1997\"
.Re
.Sh AUTHORS
The OpenPAM library and this manual page were developed for the
FreeBSD Project by ThinkSec AS and NAI Labs, the Security Research
Division of Network Associates, Inc.  under DARPA/SPAWAR contract
N66001-01-C-8035
.Pq Dq CBOSS ,
as part of the DARPA CHATS research program.
"
}

MAIN:{
    $TODAY = strftime("%B %e, %Y", localtime(time()));
    $TODAY =~ s,\s+, ,g;
    foreach my $fn (@ARGV) {
	parse_source($fn);
    }
    foreach my $func (values(%FUNCTIONS)) {
	gendoc($func);
    }
    gensummary();
}
