package CPAN::Mirrored::By;

sub new { 
    my($self,@arg) = @_;
    bless [@arg], $self;
}
sub continent { shift->[0] }
sub country { shift->[1] }
sub url { shift->[2] }

package CPAN::FirstTime;

use strict;
use ExtUtils::MakeMaker qw(prompt);
use FileHandle ();
use File::Basename ();
use File::Path ();
use vars qw($VERSION);
$VERSION = substr q$Revision: 1.36 $, 10;

=head1 NAME

CPAN::FirstTime - Utility for CPAN::Config file Initialization

=head1 SYNOPSIS

CPAN::FirstTime::init()

=head1 DESCRIPTION

The init routine asks a few questions and writes a CPAN::Config
file. Nothing special.

=cut


sub init {
    my($configpm) = @_;
    use Config;
    unless ($CPAN::VERSION) {
	require CPAN::Nox;
    }
    eval {require CPAN::Config;};
    $CPAN::Config ||= {};
    local($/) = "\n";
    local($\) = "";
    local($|) = 1;

    my($ans,$default,$local,$cont,$url,$expected_size);

    #
    # Files, directories
    #

    print qq[

CPAN is the world-wide archive of perl resources. It consists of about
100 sites that all replicate the same contents all around the globe.
Many countries have at least one CPAN site already. The resources
found on CPAN are easily accessible with the CPAN.pm module. If you
want to use CPAN.pm, you have to configure it properly.

If you do not want to enter a dialog now, you can answer 'no' to this
question and I\'ll try to autoconfigure. (Note: you can revisit this
dialog anytime later by typing 'o conf init' at the cpan prompt.)

];

    my $manual_conf =
	ExtUtils::MakeMaker::prompt("Are you ready for manual configuration?",
				    "yes");
    my $fastread;
    {
      local $^W;
      if ($manual_conf =~ /^\s*y/i) {
	$fastread = 0;
	*prompt = \&ExtUtils::MakeMaker::prompt;
      } else {
	$fastread = 1;
	*prompt = sub {
	  my($q,$a) = @_;
	  my($ret) = defined $a ? $a : "";
	  printf qq{%s [%s]\n\n}, $q, $ret;
	  $ret;
	};
      }
    }
    print qq{

The following questions are intended to help you with the
configuration. The CPAN module needs a directory of its own to cache
important index files and maybe keep a temporary mirror of CPAN files.
This may be a site-wide directory or a personal directory.

};

    my $cpan_home = $CPAN::Config->{cpan_home} || MM->catdir($ENV{HOME}, ".cpan");
    if (-d $cpan_home) {
	print qq{

I see you already have a  directory
    $cpan_home
Shall we use it as the general CPAN build and cache directory?

};
    } else {
	print qq{

First of all, I\'d like to create this directory. Where?

};
    }

    $default = $cpan_home;
    while ($ans = prompt("CPAN build and cache directory?",$default)) {
      eval { File::Path::mkpath($ans); }; # dies if it can't
      if ($@) {
	warn "Couldn't create directory $ans.
Please retry.\n";
	next;
      }
      if (-d $ans && -w _) {
	last;
      } else {
	warn "Couldn't find directory $ans
  or directory is not writable. Please retry.\n";
      }
    }
    $CPAN::Config->{cpan_home} = $ans;

    print qq{

If you want, I can keep the source files after a build in the cpan
home directory. If you choose so then future builds will take the
files from there. If you don\'t want to keep them, answer 0 to the
next question.

};

    $CPAN::Config->{keep_source_where} = MM->catdir($CPAN::Config->{cpan_home},"sources");
    $CPAN::Config->{build_dir} = MM->catdir($CPAN::Config->{cpan_home},"build");

    #
    # Cache size, Index expire
    #

    print qq{

How big should the disk cache be for keeping the build directories
with all the intermediate files?

};

    $default = $CPAN::Config->{build_cache} || 10;
    $ans = prompt("Cache size for build directory (in MB)?", $default);
    $CPAN::Config->{build_cache} = $ans;

    # XXX This the time when we refetch the index files (in days)
    $CPAN::Config->{'index_expire'} = 1;

    print qq{

By default, each time the CPAN module is started, cache scanning
is performed to keep the cache size in sync. To prevent from this,
disable the cache scanning with 'never'.

};

    $default = $CPAN::Config->{scan_cache} || 'atstart';
    do {
        $ans = prompt("Perform cache scanning (atstart or never)?", $default);
    } while ($ans ne 'atstart' && $ans ne 'never');
    $CPAN::Config->{scan_cache} = $ans;

    #
    # prerequisites_policy
    # Do we follow PREREQ_PM?
    #
    print qq{

The CPAN module can detect when a module that which you are trying to
build depends on prerequisites. If this happens, it can build the
prerequisites for you automatically ('follow'), ask you for
confirmation ('ask'), or just ignore them ('ignore'). Please set your
policy to one of the three values.

};

    $default = $CPAN::Config->{prerequisites_policy} || 'follow';
    do {
      $ans =
	  prompt("Policy on building prerequisites (follow, ask or ignore)?",
		 $default);
    } while ($ans ne 'follow' && $ans ne 'ask' && $ans ne 'ignore');
    $CPAN::Config->{prerequisites_policy} = $ans;

    #
    # External programs
    #

    print qq{

The CPAN module will need a few external programs to work
properly. Please correct me, if I guess the wrong path for a program.
Don\'t panic if you do not have some of them, just press ENTER for
those.

};

    my $old_warn = $^W;
    local $^W if $^O eq 'MacOS';
    my(@path) = split /$Config{'path_sep'}/, $ENV{'PATH'};
    local $^W = $old_warn;
    my $progname;
    for $progname (qw/gzip tar unzip make lynx ncftpget ncftp ftp/){
      if ($^O eq 'MacOS') {
          $CPAN::Config->{$progname} = 'not_here';
          next;
      }
      my $progcall = $progname;
      # we don't need ncftp if we have ncftpget
      next if $progname eq "ncftp" && $CPAN::Config->{ncftpget} gt " ";
      my $path = $CPAN::Config->{$progname} 
	  || $Config::Config{$progname}
	      || "";
      if (MM->file_name_is_absolute($path)) {
	# testing existence is not good enough, some have these exe
	# extensions

	# warn "Warning: configured $path does not exist\n" unless -e $path;
	# $path = "";
      } else {
	$path = '';
      }
      unless ($path) {
	# e.g. make -> nmake
	$progcall = $Config::Config{$progname} if $Config::Config{$progname};
      }

      $path ||= find_exe($progcall,[@path]);
      warn "Warning: $progcall not found in PATH\n" unless
	  $path; # not -e $path, because find_exe already checked that
      $ans = prompt("Where is your $progname program?",$path) || $path;
      $CPAN::Config->{$progname} = $ans;
    }
    my $path = $CPAN::Config->{'pager'} || 
	$ENV{PAGER} || find_exe("less",[@path]) || 
	    find_exe("more",[@path]) || ($^O eq 'MacOS' ? $ENV{EDITOR} : 0 )
	    || "more";
    $ans = prompt("What is your favorite pager program?",$path);
    $CPAN::Config->{'pager'} = $ans;
    $path = $CPAN::Config->{'shell'};
    if (MM->file_name_is_absolute($path)) {
	warn "Warning: configured $path does not exist\n" unless -e $path;
	$path = "";
    }
    $path ||= $ENV{SHELL};
    if ($^O eq 'MacOS') {
        $CPAN::Config->{'shell'} = 'not_here';
    } else {
        $path =~ s,\\,/,g if $^O eq 'os2';	# Cosmetic only
        $ans = prompt("What is your favorite shell?",$path);
        $CPAN::Config->{'shell'} = $ans;
    }

    #
    # Arguments to make etc.
    #

    print qq{

Every Makefile.PL is run by perl in a separate process. Likewise we
run \'make\' and \'make install\' in processes. If you have any parameters
\(e.g. PREFIX, INSTALLPRIVLIB, UNINST or the like\) you want to pass to
the calls, please specify them here.

If you don\'t understand this question, just press ENTER.

};

    $default = $CPAN::Config->{makepl_arg} || "";
    $CPAN::Config->{makepl_arg} =
	prompt("Parameters for the 'perl Makefile.PL' command?",$default);
    $default = $CPAN::Config->{make_arg} || "";
    $CPAN::Config->{make_arg} = prompt("Parameters for the 'make' command?",$default);

    $default = $CPAN::Config->{make_install_arg} || $CPAN::Config->{make_arg} || "";
    $CPAN::Config->{make_install_arg} =
	prompt("Parameters for the 'make install' command?",$default);

    #
    # Alarm period
    #

    print qq{

Sometimes you may wish to leave the processes run by CPAN alone
without caring about them. As sometimes the Makefile.PL contains
question you\'re expected to answer, you can set a timer that will
kill a 'perl Makefile.PL' process after the specified time in seconds.

If you set this value to 0, these processes will wait forever. This is
the default and recommended setting.

};

    $default = $CPAN::Config->{inactivity_timeout} || 0;
    $CPAN::Config->{inactivity_timeout} =
	prompt("Timeout for inactivity during Makefile.PL?",$default);

    # Proxies

    print qq{

If you\'re accessing the net via proxies, you can specify them in the
CPAN configuration or via environment variables. The variable in
the \$CPAN::Config takes precedence.

};

    for (qw/ftp_proxy http_proxy no_proxy/) {
	$default = $CPAN::Config->{$_} || $ENV{$_};
	$CPAN::Config->{$_} = prompt("Your $_?",$default);
    }

    #
    # MIRRORED.BY
    #

    conf_sites() unless $fastread;

    unless (@{$CPAN::Config->{'wait_list'}||[]}) {
	print qq{

WAIT support is available as a Plugin. You need the CPAN::WAIT module
to actually use it.  But we need to know your favorite WAIT server. If
you don\'t know a WAIT server near you, just press ENTER.

};
	$default = "wait://ls6.informatik.uni-dortmund.de:1404";
	$ans = prompt("Your favorite WAIT server?\n  ",$default);
	push @{$CPAN::Config->{'wait_list'}}, $ans;
    }

    # We don't ask that now, it will be noticed in time, won't it?
    $CPAN::Config->{'inhibit_startup_message'} = 0;
    $CPAN::Config->{'getcwd'} = 'cwd';

    print "\n\n";
    CPAN::Config->commit($configpm);
}

sub conf_sites {
  my $m = 'MIRRORED.BY';
  my $mby = MM->catfile($CPAN::Config->{keep_source_where},$m);
  File::Path::mkpath(File::Basename::dirname($mby));
  if (-f $mby && -f $m && -M $m < -M $mby) {
    require File::Copy;
    File::Copy::copy($m,$mby) or die "Could not update $mby: $!";
  }
  if ( ! -f $mby ){
    print qq{You have no $mby
  I\'m trying to fetch one
};
    $mby = CPAN::FTP->localize($m,$mby,3);
  } elsif (-M $mby > 30 ) {
    print qq{Your $mby is older than 30 days,
  I\'m trying to fetch one
};
    $mby = CPAN::FTP->localize($m,$mby,3);
  }
  read_mirrored_by($mby);
}

sub find_exe {
    my($exe,$path) = @_;
    my($dir);
    #warn "in find_exe exe[$exe] path[@$path]";
    for $dir (@$path) {
	my $abs = MM->catfile($dir,$exe);
	if (($abs = MM->maybe_command($abs))) {
	    return $abs;
	}
    }
}

sub picklist {
    my($items,$prompt,$default,$require_nonempty,$empty_warning)=@_;
    $default ||= '';

    my ($item, $i);
    for $item (@$items) {
	printf "(%d) %s\n", ++$i, $item;
    }

    my @nums;
    while (1) {
	my $num = prompt($prompt,$default);
	@nums = split (' ', $num);
	(warn "invalid items entered, try again\n"), next
	    if grep (/\D/ || $_ < 1 || $_ > $i, @nums);
	if ($require_nonempty) {
	    (warn "$empty_warning\n"), next
		unless @nums;
	}
	last;
    }
    print "\n";
    for (@nums) { $_-- }
    @{$items}[@nums];
}

sub read_mirrored_by {
    my($local) = @_;
    my(%all,$url,$expected_size,$default,$ans,$host,$dst,$country,$continent,@location);
    my $fh = FileHandle->new;
    $fh->open($local) or die "Couldn't open $local: $!";
    local $/ = "\012";
    while (<$fh>) {
	($host) = /^([\w\.\-]+)/ unless defined $host;
	next unless defined $host;
	next unless /\s+dst_(dst|location)/;
	/location\s+=\s+\"([^\"]+)/ and @location = (split /\s*,\s*/, $1) and
	    ($continent, $country) = @location[-1,-2];
	$continent =~ s/\s\(.*//;
	$continent =~ s/\W+$//; # if Jarkko doesn't know latitude/longitude
	/dst_dst\s+=\s+\"([^\"]+)/  and $dst = $1;
	next unless $host && $dst && $continent && $country;
	$all{$continent}{$country}{$dst} = CPAN::Mirrored::By->new($continent,$country,$dst);
	undef $host;
	$dst=$continent=$country="";
    }
    $fh->close;
    $CPAN::Config->{urllist} ||= [];
    my(@previous_urls);
    if (@previous_urls = @{$CPAN::Config->{urllist}}) {
	$CPAN::Config->{urllist} = [];
    }

    print qq{

Now we need to know where your favorite CPAN sites are located. Push
a few sites onto the array (just in case the first on the array won\'t
work). If you are mirroring CPAN to your local workstation, specify a
file: URL.

First, pick a nearby continent and country (you can pick several of
each, separated by spaces, or none if you just want to keep your
existing selections). Then, you will be presented with a list of URLs
of CPAN mirrors in the countries you selected, along with previously
selected URLs. Select some of those URLs, or just keep the old list.
Finally, you will be prompted for any extra URLs -- file:, ftp:, or
http: -- that host a CPAN mirror.

};

    my (@cont, $cont, %cont, @countries, @urls, %seen);
    my $no_previous_warn = 
       "Sorry! since you don't have any existing picks, you must make a\n" .
       "geographic selection.";
    @cont = picklist([sort keys %all],
                     "Select your continent (or several nearby continents)",
                     '',
                     ! @previous_urls,
                     $no_previous_warn);


    foreach $cont (@cont) {
        my @c = sort keys %{$all{$cont}};
        @cont{@c} = map ($cont, 0..$#c);
        @c = map ("$_ ($cont)", @c) if @cont > 1;
        push (@countries, @c);
    }

    if (@countries) {
        @countries = picklist (\@countries,
                               "Select your country (or several nearby countries)",
                               '',
                               ! @previous_urls,
                               $no_previous_warn);
        %seen = map (($_ => 1), @previous_urls);
        # hmmm, should take list of defaults from CPAN::Config->{'urllist'}...
        foreach $country (@countries) {
            (my $bare_country = $country) =~ s/ \(.*\)//;
            my @u = sort keys %{$all{$cont{$bare_country}}{$bare_country}};
            @u = grep (! $seen{$_}, @u);
            @u = map ("$_ ($bare_country)", @u)
               if @countries > 1;
            push (@urls, @u);
        }
    }
    push (@urls, map ("$_ (previous pick)", @previous_urls));
    my $prompt = "Select as many URLs as you like";
    if (@previous_urls) {
       $default = join (' ', ((scalar @urls) - (scalar @previous_urls) + 1) ..
                             (scalar @urls));
       $prompt .= "\n(or just hit RETURN to keep your previous picks)";
    }

    @urls = picklist (\@urls, $prompt, $default);
    foreach (@urls) { s/ \(.*\)//; }
    %seen = map (($_ => 1), @urls);

    do {
        $ans = prompt ("Enter another URL or RETURN to quit:", "");

        if ($ans) {
            $ans =~ s|/?$|/|; # has to end with one slash
            $ans = "file:$ans" unless $ans =~ /:/; # without a scheme is a file:
            if ($ans =~ /^\w+:\/./) {
               push @urls, $ans 
                  unless $seen{$ans};
            }
            else {
                print qq{"$ans" doesn\'t look like an URL at first sight.
I\'ll ignore it for now.  You can add it to $INC{'CPAN/MyConfig.pm'}
later if you\'re sure it\'s right.\n};
            }
        }
    } while $ans;

    push @{$CPAN::Config->{urllist}}, @urls;
    # xxx delete or comment these out when you're happy that it works
    print "New set of picks:\n";
    map { print "  $_\n" } @{$CPAN::Config->{urllist}};
}

1;
