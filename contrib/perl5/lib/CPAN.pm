package CPAN;
use vars qw{$Try_autoload
            $Revision
	    $META $Signal $Cwd $End
	    $Suppress_readline %Dontload
	    $Frontend  $Defaultsite
	   }; #};

$VERSION = '1.48';

# $Id: CPAN.pm,v 1.260 1999/03/06 19:31:02 k Exp $

# only used during development:
$Revision = "";
# $Revision = "[".substr(q$Revision: 1.260 $, 10)."]";

use Carp ();
use Config ();
use Cwd ();
use DirHandle;
use Exporter ();
use ExtUtils::MakeMaker (); # $SelfLoader::DEBUG=1;
use File::Basename ();
use File::Copy ();
use File::Find;
use File::Path ();
use FileHandle ();
use Safe ();
use Text::ParseWords ();
use Text::Wrap;
use File::Spec;

END { $End++; &cleanup; }

%CPAN::DEBUG = qw[
		  CPAN              1
		  Index             2
		  InfoObj           4
		  Author            8
		  Distribution     16
		  Bundle           32
		  Module           64
		  CacheMgr        128
		  Complete        256
		  FTP             512
		  Shell          1024
		  Eval           2048
		  Config         4096
		  Tarzip         8192
];

$CPAN::DEBUG ||= 0;
$CPAN::Signal ||= 0;
$CPAN::Frontend ||= "CPAN::Shell";
$CPAN::Defaultsite ||= "ftp://ftp.perl.org/pub/CPAN";

package CPAN;
use vars qw($VERSION @EXPORT $AUTOLOAD $DEBUG $META $term);
use strict qw(vars);

@CPAN::ISA = qw(CPAN::Debug Exporter);

@EXPORT = qw(
	     autobundle bundle expand force get
	     install make readme recompile shell test clean
	    );

#-> sub CPAN::AUTOLOAD ;
sub AUTOLOAD {
    my($l) = $AUTOLOAD;
    $l =~ s/.*:://;
    my(%EXPORT);
    @EXPORT{@EXPORT} = '';
    CPAN::Config->load unless $CPAN::Config_loaded++;
    if (exists $EXPORT{$l}){
	CPAN::Shell->$l(@_);
    } else {
	my $ok = CPAN::Shell->try_dot_al($AUTOLOAD);
	if ($ok) {
	    goto &$AUTOLOAD;
#	} else {
#	    $CPAN::Frontend->mywarn("Could not autoload $AUTOLOAD");
	}
	$CPAN::Frontend->mywarn(qq{Unknown command "$AUTOLOAD". }.
				qq{Type ? for help.
});
    }
}

#-> sub CPAN::shell ;
sub shell {
    my($self) = @_;
    $Suppress_readline ||= ! -t STDIN;
    CPAN::Config->load unless $CPAN::Config_loaded++;

    my $prompt = "cpan> ";
    local($^W) = 1;
    unless ($Suppress_readline) {
	require Term::ReadLine;
#	import Term::ReadLine;
	$term = Term::ReadLine->new('CPAN Monitor');
	if ($term->ReadLine eq "Term::ReadLine::Gnu") {
	    my $attribs = $term->Attribs;
#	     $attribs->{completion_entry_function} =
#		 $attribs->{'list_completion_function'};
	     $attribs->{attempted_completion_function} = sub {
		 &CPAN::Complete::gnu_cpl;
	     }
#	    $attribs->{completion_word} =
#		[qw(help me somebody to find out how
#                    to use completion with GNU)];
	} else {
	    $readline::rl_completion_function =
		$readline::rl_completion_function = 'CPAN::Complete::cpl';
	}
    }

    no strict;
    $META->checklock();
    my $getcwd;
    $getcwd = $CPAN::Config->{'getcwd'} || 'cwd';
    my $cwd = CPAN->$getcwd();
    my $try_detect_readline = $term->ReadLine eq "Term::ReadLine::Stub";
    my $rl_avail = $Suppress_readline ? "suppressed" :
	($term->ReadLine ne "Term::ReadLine::Stub") ? "enabled" :
	    "available (try ``install Bundle::CPAN'')";

    $CPAN::Frontend->myprint(
			     qq{
cpan shell -- CPAN exploration and modules installation (v$CPAN::VERSION$CPAN::Revision)
ReadLine support $rl_avail

}) unless $CPAN::Config->{'inhibit_startup_message'} ;
    my($continuation) = "";
    while () {
	if ($Suppress_readline) {
	    print $prompt;
	    last unless defined ($_ = <> );
	    chomp;
	} else {
	    last unless defined ($_ = $term->readline($prompt));
	}
	$_ = "$continuation$_" if $continuation;
	s/^\s+//;
	next if /^$/;
	$_ = 'h' if /^\s*\?/;
	if (/^(?:q(?:uit)?|bye|exit)$/i) {
	    last;
	} elsif (s/\\$//s) {
	    chomp;
	    $continuation = $_;
	    $prompt = "    > ";
	} elsif (/^\!/) {
	    s/^\!//;
	    my($eval) = $_;
	    package CPAN::Eval;
	    use vars qw($import_done);
	    CPAN->import(':DEFAULT') unless $import_done++;
	    CPAN->debug("eval[$eval]") if $CPAN::DEBUG;
	    eval($eval);
	    warn $@ if $@;
	    $continuation = "";
	    $prompt = "cpan> ";
	} elsif (/./) {
	    my(@line);
	    if ($] < 5.00322) { # parsewords had a bug until recently
		@line = split;
	    } else {
		eval { @line = Text::ParseWords::shellwords($_) };
		warn($@), next if $@;
	    }
	    $CPAN::META->debug("line[".join("|",@line)."]") if $CPAN::DEBUG;
	    my $command = shift @line;
	    eval { CPAN::Shell->$command(@line) };
	    warn $@ if $@;
	    chdir $cwd;
	    $CPAN::Frontend->myprint("\n");
	    $continuation = "";
	    $prompt = "cpan> ";
	}
    } continue {
      $Signal=0;
      CPAN::Queue->nullify_queue;
      if ($try_detect_readline) {
	if ($CPAN::META->has_inst("Term::ReadLine::Gnu")
	    ||
	    $CPAN::META->has_inst("Term::ReadLine::Perl")
	   ) {
	    delete $INC{"Term/ReadLine.pm"};
	    my $redef;
	    local($SIG{__WARN__}) = CPAN::Shell::dotdot_onreload(\$redef);
	    require Term::ReadLine;
	    $CPAN::Frontend->myprint("\n$redef subroutines in Term::ReadLine redefined\n");
	    goto &shell;
	}
      }
    }
}

package CPAN::CacheMgr;
@CPAN::CacheMgr::ISA = qw(CPAN::InfoObj CPAN);
use File::Find;

package CPAN::Config;
import ExtUtils::MakeMaker 'neatvalue';
use vars qw(%can $dot_cpan);

%can = (
  'commit' => "Commit changes to disk",
  'defaults' => "Reload defaults from disk",
  'init'   => "Interactive setting of all options",
);

package CPAN::FTP;
use vars qw($Ua $Thesite $Themethod);
@CPAN::FTP::ISA = qw(CPAN::Debug);

package CPAN::Complete;
@CPAN::Complete::ISA = qw(CPAN::Debug);

package CPAN::Index;
use vars qw($last_time $date_of_03);
@CPAN::Index::ISA = qw(CPAN::Debug);
$last_time ||= 0;
$date_of_03 ||= 0;

package CPAN::InfoObj;
@CPAN::InfoObj::ISA = qw(CPAN::Debug);

package CPAN::Author;
@CPAN::Author::ISA = qw(CPAN::InfoObj);

package CPAN::Distribution;
@CPAN::Distribution::ISA = qw(CPAN::InfoObj);

package CPAN::Bundle;
@CPAN::Bundle::ISA = qw(CPAN::Module);

package CPAN::Module;
@CPAN::Module::ISA = qw(CPAN::InfoObj);

package CPAN::Shell;
use vars qw($AUTOLOAD $redef @ISA);
@CPAN::Shell::ISA = qw(CPAN::Debug);

#-> sub CPAN::Shell::AUTOLOAD ;
sub AUTOLOAD {
    my($autoload) = $AUTOLOAD;
    my $class = shift(@_);
    # warn "autoload[$autoload] class[$class]";
    $autoload =~ s/.*:://;
    if ($autoload =~ /^w/) {
	if ($CPAN::META->has_inst('CPAN::WAIT')) {
	    CPAN::WAIT->$autoload(@_);
	} else {
	    $CPAN::Frontend->mywarn(qq{
Commands starting with "w" require CPAN::WAIT to be installed.
Please consider installing CPAN::WAIT to use the fulltext index.
For this you just need to type
    install CPAN::WAIT
});
	}
    } else {
	my $ok = CPAN::Shell->try_dot_al($AUTOLOAD);
	if ($ok) {
	    goto &$AUTOLOAD;
#	} else {
#	    $CPAN::Frontend->mywarn("Could not autoload $autoload");
	}
	$CPAN::Frontend->mywarn(qq{Unknown command '$autoload'. }.
				qq{Type ? for help.
});
    }
}

#-> CPAN::Shell::try_dot_al
sub try_dot_al {
    my($class,$autoload) = @_;
    return unless $CPAN::Try_autoload;
    # I don't see how to re-use that from the AutoLoader...
    my($name,$ok);
    # Braces used to preserve $1 et al.
    {
	my ($pkg,$func) = $autoload =~ /(.*)::([^:]+)$/;
	$pkg =~ s|::|/|g;
	if (defined($name=$INC{"$pkg.pm"}))
	    {
		$name =~ s|^(.*)$pkg\.pm$|$1auto/$pkg/$func.al|;
		$name = undef unless (-r $name);
	    }
	unless (defined $name)
	    {
		$name = "auto/$autoload.al";
		$name =~ s|::|/|g;
	    }
    }
    my $save = $@;
    eval {local $SIG{__DIE__};require $name};
    if ($@) {
	if (substr($autoload,-9) eq '::DESTROY') {
	    *$autoload = sub {};
	    $ok = 1;
	} else {
	    if ($name =~ s{(\w{12,})\.al$}{substr($1,0,11).".al"}e){
		eval {local $SIG{__DIE__};require $name};
	    }
	    if ($@){
		$@ =~ s/ at .*\n//;
		Carp::croak $@;
	    } else {
		$ok = 1;
	    }
	}
    } else {

      $ok = 1;

    }
    $@ = $save;
#    my $lm = Carp::longmess();
#    warn "ok[$ok] autoload[$autoload] longmess[$lm]"; # debug
    return $ok;
}

#### autoloader is experimental
#### to try it we have to set $Try_autoload and uncomment
#### the use statement and uncomment the __END__ below
#### You also need AutoSplit 1.01 available. MakeMaker will
#### then build CPAN with all the AutoLoad stuff.
# use AutoLoader;
# $Try_autoload = 1;

if ($CPAN::Try_autoload) {
  my $p;
    for $p (qw(
	       CPAN::Author CPAN::Bundle CPAN::CacheMgr CPAN::Complete
	       CPAN::Config CPAN::Debug CPAN::Distribution CPAN::FTP
	       CPAN::FTP::netrc CPAN::Index CPAN::InfoObj CPAN::Module
		 )) {
	*{"$p\::AUTOLOAD"} = \&AutoLoader::AUTOLOAD;
    }
}

package CPAN::Tarzip;
use vars qw($AUTOLOAD @ISA);
@CPAN::Tarzip::ISA = qw(CPAN::Debug);

package CPAN::Queue;

# One use of the queue is to determine if we should or shouldn't
# announce the availability of a new CPAN module

# Now we try to use it for dependency tracking. For that to happen
# we need to draw a dependency tree and do the leaves first. This can
# easily be reached by running CPAN.pm recursively, but we don't want
# to waste memory and run into deep recursion. So what we can do is
# this:

# CPAN::Queue is the package where the queue is maintained. Dependencies
# often have high priority and must be brought to the head of the queue,
# possibly by jumping the queue if they are already there. My first code
# attempt tried to be extremely correct. Whenever a module needed
# immediate treatment, I either unshifted it to the front of the queue,
# or, if it was already in the queue, I spliced and let it bypass the
# others. This became a too correct model that made it impossible to put
# an item more than once into the queue. Why would you need that? Well,
# you need temporary duplicates as the manager of the queue is a loop
# that
#
#  (1) looks at the first item in the queue without shifting it off
#
#  (2) cares for the item
#
#  (3) removes the item from the queue, *even if its agenda failed and
#      even if the item isn't the first in the queue anymore* (that way
#      protecting against never ending queues)
#
# So if an item has prerequisites, the installation fails now, but we
# want to retry later. That's easy if we have it twice in the queue.
#
# I also expect insane dependency situations where an item gets more
# than two lives in the queue. Simplest example is triggered by 'install
# Foo Foo Foo'. People make this kind of mistakes and I don't want to
# get in the way. I wanted the queue manager to be a dumb servant, not
# one that knows everything.
#
# Who would I tell in this model that the user wants to be asked before
# processing? I can't attach that information to the module object,
# because not modules are installed but distributions. So I'd have to
# tell the distribution object that it should ask the user before
# processing. Where would the question be triggered then? Most probably
# in CPAN::Distribution::rematein.
# Hope that makes sense, my head is a bit off:-) -- AK

use vars qw{ @All };

sub new {
  my($class,$mod) = @_;
  my $self = bless {mod => $mod}, $class;
  push @All, $self;
  # my @all = map { $_->{mod} } @All;
  # warn "Adding Queue object for mod[$mod] all[@all]";
  return $self;
}

sub first {
  my $obj = $All[0];
  $obj->{mod};
}

sub delete_first {
  my($class,$what) = @_;
  my $i;
  for my $i (0..$#All) {
    if (  $All[$i]->{mod} eq $what ) {
      splice @All, $i, 1;
      return;
    }
  }
}

sub jumpqueue {
  my $class = shift;
  my @what = @_;
  my $obj;
  WHAT: for my $what (reverse @what) {
    my $jumped = 0;
    for (my $i=0; $i<$#All;$i++) { #prevent deep recursion
      if ($All[$i]->{mod} eq $what){
	$jumped++;
	if ($jumped > 100) { # one's OK if e.g. just processing now;
                             # more are OK if user typed it several
                             # times
	  $CPAN::Frontend->mywarn(
qq{Object [$what] queued more than 100 times, ignoring}
				 );
	  next WHAT;
	}
      }
    }
    my $obj = bless { mod => $what }, $class;
    unshift @All, $obj;
  }
}

sub exists {
  my($self,$what) = @_;
  my @all = map { $_->{mod} } @All;
  my $exists = grep { $_->{mod} eq $what } @All;
  # warn "Checking exists in Queue object for mod[$what] all[@all] exists[$exists]";
  $exists;
}

sub delete {
  my($self,$mod) = @_;
  @All = grep { $_->{mod} ne $mod } @All;
  # my @all = map { $_->{mod} } @All;
  # warn "Deleting Queue object for mod[$mod] all[@all]";
}

sub nullify_queue {
  @All = ();
}



package CPAN;

$META ||= CPAN->new; # In case we re-eval ourselves we need the ||

1;

# __END__ # uncomment this and AutoSplit version 1.01 will split it

#-> sub CPAN::autobundle ;
sub autobundle;
#-> sub CPAN::bundle ;
sub bundle;
#-> sub CPAN::expand ;
sub expand;
#-> sub CPAN::force ;
sub force;
#-> sub CPAN::install ;
sub install;
#-> sub CPAN::make ;
sub make;
#-> sub CPAN::clean ;
sub clean;
#-> sub CPAN::test ;
sub test;

#-> sub CPAN::all ;
sub all_objects {
    my($mgr,$class) = @_;
    CPAN::Config->load unless $CPAN::Config_loaded++;
    CPAN->debug("mgr[$mgr] class[$class]") if $CPAN::DEBUG;
    CPAN::Index->reload;
    values %{ $META->{$class} };
}
*all = \&all_objects;

# Called by shell, not in batch mode. Not clean XXX
#-> sub CPAN::checklock ;
sub checklock {
    my($self) = @_;
    my $lockfile = MM->catfile($CPAN::Config->{cpan_home},".lock");
    if (-f $lockfile && -M _ > 0) {
	my $fh = FileHandle->new($lockfile);
	my $other = <$fh>;
	$fh->close;
	if (defined $other && $other) {
	    chomp $other;
	    return if $$==$other; # should never happen
	    $CPAN::Frontend->mywarn(
				    qq{
There seems to be running another CPAN process ($other). Contacting...
});
	    if (kill 0, $other) {
		$CPAN::Frontend->mydie(qq{Other job is running.
You may want to kill it and delete the lockfile, maybe. On UNIX try:
    kill $other
    rm $lockfile
});
	    } elsif (-w $lockfile) {
		my($ans) =
		    ExtUtils::MakeMaker::prompt
			(qq{Other job not responding. Shall I overwrite }.
			 qq{the lockfile? (Y/N)},"y");
		$CPAN::Frontend->myexit("Ok, bye\n")
		    unless $ans =~ /^y/i;
	    } else {
		Carp::croak(
			    qq{Lockfile $lockfile not writeable by you. }.
			    qq{Cannot proceed.\n}.
			    qq{    On UNIX try:\n}.
			    qq{    rm $lockfile\n}.
			    qq{  and then rerun us.\n}
			   );
	    }
	}
    }
    File::Path::mkpath($CPAN::Config->{cpan_home});
    my $fh;
    unless ($fh = FileHandle->new(">$lockfile")) {
	if ($! =~ /Permission/) {
	    my $incc = $INC{'CPAN/Config.pm'};
	    my $myincc = MM->catfile($ENV{HOME},'.cpan','CPAN','MyConfig.pm');
	    $CPAN::Frontend->myprint(qq{

Your configuration suggests that CPAN.pm should use a working
directory of
    $CPAN::Config->{cpan_home}
Unfortunately we could not create the lock file
    $lockfile
due to permission problems.

Please make sure that the configuration variable
    \$CPAN::Config->{cpan_home}
points to a directory where you can write a .lock file. You can set
this variable in either
    $incc
or
    $myincc

});
	}
	$CPAN::Frontend->mydie("Could not open >$lockfile: $!");
    }
    $fh->print($$, "\n");
    $self->{LOCK} = $lockfile;
    $fh->close;
    $SIG{'TERM'} = sub {
      &cleanup;
      $CPAN::Frontend->mydie("Got SIGTERM, leaving");
    };
    $SIG{'INT'} = sub {
      # no blocks!!!
      &cleanup if $Signal;
      $CPAN::Frontend->mydie("Got another SIGINT") if $Signal;
      print "Caught SIGINT\n";
      $Signal++;
    };
    $SIG{'__DIE__'} = \&cleanup;
    $self->debug("Signal handler set.") if $CPAN::DEBUG;
}

#-> sub CPAN::DESTROY ;
sub DESTROY {
    &cleanup; # need an eval?
}

#-> sub CPAN::cwd ;
sub cwd {Cwd::cwd();}

#-> sub CPAN::getcwd ;
sub getcwd {Cwd::getcwd();}

#-> sub CPAN::exists ;
sub exists {
    my($mgr,$class,$id) = @_;
    CPAN::Index->reload;
    ### Carp::croak "exists called without class argument" unless $class;
    $id ||= "";
    exists $META->{$class}{$id};
}

#-> sub CPAN::delete ;
sub delete {
  my($mgr,$class,$id) = @_;
  delete $META->{$class}{$id};
}

#-> sub CPAN::has_inst
sub has_inst {
    my($self,$mod,$message) = @_;
    Carp::croak("CPAN->has_inst() called without an argument")
	unless defined $mod;
    if (defined $message && $message eq "no") {
	$Dontload{$mod}||=1;
	return 0;
    } elsif (exists $Dontload{$mod}) {
	return 0;
    }
    my $file = $mod;
    my $obj;
    $file =~ s|::|/|g;
    $file =~ s|/|\\|g if $^O eq 'MSWin32';
    $file .= ".pm";
    if ($INC{$file}) {
	# checking %INC is wrong, because $INC{LWP} may be true
	# although $INC{"URI/URL.pm"} may have failed. But as
	# I really want to say "bla loaded OK", I have to somehow
	# cache results.
	### warn "$file in %INC"; #debug
	return 1;
    } elsif (eval { require $file }) {
	# eval is good: if we haven't yet read the database it's
	# perfect and if we have installed the module in the meantime,
	# it tries again. The second require is only a NOOP returning
	# 1 if we had success, otherwise it's retrying

	$CPAN::Frontend->myprint("CPAN: $mod loaded ok\n");
	if ($mod eq "CPAN::WAIT") {
	    push @CPAN::Shell::ISA, CPAN::WAIT;
	}
	return 1;
    } elsif ($mod eq "Net::FTP") {
	warn qq{
  Please, install Net::FTP as soon as possible. CPAN.pm installs it for you
  if you just type
      install Bundle::libnet

};
	sleep 2;
    } elsif ($mod eq "MD5"){
	$CPAN::Frontend->myprint(qq{
  CPAN: MD5 security checks disabled because MD5 not installed.
  Please consider installing the MD5 module.

});
	sleep 2;
    } else {
	delete $INC{$file}; # if it inc'd LWP but failed during, say, URI
    }
    return 0;
}

#-> sub CPAN::instance ;
sub instance {
    my($mgr,$class,$id) = @_;
    CPAN::Index->reload;
    $id ||= "";
    $META->{$class}{$id} ||= $class->new(ID => $id );
}

#-> sub CPAN::new ;
sub new {
    bless {}, shift;
}

#-> sub CPAN::cleanup ;
sub cleanup {
  # warn "cleanup called with arg[@_] End[$End] Signal[$Signal]";
  local $SIG{__DIE__} = '';
  my($message) = @_;
  my $i = 0;
  my $ineval = 0;
  if (
      0 &&           # disabled, try reload cpan with it
      $] > 5.004_60  # thereabouts
     ) {
    $ineval = $^S;
  } else {
    my($subroutine);
    while ((undef,undef,undef,$subroutine) = caller(++$i)) {
      $ineval = 1, last if
	  $subroutine eq '(eval)';
    }
  }
  return if $ineval && !$End;
  return unless defined $META->{'LOCK'};
  return unless -f $META->{'LOCK'};
  unlink $META->{'LOCK'};
  # require Carp;
  # Carp::cluck("DEBUGGING");
  $CPAN::Frontend->mywarn("Lockfile removed.\n");
}

package CPAN::CacheMgr;

#-> sub CPAN::CacheMgr::as_string ;
sub as_string {
    eval { require Data::Dumper };
    if ($@) {
	return shift->SUPER::as_string;
    } else {
	return Data::Dumper::Dumper(shift);
    }
}

#-> sub CPAN::CacheMgr::cachesize ;
sub cachesize {
    shift->{DU};
}

sub tidyup {
  my($self) = @_;
  return unless -d $self->{ID};
  while ($self->{DU} > $self->{'MAX'} ) {
    my($toremove) = shift @{$self->{FIFO}};
    $CPAN::Frontend->myprint(sprintf(
				     "Deleting from cache".
				     ": $toremove (%.1f>%.1f MB)\n",
				     $self->{DU}, $self->{'MAX'})
			    );
    return if $CPAN::Signal;
    $self->force_clean_cache($toremove);
    return if $CPAN::Signal;
  }
}

#-> sub CPAN::CacheMgr::dir ;
sub dir {
    shift->{ID};
}

#-> sub CPAN::CacheMgr::entries ;
sub entries {
    my($self,$dir) = @_;
    return unless defined $dir;
    $self->debug("reading dir[$dir]") if $CPAN::DEBUG;
    $dir ||= $self->{ID};
    my $getcwd;
    $getcwd  = $CPAN::Config->{'getcwd'} || 'cwd';
    my($cwd) = CPAN->$getcwd();
    chdir $dir or Carp::croak("Can't chdir to $dir: $!");
    my $dh = DirHandle->new(File::Spec->curdir)
        or Carp::croak("Couldn't opendir $dir: $!");
    my(@entries);
    for ($dh->read) {
	next if $_ eq "." || $_ eq "..";
	if (-f $_) {
	    push @entries, MM->catfile($dir,$_);
	} elsif (-d _) {
	    push @entries, MM->catdir($dir,$_);
	} else {
	    $CPAN::Frontend->mywarn("Warning: weird direntry in $dir: $_\n");
	}
    }
    chdir $cwd or Carp::croak("Can't chdir to $cwd: $!");
    sort { -M $b <=> -M $a} @entries;
}

#-> sub CPAN::CacheMgr::disk_usage ;
sub disk_usage {
    my($self,$dir) = @_;
    return if exists $self->{SIZE}{$dir};
    return if $CPAN::Signal;
    my($Du) = 0;
    find(
	 sub {
	   $File::Find::prune++ if $CPAN::Signal;
	   return if -l $_;
	   if ($^O eq 'MacOS') {
	     require Mac::Files;
	     my $cat  = Mac::Files::FSpGetCatInfo($_);
	     $Du += $cat->ioFlLgLen() + $cat->ioFlRLgLen();
	   } else {
	     $Du += (-s _);
	   }
	 },
	 $dir
	);
    return if $CPAN::Signal;
    $self->{SIZE}{$dir} = $Du/1024/1024;
    push @{$self->{FIFO}}, $dir;
    $self->debug("measured $dir is $Du") if $CPAN::DEBUG;
    $self->{DU} += $Du/1024/1024;
    $self->{DU};
}

#-> sub CPAN::CacheMgr::force_clean_cache ;
sub force_clean_cache {
    my($self,$dir) = @_;
    return unless -e $dir;
    $self->debug("have to rmtree $dir, will free $self->{SIZE}{$dir}")
	if $CPAN::DEBUG;
    File::Path::rmtree($dir);
    $self->{DU} -= $self->{SIZE}{$dir};
    delete $self->{SIZE}{$dir};
}

#-> sub CPAN::CacheMgr::new ;
sub new {
    my $class = shift;
    my $time = time;
    my($debug,$t2);
    $debug = "";
    my $self = {
		ID => $CPAN::Config->{'build_dir'},
		MAX => $CPAN::Config->{'build_cache'},
		SCAN => $CPAN::Config->{'scan_cache'} || 'atstart',
		DU => 0
	       };
    File::Path::mkpath($self->{ID});
    my $dh = DirHandle->new($self->{ID});
    bless $self, $class;
    $self->scan_cache;
    $t2 = time;
    $debug .= "timing of CacheMgr->new: ".($t2 - $time);
    $time = $t2;
    CPAN->debug($debug) if $CPAN::DEBUG;
    $self;
}

#-> sub CPAN::CacheMgr::scan_cache ;
sub scan_cache {
    my $self = shift;
    return if $self->{SCAN} eq 'never';
    $CPAN::Frontend->mydie("Unknown scan_cache argument: $self->{SCAN}")
	unless $self->{SCAN} eq 'atstart';
    $CPAN::Frontend->myprint(
			     sprintf("Scanning cache %s for sizes\n",
				     $self->{ID}));
    my $e;
    for $e ($self->entries($self->{ID})) {
	next if $e eq ".." || $e eq ".";
	$self->disk_usage($e);
	return if $CPAN::Signal;
    }
    $self->tidyup;
}

package CPAN::Debug;

#-> sub CPAN::Debug::debug ;
sub debug {
    my($self,$arg) = @_;
    my($caller,$func,$line,@rest) = caller(1); # caller(0) eg
                                               # Complete, caller(1)
                                               # eg readline
    ($caller) = caller(0);
    $caller =~ s/.*:://;
    $arg = "" unless defined $arg;
    my $rest = join "|", map { defined $_ ? $_ : "UNDEF" } @rest;
    if ($CPAN::DEBUG{$caller} & $CPAN::DEBUG){
	if ($arg and ref $arg) {
	    eval { require Data::Dumper };
	    if ($@) {
		$CPAN::Frontend->myprint($arg->as_string);
	    } else {
		$CPAN::Frontend->myprint(Data::Dumper::Dumper($arg));
	    }
	} else {
	    $CPAN::Frontend->myprint("Debug($caller:$func,$line,[$rest]): $arg\n");
	}
    }
}

package CPAN::Config;

#-> sub CPAN::Config::edit ;
sub edit {
    my($class,@args) = @_;
    return unless @args;
    CPAN->debug("class[$class]args[".join(" | ",@args)."]");
    my($o,$str,$func,$args,$key_exists);
    $o = shift @args;
    if($can{$o}) {
	$class->$o(@args);
	return 1;
    } else {
	if (ref($CPAN::Config->{$o}) eq ARRAY) {
	    $func = shift @args;
	    $func ||= "";
	    # Let's avoid eval, it's easier to comprehend without.
	    if ($func eq "push") {
		push @{$CPAN::Config->{$o}}, @args;
	    } elsif ($func eq "pop") {
		pop @{$CPAN::Config->{$o}};
	    } elsif ($func eq "shift") {
		shift @{$CPAN::Config->{$o}};
	    } elsif ($func eq "unshift") {
		unshift @{$CPAN::Config->{$o}}, @args;
	    } elsif ($func eq "splice") {
		splice @{$CPAN::Config->{$o}}, @args;
	    } elsif (@args) {
		$CPAN::Config->{$o} = [@args];
	    } else {
		$CPAN::Frontend->myprint(
					 join "",
					 "  $o  ",
					 ExtUtils::MakeMaker::neatvalue($CPAN::Config->{$o}),
					 "\n"
		     );
	    }
	} else {
	    $CPAN::Config->{$o} = $args[0] if defined $args[0];
	    $CPAN::Frontend->myprint("    $o    " .
				     (defined $CPAN::Config->{$o} ?
				      $CPAN::Config->{$o} : "UNDEFINED"));
	}
    }
}

#-> sub CPAN::Config::commit ;
sub commit {
    my($self,$configpm) = @_;
    unless (defined $configpm){
	$configpm ||= $INC{"CPAN/MyConfig.pm"};
	$configpm ||= $INC{"CPAN/Config.pm"};
	$configpm || Carp::confess(q{
CPAN::Config::commit called without an argument.
Please specify a filename where to save the configuration or try
"o conf init" to have an interactive course through configing.
});
    }
    my($mode);
    if (-f $configpm) {
	$mode = (stat $configpm)[2];
	if ($mode && ! -w _) {
	    Carp::confess("$configpm is not writable");
	}
    }

    my $msg = <<EOF unless $configpm =~ /MyConfig/;

# This is CPAN.pm's systemwide configuration file. This file provides
# defaults for users, and the values can be changed in a per-user
# configuration file. The user-config file is being looked for as
# ~/.cpan/CPAN/MyConfig.pm.

EOF
    $msg ||= "\n";
    my($fh) = FileHandle->new;
    rename $configpm, "$configpm~" if -f $configpm;
    open $fh, ">$configpm" or warn "Couldn't open >$configpm: $!";
    $fh->print(qq[$msg\$CPAN::Config = \{\n]);
    foreach (sort keys %$CPAN::Config) {
	$fh->print(
		   "  '$_' => ",
		   ExtUtils::MakeMaker::neatvalue($CPAN::Config->{$_}),
		   ",\n"
		  );
    }

    $fh->print("};\n1;\n__END__\n");
    close $fh;

    #$mode = 0444 | ( $mode & 0111 ? 0111 : 0 );
    #chmod $mode, $configpm;
###why was that so?    $self->defaults;
    $CPAN::Frontend->myprint("commit: wrote $configpm\n");
    1;
}

*default = \&defaults;
#-> sub CPAN::Config::defaults ;
sub defaults {
    my($self) = @_;
    $self->unload;
    $self->load;
    1;
}

sub init {
    my($self) = @_;
    undef $CPAN::Config->{'inhibit_startup_message'}; # lazy trick to
                                                      # have the least
                                                      # important
                                                      # variable
                                                      # undefined
    $self->load;
    1;
}

#-> sub CPAN::Config::load ;
sub load {
    my($self) = shift;
    my(@miss);
    use Carp;
    eval {require CPAN::Config;};       # We eval because of some
                                        # MakeMaker problems
    unless ($dot_cpan++){
      unshift @INC, MM->catdir($ENV{HOME},".cpan");
      eval {require CPAN::MyConfig;};   # where you can override
                                        # system wide settings
      shift @INC;
    }
    return unless @miss = $self->not_loaded;
    # XXX better check for arrayrefs too
    require CPAN::FirstTime;
    my($configpm,$fh,$redo,$theycalled);
    $redo ||= "";
    $theycalled++ if @miss==1 && $miss[0] eq 'inhibit_startup_message';
    if (defined $INC{"CPAN/Config.pm"} && -w $INC{"CPAN/Config.pm"}) {
	$configpm = $INC{"CPAN/Config.pm"};
	$redo++;
    } elsif (defined $INC{"CPAN/MyConfig.pm"} && -w $INC{"CPAN/MyConfig.pm"}) {
	$configpm = $INC{"CPAN/MyConfig.pm"};
	$redo++;
    } else {
	my($path_to_cpan) = File::Basename::dirname($INC{"CPAN.pm"});
	my($configpmdir) = MM->catdir($path_to_cpan,"CPAN");
	my($configpmtest) = MM->catfile($configpmdir,"Config.pm");
	if (-d $configpmdir or File::Path::mkpath($configpmdir)) {
	    if (-w $configpmtest) {
		$configpm = $configpmtest;
	    } elsif (-w $configpmdir) {
		#_#_# following code dumped core on me with 5.003_11, a.k.
		unlink "$configpmtest.bak" if -f "$configpmtest.bak";
		rename $configpmtest, "$configpmtest.bak" if -f $configpmtest;
		my $fh = FileHandle->new;
		if ($fh->open(">$configpmtest")) {
		    $fh->print("1;\n");
		    $configpm = $configpmtest;
		} else {
		    # Should never happen
		    Carp::confess("Cannot open >$configpmtest");
		}
	    }
	}
	unless ($configpm) {
	    $configpmdir = MM->catdir($ENV{HOME},".cpan","CPAN");
	    File::Path::mkpath($configpmdir);
	    $configpmtest = MM->catfile($configpmdir,"MyConfig.pm");
	    if (-w $configpmtest) {
		$configpm = $configpmtest;
	    } elsif (-w $configpmdir) {
		#_#_# following code dumped core on me with 5.003_11, a.k.
		my $fh = FileHandle->new;
		if ($fh->open(">$configpmtest")) {
		    $fh->print("1;\n");
		    $configpm = $configpmtest;
		} else {
		    # Should never happen
		    Carp::confess("Cannot open >$configpmtest");
		}
	    } else {
		Carp::confess(qq{WARNING: CPAN.pm is unable to }.
			      qq{create a configuration file.});
	    }
	}
    }
    local($") = ", ";
    $CPAN::Frontend->myprint(<<END) if $redo && ! $theycalled;
We have to reconfigure CPAN.pm due to following uninitialized parameters:

@miss
END
    $CPAN::Frontend->myprint(qq{
$configpm initialized.
});
    sleep 2;
    CPAN::FirstTime::init($configpm);
}

#-> sub CPAN::Config::not_loaded ;
sub not_loaded {
    my(@miss);
    for (qw(
	    cpan_home keep_source_where build_dir build_cache scan_cache
	    index_expire gzip tar unzip make pager makepl_arg make_arg
	    make_install_arg urllist inhibit_startup_message
	    ftp_proxy http_proxy no_proxy prerequisites_policy
	   )) {
	push @miss, $_ unless defined $CPAN::Config->{$_};
    }
    return @miss;
}

#-> sub CPAN::Config::unload ;
sub unload {
    delete $INC{'CPAN/MyConfig.pm'};
    delete $INC{'CPAN/Config.pm'};
}

#-> sub CPAN::Config::help ;
sub help {
    $CPAN::Frontend->myprint(q[
Known options:
  defaults  reload default config values from disk
  commit    commit session changes to disk
  init      go through a dialog to set all parameters

You may edit key values in the follow fashion:

  o conf build_cache 15

  o conf build_dir "/foo/bar"

  o conf urllist shift

  o conf urllist unshift ftp://ftp.foo.bar/

]);
    undef; #don't reprint CPAN::Config
}

#-> sub CPAN::Config::cpl ;
sub cpl {
    my($word,$line,$pos) = @_;
    $word ||= "";
    CPAN->debug("word[$word] line[$line] pos[$pos]") if $CPAN::DEBUG;
    my(@words) = split " ", substr($line,0,$pos+1);
    if (
	defined($words[2])
	and
	(
	 $words[2] =~ /list$/ && @words == 3
	 ||
	 $words[2] =~ /list$/ && @words == 4 && length($word)
	)
       ) {
	return grep /^\Q$word\E/, qw(splice shift unshift pop push);
    } elsif (@words >= 4) {
	return ();
    }
    my(@o_conf) = (keys %CPAN::Config::can, keys %$CPAN::Config);
    return grep /^\Q$word\E/, @o_conf;
}

package CPAN::Shell;

#-> sub CPAN::Shell::h ;
sub h {
    my($class,$about) = @_;
    if (defined $about) {
	$CPAN::Frontend->myprint("Detailed help not yet implemented\n");
    } else {
	$CPAN::Frontend->myprint(q{
command   arguments       description
a         string                  authors
b         or              display bundles
d         /regex/         info    distributions
m         or              about   modules
i         none                    anything of above

r          as             reinstall recommendations
u          above          uninstalled distributions
See manpage for autobundle, recompile, force, look, etc.

make                      make
test      modules,        make test (implies make)
install   dists, bundles, make install (implies test)
clean     "r" or "u"      make clean
readme                    display the README file

reload    index|cpan    load most recent indices/CPAN.pm
h or ?                  display this menu
o         various       set and query options
!         perl-code     eval a perl command
q                       quit the shell subroutine
});
    }
}

*help = \&h;

#-> sub CPAN::Shell::a ;
sub a { $CPAN::Frontend->myprint(shift->format_result('Author',@_));}
#-> sub CPAN::Shell::b ;
sub b {
    my($self,@which) = @_;
    CPAN->debug("which[@which]") if $CPAN::DEBUG;
    my($incdir,$bdir,$dh);
    foreach $incdir ($CPAN::Config->{'cpan_home'},@INC) {
	$bdir = MM->catdir($incdir,"Bundle");
	if ($dh = DirHandle->new($bdir)) { # may fail
	    my($entry);
	    for $entry ($dh->read) {
		next if -d MM->catdir($bdir,$entry);
		next unless $entry =~ s/\.pm$//;
		$CPAN::META->instance('CPAN::Bundle',"Bundle::$entry");
	    }
	}
    }
    $CPAN::Frontend->myprint($self->format_result('Bundle',@which));
}
#-> sub CPAN::Shell::d ;
sub d { $CPAN::Frontend->myprint(shift->format_result('Distribution',@_));}
#-> sub CPAN::Shell::m ;
sub m { # emacs confused here }; sub mimimimimi { # emacs in sync here
    $CPAN::Frontend->myprint(shift->format_result('Module',@_));
}

#-> sub CPAN::Shell::i ;
sub i {
    my($self) = shift;
    my(@args) = @_;
    my(@type,$type,@m);
    @type = qw/Author Bundle Distribution Module/;
    @args = '/./' unless @args;
    my(@result);
    for $type (@type) {
	push @result, $self->expand($type,@args);
    }
    my $result =  @result == 1 ?
	$result[0]->as_string :
	    join "", map {$_->as_glimpse} @result;
    $result ||= "No objects found of any type for argument @args\n";
    $CPAN::Frontend->myprint($result);
}

#-> sub CPAN::Shell::o ;
sub o {
    my($self,$o_type,@o_what) = @_;
    $o_type ||= "";
    CPAN->debug("o_type[$o_type] o_what[".join(" | ",@o_what)."]\n");
    if ($o_type eq 'conf') {
	shift @o_what if @o_what && $o_what[0] eq 'help';
	if (!@o_what) {
	    my($k,$v);
	    $CPAN::Frontend->myprint("CPAN::Config options");
	    if (exists $INC{'CPAN/Config.pm'}) {
	      $CPAN::Frontend->myprint(" from $INC{'CPAN/Config.pm'}");
	    }
	    if (exists $INC{'CPAN/MyConfig.pm'}) {
	      $CPAN::Frontend->myprint(" and $INC{'CPAN/MyConfig.pm'}");
	    }
	    $CPAN::Frontend->myprint(":\n");
	    for $k (sort keys %CPAN::Config::can) {
		$v = $CPAN::Config::can{$k};
		$CPAN::Frontend->myprint(sprintf "    %-18s %s\n", $k, $v);
	    }
	    $CPAN::Frontend->myprint("\n");
	    for $k (sort keys %$CPAN::Config) {
		$v = $CPAN::Config->{$k};
		if (ref $v) {
		    $CPAN::Frontend->myprint(
					     join(
						  "",
						  sprintf(
							  "    %-18s\n",
							  $k
							 ),
						  map {"\t$_\n"} @{$v}
						 )
					    );
		} else {
		    $CPAN::Frontend->myprint(sprintf "    %-18s %s\n", $k, $v);
		}
	    }
	    $CPAN::Frontend->myprint("\n");
	} elsif (!CPAN::Config->edit(@o_what)) {
	    $CPAN::Frontend->myprint(qq[Type 'o conf' to view configuration edit options\n\n]);
	}
    } elsif ($o_type eq 'debug') {
	my(%valid);
	@o_what = () if defined $o_what[0] && $o_what[0] =~ /help/i;
	if (@o_what) {
	    while (@o_what) {
		my($what) = shift @o_what;
		if ( exists $CPAN::DEBUG{$what} ) {
		    $CPAN::DEBUG |= $CPAN::DEBUG{$what};
		} elsif ($what =~ /^\d/) {
		    $CPAN::DEBUG = $what;
		} elsif (lc $what eq 'all') {
		    my($max) = 0;
		    for (values %CPAN::DEBUG) {
			$max += $_;
		    }
		    $CPAN::DEBUG = $max;
		} else {
		    my($known) = 0;
		    for (keys %CPAN::DEBUG) {
			next unless lc($_) eq lc($what);
			$CPAN::DEBUG |= $CPAN::DEBUG{$_};
			$known = 1;
		    }
		    $CPAN::Frontend->myprint("unknown argument [$what]\n")
			unless $known;
		}
	    }
	} else {
	    $CPAN::Frontend->myprint("Valid options for debug are ".
				     join(", ",sort(keys %CPAN::DEBUG), 'all').
		    qq{ or a number. Completion works on the options. }.
			qq{Case is ignored.\n\n});
	}
	if ($CPAN::DEBUG) {
	    $CPAN::Frontend->myprint("Options set for debugging:\n");
	    my($k,$v);
	    for $k (sort {$CPAN::DEBUG{$a} <=> $CPAN::DEBUG{$b}} keys %CPAN::DEBUG) {
		$v = $CPAN::DEBUG{$k};
		$CPAN::Frontend->myprint(sprintf "    %-14s(%s)\n", $k, $v) if $v & $CPAN::DEBUG;
	    }
	} else {
	    $CPAN::Frontend->myprint("Debugging turned off completely.\n");
	}
    } else {
	$CPAN::Frontend->myprint(qq{
Known options:
  conf    set or get configuration variables
  debug   set or get debugging options
});
    }
}

sub dotdot_onreload {
    my($ref) = shift;
    sub {
	if ( $_[0] =~ /Subroutine (\w+) redefined/ ) {
	    my($subr) = $1;
	    ++$$ref;
	    local($|) = 1;
	    # $CPAN::Frontend->myprint(".($subr)");
	    $CPAN::Frontend->myprint(".");
	    return;
	}
	warn @_;
    };
}

#-> sub CPAN::Shell::reload ;
sub reload {
    my($self,$command,@arg) = @_;
    $command ||= "";
    $self->debug("self[$self]command[$command]arg[@arg]") if $CPAN::DEBUG;
    if ($command =~ /cpan/i) {
	CPAN->debug("reloading the whole CPAN.pm") if $CPAN::DEBUG;
	my $fh = FileHandle->new($INC{'CPAN.pm'});
	local($/);
	$redef = 0;
	local($SIG{__WARN__}) = dotdot_onreload(\$redef);
	eval <$fh>;
	warn $@ if $@;
	$CPAN::Frontend->myprint("\n$redef subroutines redefined\n");
    } elsif ($command =~ /index/) {
      CPAN::Index->force_reload;
    } else {
      $CPAN::Frontend->myprint(qq{cpan     re-evals the CPAN.pm file
index    re-reads the index files\n});
    }
}

#-> sub CPAN::Shell::_binary_extensions ;
sub _binary_extensions {
    my($self) = shift @_;
    my(@result,$module,%seen,%need,$headerdone);
    my $isaperl = q{perl5[._-]\\d{3}(_[0-4][0-9])?\\.tar[._-]gz$};
    for $module ($self->expand('Module','/./')) {
	my $file  = $module->cpan_file;
	next if $file eq "N/A";
	next if $file =~ /^Contact Author/;
	next if $file =~ / $isaperl /xo;
	next unless $module->xs_file;
	local($|) = 1;
	$CPAN::Frontend->myprint(".");
	push @result, $module;
    }
#    print join " | ", @result;
    $CPAN::Frontend->myprint("\n");
    return @result;
}

#-> sub CPAN::Shell::recompile ;
sub recompile {
    my($self) = shift @_;
    my($module,@module,$cpan_file,%dist);
    @module = $self->_binary_extensions();
    for $module (@module){  # we force now and compile later, so we
                            # don't do it twice
	$cpan_file = $module->cpan_file;
	my $pack = $CPAN::META->instance('CPAN::Distribution',$cpan_file);
	$pack->force;
	$dist{$cpan_file}++;
    }
    for $cpan_file (sort keys %dist) {
	$CPAN::Frontend->myprint("  CPAN: Recompiling $cpan_file\n\n");
	my $pack = $CPAN::META->instance('CPAN::Distribution',$cpan_file);
	$pack->install;
	$CPAN::Signal = 0; # it's tempting to reset Signal, so we can
                           # stop a package from recompiling,
                           # e.g. IO-1.12 when we have perl5.003_10
    }
}

#-> sub CPAN::Shell::_u_r_common ;
sub _u_r_common {
    my($self) = shift @_;
    my($what) = shift @_;
    CPAN->debug("self[$self] what[$what] args[@_]") if $CPAN::DEBUG;
    Carp::croak "Usage: \$obj->_u_r_common($what)" unless defined $what;
    Carp::croak "Usage: \$obj->_u_r_common(a|r|u)" unless $what =~ /^[aru]$/;
    my(@args) = @_;
    @args = '/./' unless @args;
    my(@result,$module,%seen,%need,$headerdone,
       $version_undefs,$version_zeroes);
    $version_undefs = $version_zeroes = 0;
    my $sprintf = "%-25s %9s %9s  %s\n";
    for $module ($self->expand('Module',@args)) {
	my $file  = $module->cpan_file;
	next unless defined $file; # ??
	my($latest) = $module->cpan_version;
	my($inst_file) = $module->inst_file;
	my($have);
	return if $CPAN::Signal;
	if ($inst_file){
	    if ($what eq "a") {
		$have = $module->inst_version;
	    } elsif ($what eq "r") {
		$have = $module->inst_version;
		local($^W) = 0;
		if ($have eq "undef"){
		    $version_undefs++;
		} elsif ($have == 0){
		    $version_zeroes++;
		}
		next if $have >= $latest;
# to be pedantic we should probably say:
#    && !($have eq "undef" && $latest ne "undef" && $latest gt "");
# to catch the case where CPAN has a version 0 and we have a version undef
	    } elsif ($what eq "u") {
		next;
	    }
	} else {
	    if ($what eq "a") {
		next;
	    } elsif ($what eq "r") {
		next;
	    } elsif ($what eq "u") {
		$have = "-";
	    }
	}
	return if $CPAN::Signal; # this is sometimes lengthy
	$seen{$file} ||= 0;
	if ($what eq "a") {
	    push @result, sprintf "%s %s\n", $module->id, $have;
	} elsif ($what eq "r") {
	    push @result, $module->id;
	    next if $seen{$file}++;
	} elsif ($what eq "u") {
	    push @result, $module->id;
	    next if $seen{$file}++;
	    next if $file =~ /^Contact/;
	}
	unless ($headerdone++){
	    $CPAN::Frontend->myprint("\n");
	    $CPAN::Frontend->myprint(sprintf(
		   $sprintf,
		   "Package namespace",
		   "installed",
		   "latest",
		   "in CPAN file"
		   ));
	}
	$latest = substr($latest,0,8) if length($latest) > 8;
	$have = substr($have,0,8) if length($have) > 8;
	$CPAN::Frontend->myprint(sprintf $sprintf, $module->id, $have, $latest, $file);
	$need{$module->id}++;
    }
    unless (%need) {
	if ($what eq "u") {
	    $CPAN::Frontend->myprint("No modules found for @args\n");
	} elsif ($what eq "r") {
	    $CPAN::Frontend->myprint("All modules are up to date for @args\n");
	}
    }
    if ($what eq "r") {
	if ($version_zeroes) {
	    my $s_has = $version_zeroes > 1 ? "s have" : " has";
	    $CPAN::Frontend->myprint(qq{$version_zeroes installed module$s_has }.
		qq{a version number of 0\n});
	}
	if ($version_undefs) {
	    my $s_has = $version_undefs > 1 ? "s have" : " has";
	    $CPAN::Frontend->myprint(qq{$version_undefs installed module$s_has no }.
		qq{parseable version number\n});
	}
    }
    @result;
}

#-> sub CPAN::Shell::r ;
sub r {
    shift->_u_r_common("r",@_);
}

#-> sub CPAN::Shell::u ;
sub u {
    shift->_u_r_common("u",@_);
}

#-> sub CPAN::Shell::autobundle ;
sub autobundle {
    my($self) = shift;
    CPAN::Config->load unless $CPAN::Config_loaded++;
    my(@bundle) = $self->_u_r_common("a",@_);
    my($todir) = MM->catdir($CPAN::Config->{'cpan_home'},"Bundle");
    File::Path::mkpath($todir);
    unless (-d $todir) {
	$CPAN::Frontend->myprint("Couldn't mkdir $todir for some reason\n");
	return;
    }
    my($y,$m,$d) =  (localtime)[5,4,3];
    $y+=1900;
    $m++;
    my($c) = 0;
    my($me) = sprintf "Snapshot_%04d_%02d_%02d_%02d", $y, $m, $d, $c;
    my($to) = MM->catfile($todir,"$me.pm");
    while (-f $to) {
	$me = sprintf "Snapshot_%04d_%02d_%02d_%02d", $y, $m, $d, ++$c;
	$to = MM->catfile($todir,"$me.pm");
    }
    my($fh) = FileHandle->new(">$to") or Carp::croak "Can't open >$to: $!";
    $fh->print(
	       "package Bundle::$me;\n\n",
	       "\$VERSION = '0.01';\n\n",
	       "1;\n\n",
	       "__END__\n\n",
	       "=head1 NAME\n\n",
	       "Bundle::$me - Snapshot of installation on ",
	       $Config::Config{'myhostname'},
	       " on ",
	       scalar(localtime),
	       "\n\n=head1 SYNOPSIS\n\n",
	       "perl -MCPAN -e 'install Bundle::$me'\n\n",
	       "=head1 CONTENTS\n\n",
	       join("\n", @bundle),
	       "\n\n=head1 CONFIGURATION\n\n",
	       Config->myconfig,
	       "\n\n=head1 AUTHOR\n\n",
	       "This Bundle has been generated automatically ",
	       "by the autobundle routine in CPAN.pm.\n",
	      );
    $fh->close;
    $CPAN::Frontend->myprint("\nWrote bundle file
    $to\n\n");
}

#-> sub CPAN::Shell::expand ;
sub expand {
    shift;
    my($type,@args) = @_;
    my($arg,@m);
    for $arg (@args) {
	my $regex;
	if ($arg =~ m|^/(.*)/$|) {
	    $regex = $1;
	}
	my $class = "CPAN::$type";
	my $obj;
	if (defined $regex) {
	    for $obj ( sort {$a->id cmp $b->id} $CPAN::META->all_objects($class)) {
		push @m, $obj
		    if
			$obj->id =~ /$regex/i
			    or
			(
			 (
			  $] < 5.00303 ### provide sort of compatibility with 5.003
			  ||
			  $obj->can('name')
			 )
			 &&
			 $obj->name  =~ /$regex/i
			);
	    }
	} else {
	    my($xarg) = $arg;
	    if ( $type eq 'Bundle' ) {
		$xarg =~ s/^(Bundle::)?(.*)/Bundle::$2/;
	    }
	    if ($CPAN::META->exists($class,$xarg)) {
		$obj = $CPAN::META->instance($class,$xarg);
	    } elsif ($CPAN::META->exists($class,$arg)) {
		$obj = $CPAN::META->instance($class,$arg);
	    } else {
		next;
	    }
	    push @m, $obj;
	}
    }
    return wantarray ? @m : $m[0];
}

#-> sub CPAN::Shell::format_result ;
sub format_result {
    my($self) = shift;
    my($type,@args) = @_;
    @args = '/./' unless @args;
    my(@result) = $self->expand($type,@args);
    my $result =  @result == 1 ?
	$result[0]->as_string :
	    join "", map {$_->as_glimpse} @result;
    $result ||= "No objects of type $type found for argument @args\n";
    $result;
}

# The only reason for this method is currently to have a reliable
# debugging utility that reveals which output is going through which
# channel. No, I don't like the colors ;-)
sub print_ornamented {
    my($self,$what,$ornament) = @_;
    my $longest = 0;
    my $ornamenting = 0; # turn the colors on

    if ($ornamenting) {
	unless (defined &color) {
	    if ($CPAN::META->has_inst("Term::ANSIColor")) {
		import Term::ANSIColor "color";
	    } else {
		*color = sub { return "" };
	    }
	}
	my $line;
	for $line (split /\n/, $what) {
	    $longest = length($line) if length($line) > $longest;
	}
	my $sprintf = "%-" . $longest . "s";
	while ($what){
	    $what =~ s/(.*\n?)//m;
	    my $line = $1;
	    last unless $line;
	    my($nl) = chomp $line ? "\n" : "";
	    #	print "line[$line]ornament[$ornament]sprintf[$sprintf]\n";
	    print color($ornament), sprintf($sprintf,$line), color("reset"), $nl;
	}
    } else {
	print $what;
    }
}

sub myprint {
    my($self,$what) = @_;
    $self->print_ornamented($what, 'bold blue on_yellow');
}

sub myexit {
    my($self,$what) = @_;
    $self->myprint($what);
    exit;
}

sub mywarn {
    my($self,$what) = @_;
    $self->print_ornamented($what, 'bold red on_yellow');
}

sub myconfess {
    my($self,$what) = @_;
    $self->print_ornamented($what, 'bold red on_white');
    Carp::confess "died";
}

sub mydie {
    my($self,$what) = @_;
    $self->print_ornamented($what, 'bold red on_white');
    die "\n";
}

#-> sub CPAN::Shell::rematein ;
# RE-adme||MA-ke||TE-st||IN-stall
sub rematein {
    shift;
    my($meth,@some) = @_;
    my $pragma = "";
    if ($meth eq 'force') {
	$pragma = $meth;
	$meth = shift @some;
    }
    CPAN->debug("pragma[$pragma]meth[$meth] some[@some]") if $CPAN::DEBUG;
    my($s,@s);
    foreach $s (@some) {
      CPAN::Queue->new($s);
    }
    while ($s = CPAN::Queue->first) {
	my $obj;
	if (ref $s) {
	    $obj = $s;
	} elsif ($s =~ m|/|) { # looks like a file
	    $obj = $CPAN::META->instance('CPAN::Distribution',$s);
	} elsif ($s =~ m|^Bundle::|) {
	    $obj = $CPAN::META->instance('CPAN::Bundle',$s);
	} else {
	    $obj = $CPAN::META->instance('CPAN::Module',$s)
		if $CPAN::META->exists('CPAN::Module',$s);
	}
	if (ref $obj) {
	    CPAN->debug(
			qq{pragma[$pragma]meth[$meth]obj[$obj]as_string\[}.
			$obj->as_string.
			qq{\]}
		       ) if $CPAN::DEBUG;
	    $obj->$pragma()
		if
		    $pragma
			&&
		    ($] < 5.00303 || $obj->can($pragma)); ###
                                                          ### compatibility
                                                          ### with
                                                          ### 5.003
	    if ($]>=5.00303 && $obj->can('called_for')) {
	      $obj->called_for($s);
	    }
	    CPAN::Queue->delete($s) if $obj->$meth(); # if it is more
                                                      # than once in
                                                      # the queue
	} elsif ($CPAN::META->exists('CPAN::Author',$s)) {
	    $obj = $CPAN::META->instance('CPAN::Author',$s);
	    $CPAN::Frontend->myprint(
				     join "",
				     "Don't be silly, you can't $meth ",
				     $obj->fullname,
				     " ;-)\n"
				    );
	} else {
	    $CPAN::Frontend
		->myprint(qq{Warning: Cannot $meth $s, }.
			  qq{don\'t know what it is.
Try the command

    i /$s/

to find objects with similar identifiers.
});
	}
	CPAN::Queue->delete_first($s);
    }
}

#-> sub CPAN::Shell::force ;
sub force   { shift->rematein('force',@_); }
#-> sub CPAN::Shell::get ;
sub get     { shift->rematein('get',@_); }
#-> sub CPAN::Shell::readme ;
sub readme  { shift->rematein('readme',@_); }
#-> sub CPAN::Shell::make ;
sub make    { shift->rematein('make',@_); }
#-> sub CPAN::Shell::test ;
sub test    { shift->rematein('test',@_); }
#-> sub CPAN::Shell::install ;
sub install { shift->rematein('install',@_); }
#-> sub CPAN::Shell::clean ;
sub clean   { shift->rematein('clean',@_); }
#-> sub CPAN::Shell::look ;
sub look   { shift->rematein('look',@_); }

package CPAN::FTP;

#-> sub CPAN::FTP::ftp_get ;
sub ftp_get {
  my($class,$host,$dir,$file,$target) = @_;
  $class->debug(
		qq[Going to fetch file [$file] from dir [$dir]
	on host [$host] as local [$target]\n]
		      ) if $CPAN::DEBUG;
  my $ftp = Net::FTP->new($host);
  return 0 unless defined $ftp;
  $ftp->debug(1) if $CPAN::DEBUG{'FTP'} & $CPAN::DEBUG;
  $class->debug(qq[Going to ->login("anonymous","$Config::Config{'cf_email'}")\n]);
  unless ( $ftp->login("anonymous",$Config::Config{'cf_email'}) ){
    warn "Couldn't login on $host";
    return;
  }
  unless ( $ftp->cwd($dir) ){
    warn "Couldn't cwd $dir";
    return;
  }
  $ftp->binary;
  $class->debug(qq[Going to ->get("$file","$target")\n]) if $CPAN::DEBUG;
  unless ( $ftp->get($file,$target) ){
    warn "Couldn't fetch $file from $host\n";
    return;
  }
  $ftp->quit; # it's ok if this fails
  return 1;
}

# If more accuracy is wanted/needed, Chris Leach sent me this patch...

 # leach,> *** /install/perl/live/lib/CPAN.pm-	Wed Sep 24 13:08:48 1997
 # leach,> --- /tmp/cp	Wed Sep 24 13:26:40 1997
 # leach,> ***************
 # leach,> *** 1562,1567 ****
 # leach,> --- 1562,1580 ----
 # leach,>       return 1 if substr($url,0,4) eq "file";
 # leach,>       return 1 unless $url =~ m|://([^/]+)|;
 # leach,>       my $host = $1;
 # leach,> +     my $proxy = $CPAN::Config->{'http_proxy'} || $ENV{'http_proxy'};
 # leach,> +     if ($proxy) {
 # leach,> +         $proxy =~ m|://([^/:]+)|;
 # leach,> +         $proxy = $1;
 # leach,> +         my $noproxy = $CPAN::Config->{'no_proxy'} || $ENV{'no_proxy'};
 # leach,> +         if ($noproxy) {
 # leach,> +             if ($host !~ /$noproxy$/) {
 # leach,> +                 $host = $proxy;
 # leach,> +             }
 # leach,> +         } else {
 # leach,> +             $host = $proxy;
 # leach,> +         }
 # leach,> +     }
 # leach,>       require Net::Ping;
 # leach,>       return 1 unless $Net::Ping::VERSION >= 2;
 # leach,>       my $p;


# this is quite optimistic and returns one on several occasions where
# inappropriate. But this does no harm. It would do harm if we were
# too pessimistic (as I was before the http_proxy
sub is_reachable {
    my($self,$url) = @_;
    return 1; # we can't simply roll our own, firewalls may break ping
    return 0 unless $url;
    return 1 if substr($url,0,4) eq "file";
    return 1 unless $url =~ m|^(\w+)://([^/]+)|;
    my $proxytype = $1 . "_proxy"; # ftp_proxy or http_proxy
    my $host = $2;
    return 1 if $CPAN::Config->{$proxytype} || $ENV{$proxytype};
    require Net::Ping;
    return 1 unless $Net::Ping::VERSION >= 2;
    my $p;
    # 1.3101 had it different: only if the first eval raised an
    # exception we tried it with TCP. Now we are happy if icmp wins
    # the order and return, we don't even check for $@. Thanks to
    # thayer@uis.edu for the suggestion.
    eval {$p = Net::Ping->new("icmp");};
    return 1 if $p && ref($p) && $p->ping($host, 10);
    eval {$p = Net::Ping->new("tcp");};
    $CPAN::Frontend->mydie($@) if $@;
    return $p->ping($host, 10);
}

#-> sub CPAN::FTP::localize ;
# sorry for the ugly code here, I'll clean it up as soon as Net::FTP
# is in the core
sub localize {
    my($self,$file,$aslocal,$force) = @_;
    $force ||= 0;
    Carp::croak "Usage: ->localize(cpan_file,as_local_file[,$force])"
	unless defined $aslocal;
    $self->debug("file[$file] aslocal[$aslocal] force[$force]")
	if $CPAN::DEBUG;

    if ($^O eq 'MacOS') {
        my($name, $path) = File::Basename::fileparse($aslocal, '');
        if (length($name) > 31) {
            $name =~ s/(\.(readme(\.(gz|Z))?|(tar\.)?(gz|Z)|tgz|zip|pm\.(gz|Z)))$//;
            my $suf = $1;
            my $size = 31 - length($suf);
            while (length($name) > $size) {
                chop $name;
            }
            $name .= $suf;
            $aslocal = File::Spec->catfile($path, $name);
        }
    }

    return $aslocal if -f $aslocal && -r _ && !($force & 1);
    my($restore) = 0;
    if (-f $aslocal){
	rename $aslocal, "$aslocal.bak";
	$restore++;
    }

    my($aslocal_dir) = File::Basename::dirname($aslocal);
    File::Path::mkpath($aslocal_dir);
    $CPAN::Frontend->mywarn(qq{Warning: You are not allowed to write into }.
	qq{directory "$aslocal_dir".
    I\'ll continue, but if you encounter problems, they may be due
    to insufficient permissions.\n}) unless -w $aslocal_dir;

    # Inheritance is not easier to manage than a few if/else branches
    if ($CPAN::META->has_inst('LWP::UserAgent')) {
	require LWP::UserAgent;
 	unless ($Ua) {
	    $Ua = LWP::UserAgent->new;
	    my($var);
	    $Ua->proxy('ftp',  $var)
		if $var = $CPAN::Config->{'ftp_proxy'} || $ENV{'ftp_proxy'};
	    $Ua->proxy('http', $var)
		if $var = $CPAN::Config->{'http_proxy'} || $ENV{'http_proxy'};
	    $Ua->no_proxy($var)
		if $var = $CPAN::Config->{'no_proxy'} || $ENV{'no_proxy'};
	}
    }

    # Try the list of urls for each single object. We keep a record
    # where we did get a file from
    my(@reordered,$last);
    $CPAN::Config->{urllist} ||= [];
    $last = $#{$CPAN::Config->{urllist}};
    if ($force & 2) { # local cpans probably out of date, don't reorder
	@reordered = (0..$last);
    } else {
	@reordered =
	    sort {
		(substr($CPAN::Config->{urllist}[$b],0,4) eq "file")
		    <=>
		(substr($CPAN::Config->{urllist}[$a],0,4) eq "file")
		    or
		defined($Thesite)
		    and
		($b == $Thesite)
		    <=>
		($a == $Thesite)
	    } 0..$last;
    }
    my($level,@levels);
    if ($Themethod) {
	@levels = ($Themethod, grep {$_ ne $Themethod} qw/easy hard hardest/);
    } else {
	@levels = qw/easy hard hardest/;
    }
    @levels = qw/easy/ if $^O eq 'MacOS';
    for $level (@levels) {
	my $method = "host$level";
	my @host_seq = $level eq "easy" ?
	    @reordered : 0..$last;  # reordered has CDROM up front
	@host_seq = (0) unless @host_seq;
	my $ret = $self->$method(\@host_seq,$file,$aslocal);
	if ($ret) {
	  $Themethod = $level;
	  $self->debug("level[$level]") if $CPAN::DEBUG;
	  return $ret;
	} else {
	  unlink $aslocal;
	}
    }
    my(@mess);
    push @mess,
    qq{Please check, if the URLs I found in your configuration file \(}.
	join(", ", @{$CPAN::Config->{urllist}}).
	    qq{\) are valid. The urllist can be edited.},
	    qq{E.g. with ``o conf urllist push ftp://myurl/''};
    $CPAN::Frontend->myprint(Text::Wrap::wrap("","",@mess). "\n\n");
    sleep 2;
    $CPAN::Frontend->myprint("Cannot fetch $file\n\n");
    if ($restore) {
	rename "$aslocal.bak", $aslocal;
	$CPAN::Frontend->myprint("Trying to get away with old file:\n" .
				 $self->ls($aslocal));
	return $aslocal;
    }
    return;
}

sub hosteasy {
    my($self,$host_seq,$file,$aslocal) = @_;
    my($i);
  HOSTEASY: for $i (@$host_seq) {
      my $url = $CPAN::Config->{urllist}[$i] || $CPAN::Defaultsite;
	unless ($self->is_reachable($url)) {
	    $CPAN::Frontend->myprint("Skipping $url (seems to be not reachable)\n");
	    sleep 2;
	    next;
	}
	$url .= "/" unless substr($url,-1) eq "/";
	$url .= $file;
	$self->debug("localizing perlish[$url]") if $CPAN::DEBUG;
	if ($url =~ /^file:/) {
	    my $l;
	    if ($CPAN::META->has_inst('LWP')) {
		require URI::URL;
		my $u =  URI::URL->new($url);
		$l = $u->path;
	    } else { # works only on Unix, is poorly constructed, but
		# hopefully better than nothing.
		# RFC 1738 says fileurl BNF is
		# fileurl = "file://" [ host | "localhost" ] "/" fpath
		# Thanks to "Mark D. Baushke" <mdb@cisco.com> for
		# the code
		($l = $url) =~ s|^file://[^/]*/|/|; # discard the host part
		$l =~ s|^file:||;                   # assume they
                                                    # meant
                                                    # file://localhost
		$l =~ s|^/|| unless -f $l;          # e.g. /P:
	    }
	    if ( -f $l && -r _) {
		$Thesite = $i;
		return $l;
	    }
	    # Maybe mirror has compressed it?
	    if (-f "$l.gz") {
		$self->debug("found compressed $l.gz") if $CPAN::DEBUG;
		CPAN::Tarzip->gunzip("$l.gz", $aslocal);
		if ( -f $aslocal) {
		    $Thesite = $i;
		    return $aslocal;
		}
	    }
	}
      if ($CPAN::META->has_inst('LWP')) {
	  $CPAN::Frontend->myprint("Fetching with LWP:
  $url
");
	  unless ($Ua) {
	    require LWP::UserAgent;
	    $Ua = LWP::UserAgent->new;
	  }
	  my $res = $Ua->mirror($url, $aslocal);
	  if ($res->is_success) {
	    $Thesite = $i;
	    return $aslocal;
	  } elsif ($url !~ /\.gz$/) {
	    my $gzurl = "$url.gz";
	    $CPAN::Frontend->myprint("Fetching with LWP:
  $gzurl
");
	    $res = $Ua->mirror($gzurl, "$aslocal.gz");
	    if ($res->is_success &&
		CPAN::Tarzip->gunzip("$aslocal.gz",$aslocal)
	       ) {
	      $Thesite = $i;
	      return $aslocal;
	    } else {
	      # next HOSTEASY ;
	    }
	  } else {
	    # Alan Burlison informed me that in firewall envs Net::FTP
	    # can still succeed where LWP fails. So we do not skip
	    # Net::FTP anymore when LWP is available.
	    # next HOSTEASY ;
	  }
	} else {
	  $self->debug("LWP not installed") if $CPAN::DEBUG;
	}
	if ($url =~ m|^ftp://(.*?)/(.*)/(.*)|) {
	    # that's the nice and easy way thanks to Graham
	    my($host,$dir,$getfile) = ($1,$2,$3);
	    if ($CPAN::META->has_inst('Net::FTP')) {
		$dir =~ s|/+|/|g;
		$CPAN::Frontend->myprint("Fetching with Net::FTP:
  $url
");
		$self->debug("getfile[$getfile]dir[$dir]host[$host]" .
			     "aslocal[$aslocal]") if $CPAN::DEBUG;
		if (CPAN::FTP->ftp_get($host,$dir,$getfile,$aslocal)) {
		    $Thesite = $i;
		    return $aslocal;
		}
		if ($aslocal !~ /\.gz$/) {
		    my $gz = "$aslocal.gz";
		    $CPAN::Frontend->myprint("Fetching with Net::FTP
  $url.gz
");
		   if (CPAN::FTP->ftp_get($host,
					   $dir,
					   "$getfile.gz",
					   $gz) &&
			CPAN::Tarzip->gunzip($gz,$aslocal)
		       ){
			$Thesite = $i;
			return $aslocal;
		    }
		}
		# next HOSTEASY;
	    }
	}
    }
}

sub hosthard {
  my($self,$host_seq,$file,$aslocal) = @_;

  # Came back if Net::FTP couldn't establish connection (or
  # failed otherwise) Maybe they are behind a firewall, but they
  # gave us a socksified (or other) ftp program...

  my($i);
  my($devnull) = $CPAN::Config->{devnull} || "";
  # < /dev/null ";
  my($aslocal_dir) = File::Basename::dirname($aslocal);
  File::Path::mkpath($aslocal_dir);
  HOSTHARD: for $i (@$host_seq) {
	my $url = $CPAN::Config->{urllist}[$i] || $CPAN::Defaultsite;
	unless ($self->is_reachable($url)) {
	    $CPAN::Frontend->myprint("Skipping $url (not reachable)\n");
	    next;
	}
	$url .= "/" unless substr($url,-1) eq "/";
	$url .= $file;
	my($proto,$host,$dir,$getfile);

	# Courtesy Mark Conty mark_conty@cargill.com change from
	# if ($url =~ m|^ftp://(.*?)/(.*)/(.*)|) {
	# to
	if ($url =~ m|^([^:]+)://(.*?)/(.*)/(.*)|) {
	    # proto not yet used
	    ($proto,$host,$dir,$getfile) = ($1,$2,$3,$4);
	} else {
	    next HOSTHARD; # who said, we could ftp anything except ftp?
	}
	$self->debug("localizing funkyftpwise[$url]") if $CPAN::DEBUG;
	my($f,$funkyftp);
	for $f ('lynx','ncftpget','ncftp') {
	    next unless exists $CPAN::Config->{$f};
	    $funkyftp = $CPAN::Config->{$f};
	    next unless defined $funkyftp;
	    next if $funkyftp =~ /^\s*$/;
	    my($want_compressed);
	    my $aslocal_uncompressed;
	    ($aslocal_uncompressed = $aslocal) =~ s/\.gz//;
	    my($source_switch) = "";
	    $source_switch = " -source" if $funkyftp =~ /\blynx$/;
	    $source_switch = " -c" if $funkyftp =~ /\bncftp$/;
	    $CPAN::Frontend->myprint(
		  qq[
Trying with "$funkyftp$source_switch" to get
    $url
]);
	    my($system) = "$funkyftp$source_switch '$url' $devnull > ".
		"$aslocal_uncompressed";
	    $self->debug("system[$system]") if $CPAN::DEBUG;
	    my($wstatus);
	    if (($wstatus = system($system)) == 0
		&&
		-s $aslocal_uncompressed   # lynx returns 0 on my
                                           # system even if it fails
	       ) {
		if ($aslocal_uncompressed ne $aslocal) {
		  # test gzip integrity
		  if (
		      CPAN::Tarzip->gtest($aslocal_uncompressed)
		     ) {
		    rename $aslocal_uncompressed, $aslocal;
		  } else {
		    CPAN::Tarzip->gzip($aslocal_uncompressed,
				     "$aslocal_uncompressed.gz");
		  }
		}
		$Thesite = $i;
		return $aslocal;
	    } elsif ($url !~ /\.gz$/) {
	      unlink $aslocal_uncompressed if
		  -f $aslocal_uncompressed && -s _ == 0;
	      my $gz = "$aslocal.gz";
	      my $gzurl = "$url.gz";
	      $CPAN::Frontend->myprint(
		      qq[
Trying with "$funkyftp$source_switch" to get
  $url.gz
]);
	      my($system) = "$funkyftp$source_switch '$url.gz' $devnull > ".
		  "$aslocal_uncompressed.gz";
	      $self->debug("system[$system]") if $CPAN::DEBUG;
	      my($wstatus);
	      if (($wstatus = system($system)) == 0
		  &&
		  -s "$aslocal_uncompressed.gz"
		 ) {
		# test gzip integrity
		if (CPAN::Tarzip->gtest("$aslocal_uncompressed.gz")) {
		  CPAN::Tarzip->gunzip("$aslocal_uncompressed.gz",
				       $aslocal);
		} else {
		  rename $aslocal_uncompressed, $aslocal;
		}
		$Thesite = $i;
		return $aslocal;
	      } else {
		unlink "$aslocal_uncompressed.gz" if
		    -f "$aslocal_uncompressed.gz";
	      }
	    } else {
		my $estatus = $wstatus >> 8;
		my $size = -f $aslocal ? ", left\n$aslocal with size ".-s _ : "";
		$CPAN::Frontend->myprint(qq{
System call "$system"
returned status $estatus (wstat $wstatus)$size
});
	    }
	}
    }
}

sub hosthardest {
    my($self,$host_seq,$file,$aslocal) = @_;

    my($i);
    my($aslocal_dir) = File::Basename::dirname($aslocal);
    File::Path::mkpath($aslocal_dir);
  HOSTHARDEST: for $i (@$host_seq) {
	unless (length $CPAN::Config->{'ftp'}) {
	    $CPAN::Frontend->myprint("No external ftp command available\n\n");
	    last HOSTHARDEST;
	}
	my $url = $CPAN::Config->{urllist}[$i] || $CPAN::Defaultsite;
	unless ($self->is_reachable($url)) {
	    $CPAN::Frontend->myprint("Skipping $url (not reachable)\n");
	    next;
	}
	$url .= "/" unless substr($url,-1) eq "/";
	$url .= $file;
	$self->debug("localizing ftpwise[$url]") if $CPAN::DEBUG;
	unless ($url =~ m|^ftp://(.*?)/(.*)/(.*)|) {
	    next;
	}
	my($host,$dir,$getfile) = ($1,$2,$3);
	my($netrcfile,$fh);
	my $timestamp = 0;
	my($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,
	   $ctime,$blksize,$blocks) = stat($aslocal);
	$timestamp = $mtime ||= 0;
	my($netrc) = CPAN::FTP::netrc->new;
	my($verbose) = $CPAN::DEBUG{'FTP'} & $CPAN::DEBUG ? " -v" : "";
	my $targetfile = File::Basename::basename($aslocal);
	my(@dialog);
	push(
	     @dialog,
	     "lcd $aslocal_dir",
	     "cd /",
	     map("cd $_", split "/", $dir), # RFC 1738
	     "bin",
	     "get $getfile $targetfile",
	     "quit"
	    );
	if (! $netrc->netrc) {
	    CPAN->debug("No ~/.netrc file found") if $CPAN::DEBUG;
	} elsif ($netrc->hasdefault || $netrc->contains($host)) {
	    CPAN->debug(sprintf("hasdef[%d]cont($host)[%d]",
				$netrc->hasdefault,
				$netrc->contains($host))) if $CPAN::DEBUG;
	    if ($netrc->protected) {
		$CPAN::Frontend->myprint(qq{
  Trying with external ftp to get
    $url
  As this requires some features that are not thoroughly tested, we\'re
  not sure, that we get it right....

}
		     );
		$self->talk_ftp("$CPAN::Config->{'ftp'}$verbose $host",
				@dialog);
		($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
		 $atime,$mtime,$ctime,$blksize,$blocks) = stat($aslocal);
		$mtime ||= 0;
		if ($mtime > $timestamp) {
		    $CPAN::Frontend->myprint("GOT $aslocal\n");
		    $Thesite = $i;
		    return $aslocal;
		} else {
		    $CPAN::Frontend->myprint("Hmm... Still failed!\n");
		}
	    } else {
		$CPAN::Frontend->mywarn(qq{Your $netrcfile is not }.
					qq{correctly protected.\n});
	    }
	} else {
	    $CPAN::Frontend->mywarn("Your ~/.netrc neither contains $host
  nor does it have a default entry\n");
	}

	# OK, they don't have a valid ~/.netrc. Use 'ftp -n'
	# then and login manually to host, using e-mail as
	# password.
	$CPAN::Frontend->myprint(qq{Issuing "$CPAN::Config->{'ftp'}$verbose -n"\n});
	unshift(
		@dialog,
		"open $host",
		"user anonymous $Config::Config{'cf_email'}"
	       );
	$self->talk_ftp("$CPAN::Config->{'ftp'}$verbose -n", @dialog);
	($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
	 $atime,$mtime,$ctime,$blksize,$blocks) = stat($aslocal);
	$mtime ||= 0;
	if ($mtime > $timestamp) {
	    $CPAN::Frontend->myprint("GOT $aslocal\n");
	    $Thesite = $i;
	    return $aslocal;
	} else {
	    $CPAN::Frontend->myprint("Bad luck... Still failed!\n");
	}
	$CPAN::Frontend->myprint("Can't access URL $url.\n\n");
	sleep 2;
    }
}

sub talk_ftp {
    my($self,$command,@dialog) = @_;
    my $fh = FileHandle->new;
    $fh->open("|$command") or die "Couldn't open ftp: $!";
    foreach (@dialog) { $fh->print("$_\n") }
    $fh->close;		# Wait for process to complete
    my $wstatus = $?;
    my $estatus = $wstatus >> 8;
    $CPAN::Frontend->myprint(qq{
Subprocess "|$command"
  returned status $estatus (wstat $wstatus)
}) if $wstatus;
}

# find2perl needs modularization, too, all the following is stolen
# from there
# CPAN::FTP::ls
sub ls {
    my($self,$name) = @_;
    my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$sizemm,
     $atime,$mtime,$ctime,$blksize,$blocks) = lstat($name);

    my($perms,%user,%group);
    my $pname = $name;

    if ($blocks) {
	$blocks = int(($blocks + 1) / 2);
    }
    else {
	$blocks = int(($sizemm + 1023) / 1024);
    }

    if    (-f _) { $perms = '-'; }
    elsif (-d _) { $perms = 'd'; }
    elsif (-c _) { $perms = 'c'; $sizemm = &sizemm; }
    elsif (-b _) { $perms = 'b'; $sizemm = &sizemm; }
    elsif (-p _) { $perms = 'p'; }
    elsif (-S _) { $perms = 's'; }
    else         { $perms = 'l'; $pname .= ' -> ' . readlink($_); }

    my(@rwx) = ('---','--x','-w-','-wx','r--','r-x','rw-','rwx');
    my(@moname) = qw(Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec);
    my $tmpmode = $mode;
    my $tmp = $rwx[$tmpmode & 7];
    $tmpmode >>= 3;
    $tmp = $rwx[$tmpmode & 7] . $tmp;
    $tmpmode >>= 3;
    $tmp = $rwx[$tmpmode & 7] . $tmp;
    substr($tmp,2,1) =~ tr/-x/Ss/ if -u _;
    substr($tmp,5,1) =~ tr/-x/Ss/ if -g _;
    substr($tmp,8,1) =~ tr/-x/Tt/ if -k _;
    $perms .= $tmp;

    my $user = $user{$uid} || $uid;   # too lazy to implement lookup
    my $group = $group{$gid} || $gid;

    my($sec,$min,$hour,$mday,$mon,$year) = localtime($mtime);
    my($timeyear);
    my($moname) = $moname[$mon];
    if (-M _ > 365.25 / 2) {
	$timeyear = $year + 1900;
    }
    else {
	$timeyear = sprintf("%02d:%02d", $hour, $min);
    }

    sprintf "%5lu %4ld %-10s %2d %-8s %-8s %8s %s %2d %5s %s\n",
	    $ino,
		 $blocks,
		      $perms,
			    $nlink,
				$user,
				     $group,
					  $sizemm,
					      $moname,
						 $mday,
						     $timeyear,
							 $pname;
}

package CPAN::FTP::netrc;

sub new {
    my($class) = @_;
    my $file = MM->catfile($ENV{HOME},".netrc");

    my($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
       $atime,$mtime,$ctime,$blksize,$blocks)
	= stat($file);
    $mode ||= 0;
    my $protected = 0;

    my($fh,@machines,$hasdefault);
    $hasdefault = 0;
    $fh = FileHandle->new or die "Could not create a filehandle";

    if($fh->open($file)){
	$protected = ($mode & 077) == 0;
	local($/) = "";
      NETRC: while (<$fh>) {
	    my(@tokens) = split " ", $_;
	  TOKEN: while (@tokens) {
		my($t) = shift @tokens;
		if ($t eq "default"){
		    $hasdefault++;
		    last NETRC;
		}
		last TOKEN if $t eq "macdef";
		if ($t eq "machine") {
		    push @machines, shift @tokens;
		}
	    }
	}
    } else {
	$file = $hasdefault = $protected = "";
    }

    bless {
	   'mach' => [@machines],
	   'netrc' => $file,
	   'hasdefault' => $hasdefault,
	   'protected' => $protected,
	  }, $class;
}

sub hasdefault { shift->{'hasdefault'} }
sub netrc      { shift->{'netrc'}      }
sub protected  { shift->{'protected'}  }
sub contains {
    my($self,$mach) = @_;
    for ( @{$self->{'mach'}} ) {
	return 1 if $_ eq $mach;
    }
    return 0;
}

package CPAN::Complete;

sub gnu_cpl {
    my($text, $line, $start, $end) = @_;
    my(@perlret) = cpl($text, $line, $start);
    # find longest common match. Can anybody show me how to peruse
    # T::R::Gnu to have this done automatically? Seems expensive.
    return () unless @perlret;
    my($newtext) = $text;
    for (my $i = length($text)+1;;$i++) {
	last unless length($perlret[0]) && length($perlret[0]) >= $i;
	my $try = substr($perlret[0],0,$i);
	my @tries = grep {substr($_,0,$i) eq $try} @perlret;
	# warn "try[$try]tries[@tries]";
	if (@tries == @perlret) {
	    $newtext = $try;
	} else {
	    last;
	}
    }
    ($newtext,@perlret);
}

#-> sub CPAN::Complete::cpl ;
sub cpl {
    my($word,$line,$pos) = @_;
    $word ||= "";
    $line ||= "";
    $pos ||= 0;
    CPAN->debug("word [$word] line[$line] pos[$pos]") if $CPAN::DEBUG;
    $line =~ s/^\s*//;
    if ($line =~ s/^(force\s*)//) {
	$pos -= length($1);
    }
    my @return;
    if ($pos == 0) {
	@return = grep(
		       /^$word/,
		       sort qw(
			       ! a b d h i m o q r u autobundle clean
			       make test install force reload look
			      )
		      );
    } elsif ( $line !~ /^[\!abdhimorutl]/ ) {
	@return = ();
    } elsif ($line =~ /^a\s/) {
	@return = cplx('CPAN::Author',$word);
    } elsif ($line =~ /^b\s/) {
	@return = cplx('CPAN::Bundle',$word);
    } elsif ($line =~ /^d\s/) {
	@return = cplx('CPAN::Distribution',$word);
    } elsif ($line =~ /^([mru]|make|clean|test|install|readme|look)\s/ ) {
	@return = (cplx('CPAN::Module',$word),cplx('CPAN::Bundle',$word));
    } elsif ($line =~ /^i\s/) {
	@return = cpl_any($word);
    } elsif ($line =~ /^reload\s/) {
	@return = cpl_reload($word,$line,$pos);
    } elsif ($line =~ /^o\s/) {
	@return = cpl_option($word,$line,$pos);
    } else {
	@return = ();
    }
    return @return;
}

#-> sub CPAN::Complete::cplx ;
sub cplx {
    my($class, $word) = @_;
    grep /^\Q$word\E/, map { $_->id } $CPAN::META->all_objects($class);
}

#-> sub CPAN::Complete::cpl_any ;
sub cpl_any {
    my($word) = shift;
    return (
	    cplx('CPAN::Author',$word),
	    cplx('CPAN::Bundle',$word),
	    cplx('CPAN::Distribution',$word),
	    cplx('CPAN::Module',$word),
	   );
}

#-> sub CPAN::Complete::cpl_reload ;
sub cpl_reload {
    my($word,$line,$pos) = @_;
    $word ||= "";
    my(@words) = split " ", $line;
    CPAN->debug("word[$word] line[$line] pos[$pos]") if $CPAN::DEBUG;
    my(@ok) = qw(cpan index);
    return @ok if @words == 1;
    return grep /^\Q$word\E/, @ok if @words == 2 && $word;
}

#-> sub CPAN::Complete::cpl_option ;
sub cpl_option {
    my($word,$line,$pos) = @_;
    $word ||= "";
    my(@words) = split " ", $line;
    CPAN->debug("word[$word] line[$line] pos[$pos]") if $CPAN::DEBUG;
    my(@ok) = qw(conf debug);
    return @ok if @words == 1;
    return grep /^\Q$word\E/, @ok if @words == 2 && length($word);
    if (0) {
    } elsif ($words[1] eq 'index') {
	return ();
    } elsif ($words[1] eq 'conf') {
	return CPAN::Config::cpl(@_);
    } elsif ($words[1] eq 'debug') {
	return sort grep /^\Q$word\E/, sort keys %CPAN::DEBUG, 'all';
    }
}

package CPAN::Index;

#-> sub CPAN::Index::force_reload ;
sub force_reload {
    my($class) = @_;
    $CPAN::Index::last_time = 0;
    $class->reload(1);
}

#-> sub CPAN::Index::reload ;
sub reload {
    my($cl,$force) = @_;
    my $time = time;

    # XXX check if a newer one is available. (We currently read it
    # from time to time)
    for ($CPAN::Config->{index_expire}) {
	$_ = 0.001 unless $_ > 0.001;
    }
    return if $last_time + $CPAN::Config->{index_expire}*86400 > $time
	and ! $force;
    my($debug,$t2);
    $last_time = $time;

    my $needshort = $^O eq "dos";

    $cl->rd_authindex($cl
		      ->reload_x(
				 "authors/01mailrc.txt.gz",
				 $needshort ?
				 File::Spec->catfile('authors', '01mailrc.gz') :
				 File::Spec->catfile('authors', '01mailrc.txt.gz'),
				 $force));
    $t2 = time;
    $debug = "timing reading 01[".($t2 - $time)."]";
    $time = $t2;
    return if $CPAN::Signal; # this is sometimes lengthy
    $cl->rd_modpacks($cl
		     ->reload_x(
				"modules/02packages.details.txt.gz",
				$needshort ?
				File::Spec->catfile('modules', '02packag.gz') :
				File::Spec->catfile('modules', '02packages.details.txt.gz'),
				$force));
    $t2 = time;
    $debug .= "02[".($t2 - $time)."]";
    $time = $t2;
    return if $CPAN::Signal; # this is sometimes lengthy
    $cl->rd_modlist($cl
		    ->reload_x(
			       "modules/03modlist.data.gz",
			       $needshort ?
			       File::Spec->catfile('modules', '03mlist.gz') :
			       File::Spec->catfile('modules', '03modlist.data.gz'),
			       $force));
    $t2 = time;
    $debug .= "03[".($t2 - $time)."]";
    $time = $t2;
    CPAN->debug($debug) if $CPAN::DEBUG;
}

#-> sub CPAN::Index::reload_x ;
sub reload_x {
    my($cl,$wanted,$localname,$force) = @_;
    $force |= 2; # means we're dealing with an index here
    CPAN::Config->load; # we should guarantee loading wherever we rely
                        # on Config XXX
    $localname ||= $wanted;
    my $abs_wanted = MM->catfile($CPAN::Config->{'keep_source_where'},
				   $localname);
    if (
	-f $abs_wanted &&
	-M $abs_wanted < $CPAN::Config->{'index_expire'} &&
	!($force & 1)
       ) {
	my $s = $CPAN::Config->{'index_expire'} == 1 ? "" : "s";
	$cl->debug(qq{$abs_wanted younger than $CPAN::Config->{'index_expire'} }.
		   qq{day$s. I\'ll use that.});
	return $abs_wanted;
    } else {
	$force |= 1; # means we're quite serious about it.
    }
    return CPAN::FTP->localize($wanted,$abs_wanted,$force);
}

#-> sub CPAN::Index::rd_authindex ;
sub rd_authindex {
    my($cl, $index_target) = @_;
    my @lines;
    return unless defined $index_target;
    $CPAN::Frontend->myprint("Going to read $index_target\n");
#    my $fh = CPAN::Tarzip->TIEHANDLE($index_target);
#    while ($_ = $fh->READLINE) {
    # no strict 'refs';
    local(*FH);
    tie *FH, CPAN::Tarzip, $index_target;
    local($/) = "\n";
    push @lines, split /\012/ while <FH>;
    foreach (@lines) {
	my($userid,$fullname,$email) =
	    m/alias\s+(\S+)\s+\"([^\"\<]+)\s+\<([^\>]+)\>\"/;
	next unless $userid && $fullname && $email;

	# instantiate an author object
 	my $userobj = $CPAN::META->instance('CPAN::Author',$userid);
	$userobj->set('FULLNAME' => $fullname, 'EMAIL' => $email);
	return if $CPAN::Signal;
    }
}

sub userid {
  my($self,$dist) = @_;
  $dist = $self->{'id'} unless defined $dist;
  my($ret) = $dist =~ m|(?:\w/\w\w/)?([^/]+)/|;
  $ret;
}

#-> sub CPAN::Index::rd_modpacks ;
sub rd_modpacks {
    my($cl, $index_target) = @_;
    my @lines;
    return unless defined $index_target;
    $CPAN::Frontend->myprint("Going to read $index_target\n");
    my $fh = CPAN::Tarzip->TIEHANDLE($index_target);
    local($/) = "\n";
    while ($_ = $fh->READLINE) {
	s/\012/\n/g;
	my @ls = map {"$_\n"} split /\n/, $_;
	unshift @ls, "\n" x length($1) if /^(\n+)/;
	push @lines, @ls;
    }
    while (@lines) {
	my $shift = shift(@lines);
	last if $shift =~ /^\s*$/;
    }
    foreach (@lines) {
	chomp;
	my($mod,$version,$dist) = split;
###	$version =~ s/^\+//;

	# if it is a bundle, instatiate a bundle object
	my($bundle,$id,$userid);

	if ($mod eq 'CPAN' &&
	    ! (
	       CPAN::Queue->exists('Bundle::CPAN') ||
	       CPAN::Queue->exists('CPAN')
	      )
	   ) {
	    local($^W)= 0;
	    if ($version > $CPAN::VERSION){
		$CPAN::Frontend->myprint(qq{
  There\'s a new CPAN.pm version (v$version) available!
  You might want to try
    install Bundle::CPAN
    reload cpan
  without quitting the current session. It should be a seamless upgrade
  while we are running...
});
		sleep 2;
		$CPAN::Frontend->myprint(qq{\n});
	    }
	    last if $CPAN::Signal;
	} elsif ($mod =~ /^Bundle::(.*)/) {
	    $bundle = $1;
	}

	if ($bundle){
	    $id =  $CPAN::META->instance('CPAN::Bundle',$mod);
	    # warn "made mod[$mod]a bundle";
	    # Let's make it a module too, because bundles have so much
	    # in common with modules
	    $CPAN::META->instance('CPAN::Module',$mod);
	    # warn "made mod[$mod]a module";

# This "next" makes us faster but if the job is running long, we ignore
# rereads which is bad. So we have to be a bit slower again.
#	} elsif ($CPAN::META->exists('CPAN::Module',$mod)) {
#	    next;

	}
	else {
	    # instantiate a module object
	    $id = $CPAN::META->instance('CPAN::Module',$mod);
	}

	if ($id->cpan_file ne $dist){
	    $userid = $cl->userid($dist);
	    $id->set(
		     'CPAN_USERID' => $userid,
		     'CPAN_VERSION' => $version,
		     'CPAN_FILE' => $dist
		    );
	}

	# instantiate a distribution object
	unless ($CPAN::META->exists('CPAN::Distribution',$dist)) {
	    $CPAN::META->instance(
				  'CPAN::Distribution' => $dist
				 )->set(
					'CPAN_USERID' => $userid
				       );
	}

	return if $CPAN::Signal;
    }
    undef $fh;
}

#-> sub CPAN::Index::rd_modlist ;
sub rd_modlist {
    my($cl,$index_target) = @_;
    return unless defined $index_target;
    $CPAN::Frontend->myprint("Going to read $index_target\n");
    my $fh = CPAN::Tarzip->TIEHANDLE($index_target);
    my @eval;
    local($/) = "\n";
    while ($_ = $fh->READLINE) {
	s/\012/\n/g;
	my @ls = map {"$_\n"} split /\n/, $_;
	unshift @ls, "\n" x length($1) if /^(\n+)/;
	push @eval, @ls;
    }
    while (@eval) {
	my $shift = shift(@eval);
	if ($shift =~ /^Date:\s+(.*)/){
	    return if $date_of_03 eq $1;
	    ($date_of_03) = $1;
	}
	last if $shift =~ /^\s*$/;
    }
    undef $fh;
    push @eval, q{CPAN::Modulelist->data;};
    local($^W) = 0;
    my($comp) = Safe->new("CPAN::Safe1");
    my($eval) = join("", @eval);
    my $ret = $comp->reval($eval);
    Carp::confess($@) if $@;
    return if $CPAN::Signal;
    for (keys %$ret) {
	my $obj = $CPAN::META->instance(CPAN::Module,$_);
	$obj->set(%{$ret->{$_}});
	return if $CPAN::Signal;
    }
}

package CPAN::InfoObj;

#-> sub CPAN::InfoObj::new ;
sub new { my $this = bless {}, shift; %$this = @_; $this }

#-> sub CPAN::InfoObj::set ;
sub set {
    my($self,%att) = @_;
    my(%oldatt) = %$self;
    %$self = (%oldatt, %att);
}

#-> sub CPAN::InfoObj::id ;
sub id { shift->{'ID'} }

#-> sub CPAN::InfoObj::as_glimpse ;
sub as_glimpse {
    my($self) = @_;
    my(@m);
    my $class = ref($self);
    $class =~ s/^CPAN:://;
    push @m, sprintf "%-15s %s\n", $class, $self->{ID};
    join "", @m;
}

#-> sub CPAN::InfoObj::as_string ;
sub as_string {
    my($self) = @_;
    my(@m);
    my $class = ref($self);
    $class =~ s/^CPAN:://;
    push @m, $class, " id = $self->{ID}\n";
    for (sort keys %$self) {
	next if $_ eq 'ID';
	my $extra = "";
	if ($_ eq "CPAN_USERID") {
	  $extra .= " (".$self->author;
	  my $email; # old perls!
	  if ($email = $CPAN::META->instance(CPAN::Author,
						$self->{$_}
					       )->email) {
	    $extra .= " <$email>";
	  } else {
	    $extra .= " <no email>";
	  }
	  $extra .= ")";
	}
	if (ref($self->{$_}) eq "ARRAY") { # language interface? XXX
	    push @m, sprintf "    %-12s %s%s\n", $_, "@{$self->{$_}}", $extra;
	} else {
	    push @m, sprintf "    %-12s %s%s\n", $_, $self->{$_}, $extra;
	}
    }
    join "", @m, "\n";
}

#-> sub CPAN::InfoObj::author ;
sub author {
    my($self) = @_;
    $CPAN::META->instance(CPAN::Author,$self->{CPAN_USERID})->fullname;
}

package CPAN::Author;

#-> sub CPAN::Author::as_glimpse ;
sub as_glimpse {
    my($self) = @_;
    my(@m);
    my $class = ref($self);
    $class =~ s/^CPAN:://;
    push @m, sprintf "%-15s %s (%s)\n", $class, $self->{ID}, $self->fullname;
    join "", @m;
}

# Dead code, I would have liked to have,,, but it was never reached,,,
#sub make {
#    my($self) = @_;
#    return "Don't be silly, you can't make $self->{FULLNAME} ;-)\n";
#}

#-> sub CPAN::Author::fullname ;
sub fullname { shift->{'FULLNAME'} }
*name = \&fullname;

#-> sub CPAN::Author::email ;
sub email    { shift->{'EMAIL'} }

package CPAN::Distribution;

#-> sub CPAN::Distribution::called_for ;
sub called_for {
    my($self,$id) = @_;
    $self->{'CALLED_FOR'} = $id if defined $id;
    return $self->{'CALLED_FOR'};
}

#-> sub CPAN::Distribution::get ;
sub get {
    my($self) = @_;
  EXCUSE: {
	my @e;
	exists $self->{'build_dir'} and push @e,
	    "Unwrapped into directory $self->{'build_dir'}";
	$CPAN::Frontend->myprint(join "", map {"  $_\n"} @e) and return if @e;
    }
    my($local_file);
    my($local_wanted) =
	 MM->catfile(
			$CPAN::Config->{keep_source_where},
			"authors",
			"id",
			split("/",$self->{ID})
		       );

    $self->debug("Doing localize") if $CPAN::DEBUG;
    $local_file =
	CPAN::FTP->localize("authors/id/$self->{ID}", $local_wanted)
	    or $CPAN::Frontend->mydie("Giving up on '$local_wanted'\n");
    $self->{localfile} = $local_file;
    my $builddir = $CPAN::META->{cachemgr}->dir;
    $self->debug("doing chdir $builddir") if $CPAN::DEBUG;
    chdir $builddir or Carp::croak("Couldn't chdir $builddir: $!");
    my $packagedir;

    $self->debug("local_file[$local_file]") if $CPAN::DEBUG;
    if ($CPAN::META->has_inst('MD5')) {
	$self->debug("MD5 is installed, verifying");
	$self->verifyMD5;
    } else {
	$self->debug("MD5 is NOT installed");
    }
    $self->debug("Removing tmp") if $CPAN::DEBUG;
    File::Path::rmtree("tmp");
    mkdir "tmp", 0755 or Carp::croak "Couldn't mkdir tmp: $!";
    chdir "tmp";
    $self->debug("Changed directory to tmp") if $CPAN::DEBUG;
    if (! $local_file) {
	Carp::croak "bad download, can't do anything :-(\n";
    } elsif ($local_file =~ /(\.tar\.(gz|Z)|\.tgz)$/i){
	$self->untar_me($local_file);
    } elsif ( $local_file =~ /\.zip$/i ) {
	$self->unzip_me($local_file);
    } elsif ( $local_file =~ /\.pm\.(gz|Z)$/) {
	$self->pm2dir_me($local_file);
    } else {
	$self->{archived} = "NO";
    }
    chdir File::Spec->updir;
    if ($self->{archived} ne 'NO') {
	chdir File::Spec->catdir(File::Spec->curdir, "tmp");
	# Let's check if the package has its own directory.
	my $dh = DirHandle->new(File::Spec->curdir)
	    or Carp::croak("Couldn't opendir .: $!");
	my @readdir = grep $_ !~ /^\.\.?$/, $dh->read; ### MAC??
	$dh->close;
	my ($distdir,$packagedir);
	if (@readdir == 1 && -d $readdir[0]) {
	    $distdir = $readdir[0];
	    $packagedir = MM->catdir($builddir,$distdir);
	    -d $packagedir and $CPAN::Frontend->myprint("Removing previously used $packagedir\n");
	    File::Path::rmtree($packagedir);
	    rename($distdir,$packagedir) or Carp::confess("Couldn't rename $distdir to $packagedir: $!");
	} else {
	    my $pragmatic_dir = $self->{'CPAN_USERID'} . '000';
	    $pragmatic_dir =~ s/\W_//g;
	    $pragmatic_dir++ while -d "../$pragmatic_dir";
	    $packagedir = MM->catdir($builddir,$pragmatic_dir);
	    File::Path::mkpath($packagedir);
	    my($f);
	    for $f (@readdir) { # is already without "." and ".."
		my $to = MM->catdir($packagedir,$f);
		rename($f,$to) or Carp::confess("Couldn't rename $f to $to: $!");
	    }
	}
	$self->{'build_dir'} = $packagedir;
	chdir File::Spec->updir;

	$self->debug("Changed directory to .. (self is $self [".$self->as_string."])")
	    if $CPAN::DEBUG;
	File::Path::rmtree("tmp");
	if ($CPAN::Config->{keep_source_where} =~ /^no/i ){
	    $CPAN::Frontend->myprint("Going to unlink $local_file\n");
	    unlink $local_file or Carp::carp "Couldn't unlink $local_file";
	}
	my($makefilepl) = MM->catfile($packagedir,"Makefile.PL");
	unless (-f $makefilepl) {
	  my($configure) = MM->catfile($packagedir,"Configure");
	  if (-f $configure) {
	    # do we have anything to do?
	    $self->{'configure'} = $configure;
	  } elsif (-f MM->catfile($packagedir,"Makefile")) {
	    $CPAN::Frontend->myprint(qq{
Package comes with a Makefile and without a Makefile.PL.
We\'ll try to build it with that Makefile then.
});
	    $self->{writemakefile} = "YES";
	    sleep 2;
	  } else {
	    my $fh = FileHandle->new(">$makefilepl")
		or Carp::croak("Could not open >$makefilepl");
	    my $cf = $self->called_for || "unknown";
	    $fh->print(
qq{# This Makefile.PL has been autogenerated by the module CPAN.pm
# because there was no Makefile.PL supplied.
# Autogenerated on: }.scalar localtime().qq{

use ExtUtils::MakeMaker;
WriteMakefile(NAME => q[$cf]);

});
	    $CPAN::Frontend->myprint(qq{Package comes without Makefile.PL.
  Writing one on our own (calling it $cf)\n});
	    }
	}
    }
    return $self;
}

sub untar_me {
    my($self,$local_file) = @_;
    $self->{archived} = "tar";
    if (CPAN::Tarzip->untar($local_file)) {
	$self->{unwrapped} = "YES";
    } else {
	$self->{unwrapped} = "NO";
    }
}

sub unzip_me {
    my($self,$local_file) = @_;
    $self->{archived} = "zip";
    my $system = "$CPAN::Config->{unzip} $local_file";
    if (system($system) == 0) {
	$self->{unwrapped} = "YES";
    } else {
	$self->{unwrapped} = "NO";
    }
}

sub pm2dir_me {
    my($self,$local_file) = @_;
    $self->{archived} = "pm";
    my $to = File::Basename::basename($local_file);
    $to =~ s/\.(gz|Z)$//;
    if (CPAN::Tarzip->gunzip($local_file,$to)) {
	$self->{unwrapped} = "YES";
    } else {
	$self->{unwrapped} = "NO";
    }
}

#-> sub CPAN::Distribution::new ;
sub new {
    my($class,%att) = @_;

    $CPAN::META->{cachemgr} ||= CPAN::CacheMgr->new();

    my $this = { %att };
    return bless $this, $class;
}

#-> sub CPAN::Distribution::look ;
sub look {
    my($self) = @_;

    if ($^O eq 'MacOS') {
      $self->ExtUtils::MM_MacOS::look;
      return;
    }

    if (  $CPAN::Config->{'shell'} ) {
	$CPAN::Frontend->myprint(qq{
Trying to open a subshell in the build directory...
});
    } else {
	$CPAN::Frontend->myprint(qq{
Your configuration does not define a value for subshells.
Please define it with "o conf shell <your shell>"
});
	return;
    }
    my $dist = $self->id;
    my $dir  = $self->dir or $self->get;
    $dir = $self->dir;
    my $getcwd;
    $getcwd = $CPAN::Config->{'getcwd'} || 'cwd';
    my $pwd  = CPAN->$getcwd();
    chdir($dir);
    $CPAN::Frontend->myprint(qq{Working directory is $dir\n});
    system($CPAN::Config->{'shell'}) == 0
	or $CPAN::Frontend->mydie("Subprocess shell error");
    chdir($pwd);
}

#-> sub CPAN::Distribution::readme ;
sub readme {
    my($self) = @_;
    my($dist) = $self->id;
    my($sans,$suffix) = $dist =~ /(.+)\.(tgz|tar[\._-]gz|tar\.Z|zip)$/;
    $self->debug("sans[$sans] suffix[$suffix]\n") if $CPAN::DEBUG;
    my($local_file);
    my($local_wanted) =
	 MM->catfile(
			$CPAN::Config->{keep_source_where},
			"authors",
			"id",
			split("/","$sans.readme"),
		       );
    $self->debug("Doing localize") if $CPAN::DEBUG;
    $local_file = CPAN::FTP->localize("authors/id/$sans.readme",
				      $local_wanted)
	or $CPAN::Frontend->mydie(qq{No $sans.readme found});;

    if ($^O eq 'MacOS') {
        ExtUtils::MM_MacOS::launch_file($local_file);
        return;
    }

    my $fh_pager = FileHandle->new;
    local($SIG{PIPE}) = "IGNORE";
    $fh_pager->open("|$CPAN::Config->{'pager'}")
	or die "Could not open pager $CPAN::Config->{'pager'}: $!";
    my $fh_readme = FileHandle->new;
    $fh_readme->open($local_file)
	or $CPAN::Frontend->mydie(qq{Could not open "$local_file": $!});
    $CPAN::Frontend->myprint(qq{
Displaying file
  $local_file
with pager "$CPAN::Config->{'pager'}"
});
    sleep 2;
    $fh_pager->print(<$fh_readme>);
}

#-> sub CPAN::Distribution::verifyMD5 ;
sub verifyMD5 {
    my($self) = @_;
  EXCUSE: {
	my @e;
	$self->{MD5_STATUS} ||= "";
	$self->{MD5_STATUS} eq "OK" and push @e, "MD5 Checksum was ok";
	$CPAN::Frontend->myprint(join "", map {"  $_\n"} @e) and return if @e;
    }
    my($lc_want,$lc_file,@local,$basename);
    @local = split("/",$self->{ID});
    pop @local;
    push @local, "CHECKSUMS";
    $lc_want =
	MM->catfile($CPAN::Config->{keep_source_where},
		      "authors", "id", @local);
    local($") = "/";
    if (
	-s $lc_want
	&&
	$self->MD5_check_file($lc_want)
       ) {
	return $self->{MD5_STATUS} = "OK";
    }
    $lc_file = CPAN::FTP->localize("authors/id/@local",
				   $lc_want,1);
    unless ($lc_file) {
	$local[-1] .= ".gz";
	$lc_file = CPAN::FTP->localize("authors/id/@local",
				       "$lc_want.gz",1);
	if ($lc_file) {
	    $lc_file =~ s/\.gz$//;
	    CPAN::Tarzip->gunzip("$lc_file.gz",$lc_file);
	} else {
	    return;
	}
    }
    $self->MD5_check_file($lc_file);
}

#-> sub CPAN::Distribution::MD5_check_file ;
sub MD5_check_file {
    my($self,$chk_file) = @_;
    my($cksum,$file,$basename);
    $file = $self->{localfile};
    $basename = File::Basename::basename($file);
    my $fh = FileHandle->new;
    if (open $fh, $chk_file){
	local($/);
	my $eval = <$fh>;
	$eval =~ s/\015?\012/\n/g;
	close $fh;
	my($comp) = Safe->new();
	$cksum = $comp->reval($eval);
	if ($@) {
	    rename $chk_file, "$chk_file.bad";
	    Carp::confess($@) if $@;
	}
    } else {
	Carp::carp "Could not open $chk_file for reading";
    }

    if (exists $cksum->{$basename}{md5}) {
	$self->debug("Found checksum for $basename:" .
		     "$cksum->{$basename}{md5}\n") if $CPAN::DEBUG;

	open($fh, $file);
	binmode $fh;
	my $eq = $self->eq_MD5($fh,$cksum->{$basename}{'md5'});
	$fh->close;
	$fh = CPAN::Tarzip->TIEHANDLE($file);

	unless ($eq) {
	  # had to inline it, when I tied it, the tiedness got lost on
	  # the call to eq_MD5. (Jan 1998)
	  my $md5 = MD5->new;
	  my($data,$ref);
	  $ref = \$data;
	  while ($fh->READ($ref, 4096)){
	    $md5->add($data);
	  }
	  my $hexdigest = $md5->hexdigest;
	  $eq += $hexdigest eq $cksum->{$basename}{'md5-ungz'};
	}

	if ($eq) {
	  $CPAN::Frontend->myprint("Checksum for $file ok\n");
	  return $self->{MD5_STATUS} = "OK";
	} else {
	    $CPAN::Frontend->myprint(qq{Checksum mismatch for }.
				     qq{distribution file. }.
				     qq{Please investigate.\n\n}.
				     $self->as_string,
				     $CPAN::META->instance(
							   'CPAN::Author',
							   $self->{CPAN_USERID}
							  )->as_string);
	    my $wrap = qq{I\'d recommend removing $file. It seems to
be a bogus file. Maybe you have configured your \`urllist\' with a
bad URL. Please check this array with \`o conf urllist\', and
retry.};
	    $CPAN::Frontend->myprint(Text::Wrap::wrap("","",$wrap));
	    $CPAN::Frontend->myprint("\n\n");
	    sleep 3;
	    return;
	}
	# close $fh if fileno($fh);
    } else {
	$self->{MD5_STATUS} ||= "";
	if ($self->{MD5_STATUS} eq "NIL") {
	    $CPAN::Frontend->myprint(qq{
No md5 checksum for $basename in local $chk_file.
Removing $chk_file
});
	    unlink $chk_file or $CPAN::Frontend->myprint("Could not unlink: $!");
	    sleep 1;
	}
	$self->{MD5_STATUS} = "NIL";
	return;
    }
}

#-> sub CPAN::Distribution::eq_MD5 ;
sub eq_MD5 {
    my($self,$fh,$expectMD5) = @_;
    my $md5 = MD5->new;
    my($data);
    while (read($fh, $data, 4096)){
      $md5->add($data);
    }
    # $md5->addfile($fh);
    my $hexdigest = $md5->hexdigest;
    # warn "fh[$fh] hex[$hexdigest] aexp[$expectMD5]";
    $hexdigest eq $expectMD5;
}

#-> sub CPAN::Distribution::force ;
sub force {
  my($self) = @_;
  $self->{'force_update'}++;
  for my $att (qw(
  MD5_STATUS archived build_dir localfile make install unwrapped
  writemakefile have_sponsored
 )) {
    delete $self->{$att};
  }
}

sub isa_perl {
  my($self) = @_;
  my $file = File::Basename::basename($self->id);
  return unless $file =~ m{ ^ perl
			    (5)
			    ([._-])
			    (\d{3}(_[0-4][0-9])?)
			    \.tar[._-]gz
			    $
			  }x;
  "$1.$3";
}

#-> sub CPAN::Distribution::perl ;
sub perl {
    my($self) = @_;
    my($perl) = MM->file_name_is_absolute($^X) ? $^X : "";
    my $getcwd = $CPAN::Config->{'getcwd'} || 'cwd';
    my $pwd  = CPAN->$getcwd();
    my $candidate = MM->catfile($pwd,$^X);
    $perl ||= $candidate if MM->maybe_command($candidate);
    unless ($perl) {
	my ($component,$perl_name);
      DIST_PERLNAME: foreach $perl_name ($^X, 'perl', 'perl5', "perl$]") {
	    PATH_COMPONENT: foreach $component (MM->path(),
						$Config::Config{'binexp'}) {
		  next unless defined($component) && $component;
		  my($abs) = MM->catfile($component,$perl_name);
		  if (MM->maybe_command($abs)) {
		      $perl = $abs;
		      last DIST_PERLNAME;
		  }
	      }
	  }
    }
    $perl;
}

#-> sub CPAN::Distribution::make ;
sub make {
    my($self) = @_;
    $CPAN::Frontend->myprint(sprintf "Running make for %s\n", $self->id);
    # Emergency brake if they said install Pippi and get newest perl
    if ($self->isa_perl) {
      if (
	  $self->called_for ne $self->id && ! $self->{'force_update'}
	 ) {
	$CPAN::Frontend->mydie(sprintf qq{
The most recent version "%s" of the module "%s"
comes with the current version of perl (%s).
I\'ll build that only if you ask for something like
    force install %s
or
    install %s
},
			       $CPAN::META->instance(
						     'CPAN::Module',
						     $self->called_for
						    )->cpan_version,
			       $self->called_for,
			       $self->isa_perl,
			       $self->called_for,
			       $self->id);
      }
    }
    $self->get;
  EXCUSE: {
	my @e;
	$self->{archived} eq "NO" and push @e,
	"Is neither a tar nor a zip archive.";

	$self->{unwrapped} eq "NO" and push @e,
	"had problems unarchiving. Please build manually";

	exists $self->{writemakefile} &&
	    $self->{writemakefile} eq "NO" and push @e,
	    "Had some problem writing Makefile";

	defined $self->{'make'} and push @e,
	"Has already been processed within this session";

	$CPAN::Frontend->myprint(join "", map {"  $_\n"} @e) and return if @e;
    }
    $CPAN::Frontend->myprint("\n  CPAN.pm: Going to build ".$self->id."\n\n");
    my $builddir = $self->dir;
    chdir $builddir or Carp::croak("Couldn't chdir $builddir: $!");
    $self->debug("Changed directory to $builddir") if $CPAN::DEBUG;

    if ($^O eq 'MacOS') {
        ExtUtils::MM_MacOS::make($self);
        return;
    }

    my $system;
    if ($self->{'configure'}) {
      $system = $self->{'configure'};
    } else {
	my($perl) = $self->perl or die "Couldn\'t find executable perl\n";
	my $switch = "";
# This needs a handler that can be turned on or off:
#	$switch = "-MExtUtils::MakeMaker ".
#	    "-Mops=:default,:filesys_read,:filesys_open,require,chdir"
#	    if $] > 5.00310;
	$system = "$perl $switch Makefile.PL $CPAN::Config->{makepl_arg}";
    }
    unless (exists $self->{writemakefile}) {
	local($SIG{ALRM}) = sub { die "inactivity_timeout reached\n" };
	my($ret,$pid);
	$@ = "";
	if ($CPAN::Config->{inactivity_timeout}) {
	    eval {
		alarm $CPAN::Config->{inactivity_timeout};
		local $SIG{CHLD}; # = sub { wait };
		if (defined($pid = fork)) {
		    if ($pid) { #parent
			# wait;
			waitpid $pid, 0;
		    } else {    #child
		      # note, this exec isn't necessary if
		      # inactivity_timeout is 0. On the Mac I'd
		      # suggest, we set it always to 0.
		      exec $system;
		    }
		} else {
		    $CPAN::Frontend->myprint("Cannot fork: $!");
		    return;
		}
	    };
	    alarm 0;
	    if ($@){
		kill 9, $pid;
		waitpid $pid, 0;
		$CPAN::Frontend->myprint($@);
		$self->{writemakefile} = "NO - $@";
		$@ = "";
		return;
	    }
	} else {
	  $ret = system($system);
	  if ($ret != 0) {
	    $self->{writemakefile} = "NO";
	    return;
	  }
	}
	$self->{writemakefile} = "YES";
    }
    return if $CPAN::Signal;
    if (my @prereq = $self->needs_prereq){
      my $id = $self->id;
      $CPAN::Frontend->myprint("---- Dependencies detected ".
			       "during [$id] -----\n");

      for my $p (@prereq) {
	$CPAN::Frontend->myprint("    $p\n");
      }
      my $follow = 0;
      if ($CPAN::Config->{prerequisites_policy} eq "follow") {
	$follow = 1;
      } elsif ($CPAN::Config->{prerequisites_policy} eq "ask") {
	require ExtUtils::MakeMaker;
	my $answer = ExtUtils::MakeMaker::prompt(
"Shall I follow them and prepend them to the queue
of modules we are processing right now?", "yes");
	$follow = $answer =~ /^\s*y/i;
      } else {
	local($") = ", ";
	$CPAN::Frontend->myprint("  Ignoring dependencies on modules @prereq\n");
      }
      if ($follow) {
	CPAN::Queue->jumpqueue(@prereq,$id); # requeue yourself
	return;
      }
    }
    $system = join " ", $CPAN::Config->{'make'}, $CPAN::Config->{make_arg};
    if (system($system) == 0) {
	 $CPAN::Frontend->myprint("  $system -- OK\n");
	 $self->{'make'} = "YES";
    } else {
	 $self->{writemakefile} = "YES";
	 $self->{'make'} = "NO";
	 $CPAN::Frontend->myprint("  $system -- NOT OK\n");
    }
}

#-> sub CPAN::Distribution::needs_prereq ;
sub needs_prereq {
  my($self) = @_;
  return unless -f "Makefile"; # we cannot say much
  my $fh = FileHandle->new("<Makefile") or
      $CPAN::Frontend->mydie("Couldn't open Makefile: $!");
  local($/) = "\n";

  my(@p,@need);
  while (<$fh>) {
    last if /MakeMaker post_initialize section/;
    my($p) = m{^[\#]
		 \s+PREREQ_PM\s+=>\s+(.+)
		 }x;
    next unless $p;
    # warn "Found prereq expr[$p]";

    while ( $p =~ m/(?:\s)([\w\:]+)=>q\[.*?\],?/g ){
      push @p, $1;
    }
    last;
  }
  for my $p (@p) {
    my $mo = $CPAN::META->instance("CPAN::Module",$p);
    next if $mo->uptodate;
    # it's not needed, so don't push it. We cannot omit this step, because
    # if 'force' is in effect, nobody else will check.
    if ($self->{'have_sponsored'}{$p}++){
      # We have already sponsored it and for some reason it's still
      # not available. So we do nothing. Or what should we do?
      # if we push it again, we have a potential infinite loop
      next;
    }
    push @need, $p;
  }
  return @need;
}

#-> sub CPAN::Distribution::test ;
sub test {
    my($self) = @_;
    $self->make;
    return if $CPAN::Signal;
    $CPAN::Frontend->myprint("Running make test\n");
  EXCUSE: {
	my @e;
	exists $self->{'make'} or push @e,
	"Make had some problems, maybe interrupted? Won't test";

	exists $self->{'make'} and
	    $self->{'make'} eq 'NO' and
		push @e, "Oops, make had returned bad status";

	exists $self->{'build_dir'} or push @e, "Has no own directory";
	$CPAN::Frontend->myprint(join "", map {"  $_\n"} @e) and return if @e;
    }
    chdir $self->{'build_dir'} or
	Carp::croak("Couldn't chdir to $self->{'build_dir'}");
    $self->debug("Changed directory to $self->{'build_dir'}")
	if $CPAN::DEBUG;

    if ($^O eq 'MacOS') {
        ExtUtils::MM_MacOS::make_test($self);
        return;
    }

    my $system = join " ", $CPAN::Config->{'make'}, "test";
    if (system($system) == 0) {
	 $CPAN::Frontend->myprint("  $system -- OK\n");
	 $self->{'make_test'} = "YES";
    } else {
	 $self->{'make_test'} = "NO";
	 $CPAN::Frontend->myprint("  $system -- NOT OK\n");
    }
}

#-> sub CPAN::Distribution::clean ;
sub clean {
    my($self) = @_;
    $CPAN::Frontend->myprint("Running make clean\n");
  EXCUSE: {
	my @e;
	exists $self->{'build_dir'} or push @e, "Has no own directory";
	$CPAN::Frontend->myprint(join "", map {"  $_\n"} @e) and return if @e;
    }
    chdir $self->{'build_dir'} or
	Carp::croak("Couldn't chdir to $self->{'build_dir'}");
    $self->debug("Changed directory to $self->{'build_dir'}") if $CPAN::DEBUG;

    if ($^O eq 'MacOS') {
        ExtUtils::MM_MacOS::make_clean($self);
        return;
    }

    my $system = join " ", $CPAN::Config->{'make'}, "clean";
    if (system($system) == 0) {
	$CPAN::Frontend->myprint("  $system -- OK\n");
	$self->force;
    } else {
	# Hmmm, what to do if make clean failed?
    }
}

#-> sub CPAN::Distribution::install ;
sub install {
    my($self) = @_;
    $self->test;
    return if $CPAN::Signal;
    $CPAN::Frontend->myprint("Running make install\n");
  EXCUSE: {
	my @e;
	exists $self->{'build_dir'} or push @e, "Has no own directory";

	exists $self->{'make'} or push @e,
	"Make had some problems, maybe interrupted? Won't install";

	exists $self->{'make'} and
	    $self->{'make'} eq 'NO' and
		push @e, "Oops, make had returned bad status";

	push @e, "make test had returned bad status, ".
	    "won't install without force"
	    if exists $self->{'make_test'} and
	    $self->{'make_test'} eq 'NO' and
	    ! $self->{'force_update'};

	exists $self->{'install'} and push @e,
	$self->{'install'} eq "YES" ?
	    "Already done" : "Already tried without success";

	$CPAN::Frontend->myprint(join "", map {"  $_\n"} @e) and return if @e;
    }
    chdir $self->{'build_dir'} or
	Carp::croak("Couldn't chdir to $self->{'build_dir'}");
    $self->debug("Changed directory to $self->{'build_dir'}")
	if $CPAN::DEBUG;

    if ($^O eq 'MacOS') {
        ExtUtils::MM_MacOS::make_install($self);
        return;
    }

    my $system = join(" ", $CPAN::Config->{'make'},
		      "install", $CPAN::Config->{make_install_arg});
    my($stderr) = $^O =~ /Win/i ? "" : " 2>&1 ";
    my($pipe) = FileHandle->new("$system $stderr |");
    my($makeout) = "";
    while (<$pipe>){
	$CPAN::Frontend->myprint($_);
	$makeout .= $_;
    }
    $pipe->close;
    if ($?==0) {
	 $CPAN::Frontend->myprint("  $system -- OK\n");
	 return $self->{'install'} = "YES";
    } else {
	 $self->{'install'} = "NO";
	 $CPAN::Frontend->myprint("  $system -- NOT OK\n");
	 if ($makeout =~ /permission/s && $> > 0) {
	     $CPAN::Frontend->myprint(qq{    You may have to su }.
				      qq{to root to install the package\n});
	 }
    }
}

#-> sub CPAN::Distribution::dir ;
sub dir {
    shift->{'build_dir'};
}

package CPAN::Bundle;

#-> sub CPAN::Bundle::as_string ;
sub as_string {
    my($self) = @_;
    $self->contains;
    $self->{INST_VERSION} = $self->inst_version;
    return $self->SUPER::as_string;
}

#-> sub CPAN::Bundle::contains ;
sub contains {
  my($self) = @_;
  my($parsefile) = $self->inst_file;
  my($id) = $self->id;
  $self->debug("parsefile[$parsefile]id[$id]") if $CPAN::DEBUG;
  unless ($parsefile) {
    # Try to get at it in the cpan directory
    $self->debug("no parsefile") if $CPAN::DEBUG;
    Carp::confess "I don't know a $id" unless $self->{CPAN_FILE};
    my $dist = $CPAN::META->instance('CPAN::Distribution',
				     $self->{CPAN_FILE});
    $dist->get;
    $self->debug($dist->as_string) if $CPAN::DEBUG;
    my($todir) = $CPAN::Config->{'cpan_home'};
    my(@me,$from,$to,$me);
    @me = split /::/, $self->id;
    $me[-1] .= ".pm";
    $me = MM->catfile(@me);
    $from = $self->find_bundle_file($dist->{'build_dir'},$me);
    $to = MM->catfile($todir,$me);
    File::Path::mkpath(File::Basename::dirname($to));
    File::Copy::copy($from, $to)
	or Carp::confess("Couldn't copy $from to $to: $!");
    $parsefile = $to;
  }
  my @result;
  my $fh = FileHandle->new;
  local $/ = "\n";
  open($fh,$parsefile) or die "Could not open '$parsefile': $!";
  my $inpod = 0;
  $self->debug("parsefile[$parsefile]") if $CPAN::DEBUG;
  while (<$fh>) {
    $inpod = m/^=(?!head1\s+CONTENTS)/ ? 0 :
	m/^=head1\s+CONTENTS/ ? 1 : $inpod;
    next unless $inpod;
    next if /^=/;
    next if /^\s+$/;
    chomp;
    push @result, (split " ", $_, 2)[0];
  }
  close $fh;
  delete $self->{STATUS};
  $self->{CONTAINS} = join ", ", @result;
  $self->debug("CONTAINS[@result]") if $CPAN::DEBUG;
  unless (@result) {
    $CPAN::Frontend->mywarn(qq{
The bundle file "$parsefile" may be a broken
bundlefile. It seems not to contain any bundle definition.
Please check the file and if it is bogus, please delete it.
Sorry for the inconvenience.
});
  }
  @result;
}

#-> sub CPAN::Bundle::find_bundle_file
sub find_bundle_file {
    my($self,$where,$what) = @_;
    $self->debug("where[$where]what[$what]") if $CPAN::DEBUG;
### The following two lines let CPAN.pm become Bundle/CPAN.pm :-(
###    my $bu = MM->catfile($where,$what);
###    return $bu if -f $bu;
    my $manifest = MM->catfile($where,"MANIFEST");
    unless (-f $manifest) {
	require ExtUtils::Manifest;
	my $getcwd = $CPAN::Config->{'getcwd'} || 'cwd';
	my $cwd = CPAN->$getcwd();
	chdir $where;
	ExtUtils::Manifest::mkmanifest();
	chdir $cwd;
    }
    my $fh = FileHandle->new($manifest)
	or Carp::croak("Couldn't open $manifest: $!");
    local($/) = "\n";
    my $what2 = $what;
    if ($^O eq 'MacOS') {
      $what =~ s/^://;
      $what2 =~ tr|:|/|;
      $what2 =~ s/:Bundle://;
      $what2 =~ tr|:|/|;
    } else {
	$what2 =~ s|Bundle/||;
    }
    my $bu;
    while (<$fh>) {
	next if /^\s*\#/;
	my($file) = /(\S+)/;
	if ($file =~ m|\Q$what\E$|) {
	    $bu = $file;
	    # return MM->catfile($where,$bu); # bad
	    last;
	}
	# retry if she managed to
	# have no Bundle directory
	$bu = $file if $file =~ m|\Q$what2\E$|;
    }
    $bu =~ tr|/|:| if $^O eq 'MacOS';
    return MM->catfile($where, $bu) if $bu;
    Carp::croak("Couldn't find a Bundle file in $where");
}

#-> sub CPAN::Bundle::inst_file ;
sub inst_file {
    my($self) = @_;
    my($me,$inst_file);
    ($me = $self->id) =~ s/.*://;
##    my(@me,$inst_file);
##    @me = split /::/, $self->id;
##    $me[-1] .= ".pm";
    $inst_file = MM->catfile($CPAN::Config->{'cpan_home'},
				      "Bundle", "$me.pm");
##				      "Bundle", @me);
    return $self->{'INST_FILE'} = $inst_file if -f $inst_file;
#    $inst_file =
    $self->SUPER::inst_file;
#    return $self->{'INST_FILE'} = $inst_file if -f $inst_file;
#    return $self->{'INST_FILE'}; # even if undefined?
}

#-> sub CPAN::Bundle::rematein ;
sub rematein {
    my($self,$meth) = @_;
    $self->debug("self[$self] meth[$meth]") if $CPAN::DEBUG;
    my($id) = $self->id;
    Carp::croak "Can't $meth $id, don't have an associated bundle file. :-(\n"
	unless $self->inst_file || $self->{CPAN_FILE};
    my($s,%fail);
    for $s ($self->contains) {
	my($type) = $s =~ m|/| ? 'CPAN::Distribution' :
	    $s =~ m|^Bundle::| ? 'CPAN::Bundle' : 'CPAN::Module';
	if ($type eq 'CPAN::Distribution') {
	    $CPAN::Frontend->mywarn(qq{
The Bundle }.$self->id.qq{ contains
explicitly a file $s.
});
	    sleep 3;
	}
	# possibly noisy action:
	my $obj = $CPAN::META->instance($type,$s);
	$obj->$meth();
	my $success = $obj->can("uptodate") ? $obj->uptodate : 0;
	$success ||= $obj->{'install'} && $obj->{'install'} eq "YES";
	$fail{$s} = 1 unless $success;
    }
    # recap with less noise
    if ( $meth eq "install") {
	if (%fail) {
	    $CPAN::Frontend->myprint(qq{\nBundle summary: }.
				     qq{The following items seem to }.
				     qq{have had installation problems:\n});
	    for $s ($self->contains) {
		$CPAN::Frontend->myprint( "$s " ) if $fail{$s};
	    }
	    $CPAN::Frontend->myprint(qq{\n});
	} else {
	    $self->{'install'} = 'YES';
	}
    }
}

#sub CPAN::Bundle::xs_file
sub xs_file {
    # If a bundle contains another that contains an xs_file we have
    # here, we just don't bother I suppose
    return 0;
}

#-> sub CPAN::Bundle::force ;
sub force   { shift->rematein('force',@_); }
#-> sub CPAN::Bundle::get ;
sub get     { shift->rematein('get',@_); }
#-> sub CPAN::Bundle::make ;
sub make    { shift->rematein('make',@_); }
#-> sub CPAN::Bundle::test ;
sub test    { shift->rematein('test',@_); }
#-> sub CPAN::Bundle::install ;
sub install {
  my $self = shift;
  $self->rematein('install',@_);
}
#-> sub CPAN::Bundle::clean ;
sub clean   { shift->rematein('clean',@_); }

#-> sub CPAN::Bundle::readme ;
sub readme  {
    my($self) = @_;
    my($file) = $self->cpan_file or $CPAN::Frontend->myprint(qq{
No File found for bundle } . $self->id . qq{\n}), return;
    $self->debug("self[$self] file[$file]") if $CPAN::DEBUG;
    $CPAN::META->instance('CPAN::Distribution',$file)->readme;
}

package CPAN::Module;

#-> sub CPAN::Module::as_glimpse ;
sub as_glimpse {
    my($self) = @_;
    my(@m);
    my $class = ref($self);
    $class =~ s/^CPAN:://;
    push @m, sprintf("%-15s %-15s (%s)\n", $class, $self->{ID},
		     $self->cpan_file);
    join "", @m;
}

#-> sub CPAN::Module::as_string ;
sub as_string {
    my($self) = @_;
    my(@m);
    CPAN->debug($self) if $CPAN::DEBUG;
    my $class = ref($self);
    $class =~ s/^CPAN:://;
    local($^W) = 0;
    push @m, $class, " id = $self->{ID}\n";
    my $sprintf = "    %-12s %s\n";
    push @m, sprintf($sprintf, 'DESCRIPTION', $self->{description})
	if $self->{description};
    my $sprintf2 = "    %-12s %s (%s)\n";
    my($userid);
    if ($userid = $self->{'CPAN_USERID'} || $self->{'userid'}){
	my $author;
	if ($author = CPAN::Shell->expand('Author',$userid)) {
	  my $email = "";
	  my $m; # old perls
	  if ($m = $author->email) {
            $email = " <$m>";
          }
	  push @m, sprintf(
			   $sprintf2,
			   'CPAN_USERID',
			   $userid,
			   $author->fullname . $email
			  );
	}
    }
    push @m, sprintf($sprintf, 'CPAN_VERSION', $self->{CPAN_VERSION})
	if $self->{CPAN_VERSION};
    push @m, sprintf($sprintf, 'CPAN_FILE', $self->{CPAN_FILE})
	if $self->{CPAN_FILE};
    my $sprintf3 = "    %-12s %1s%1s%1s%1s (%s,%s,%s,%s)\n";
    my(%statd,%stats,%statl,%stati);
    @statd{qw,? i c a b R M S,} = qw,unknown idea
	pre-alpha alpha beta released mature standard,;
    @stats{qw,? m d u n,}       = qw,unknown mailing-list
	developer comp.lang.perl.* none,;
    @statl{qw,? p c + o h,}       = qw,unknown perl C C++ other hybrid,;
    @stati{qw,? f r O h,}         = qw,unknown functions
	references+ties object-oriented hybrid,;
    $statd{' '} = 'unknown';
    $stats{' '} = 'unknown';
    $statl{' '} = 'unknown';
    $stati{' '} = 'unknown';
    push @m, sprintf(
		     $sprintf3,
		     'DSLI_STATUS',
		     $self->{statd},
		     $self->{stats},
		     $self->{statl},
		     $self->{stati},
		     $statd{$self->{statd}},
		     $stats{$self->{stats}},
		     $statl{$self->{statl}},
		     $stati{$self->{stati}}
		    ) if $self->{statd};
    my $local_file = $self->inst_file;
    if ($local_file) {
      $self->{MANPAGE} ||= $self->manpage_headline($local_file);
    }
    my($item);
    for $item (qw/MANPAGE CONTAINS/) {
	push @m, sprintf($sprintf, $item, $self->{$item})
	    if exists $self->{$item};
    }
    push @m, sprintf($sprintf, 'INST_FILE',
		     $local_file || "(not installed)");
    push @m, sprintf($sprintf, 'INST_VERSION',
		     $self->inst_version) if $local_file;
    join "", @m, "\n";
}

sub manpage_headline {
  my($self,$local_file) = @_;
  my(@local_file) = $local_file;
  $local_file =~ s/\.pm$/.pod/;
  push @local_file, $local_file;
  my(@result,$locf);
  for $locf (@local_file) {
    next unless -f $locf;
    my $fh = FileHandle->new($locf)
	or $Carp::Frontend->mydie("Couldn't open $locf: $!");
    my $inpod = 0;
    local $/ = "\n";
    while (<$fh>) {
      $inpod = m/^=(?!head1\s+NAME)/ ? 0 :
	  m/^=head1\s+NAME/ ? 1 : $inpod;
      next unless $inpod;
      next if /^=/;
      next if /^\s+$/;
      chomp;
      push @result, $_;
    }
    close $fh;
    last if @result;
  }
  join " ", @result;
}

#-> sub CPAN::Module::cpan_file ;
sub cpan_file    {
    my $self = shift;
    CPAN->debug($self->id) if $CPAN::DEBUG;
    unless (defined $self->{'CPAN_FILE'}) {
	CPAN::Index->reload;
    }
    if (exists $self->{'CPAN_FILE'} && defined $self->{'CPAN_FILE'}){
	return $self->{'CPAN_FILE'};
    } elsif (exists $self->{'userid'} && defined $self->{'userid'}) {
	my $fullname = $CPAN::META->instance(CPAN::Author,
				      $self->{'userid'})->fullname;
	my $email = $CPAN::META->instance(CPAN::Author,
				      $self->{'userid'})->email;
	unless (defined $fullname && defined $email) {
	    return "Contact Author $self->{userid} (Try ``a $self->{userid}'')";
	}
	return "Contact Author $fullname <$email>";
    } else {
	return "N/A";
    }
}

*name = \&cpan_file;

#-> sub CPAN::Module::cpan_version ;
sub cpan_version {
    my $self = shift;
    $self->{'CPAN_VERSION'} = 'undef'
	unless defined $self->{'CPAN_VERSION'}; # I believe this is
                                                # always a bug in the
                                                # index and should be
                                                # reported as such,
                                                # but usually I find
                                                # out such an error
                                                # and do not want to
                                                # provoke too many
                                                # bugreports
    $self->{'CPAN_VERSION'};
}

#-> sub CPAN::Module::force ;
sub force {
    my($self) = @_;
    $self->{'force_update'}++;
}

#-> sub CPAN::Module::rematein ;
sub rematein {
    my($self,$meth) = @_;
    $self->debug($self->id) if $CPAN::DEBUG;
    my $cpan_file = $self->cpan_file;
    if ($cpan_file eq "N/A" || $cpan_file =~ /^Contact Author/){
      $CPAN::Frontend->mywarn(sprintf qq{
  The module %s isn\'t available on CPAN.

  Either the module has not yet been uploaded to CPAN, or it is
  temporary unavailable. Please contact the author to find out
  more about the status. Try ``i %s''.
},
			      $self->id,
			      $self->id,
			     );
      return;
    }
    my $pack = $CPAN::META->instance('CPAN::Distribution',$cpan_file);
    $pack->called_for($self->id);
    $pack->force if exists $self->{'force_update'};
    $pack->$meth();
    delete $self->{'force_update'};
}

#-> sub CPAN::Module::readme ;
sub readme { shift->rematein('readme') }
#-> sub CPAN::Module::look ;
sub look { shift->rematein('look') }
#-> sub CPAN::Module::get ;
sub get    { shift->rematein('get',@_); }
#-> sub CPAN::Module::make ;
sub make   { shift->rematein('make') }
#-> sub CPAN::Module::test ;
sub test   { shift->rematein('test') }
#-> sub CPAN::Module::uptodate ;
sub uptodate {
    my($self) = @_;
    my($latest) = $self->cpan_version;
    $latest ||= 0;
    my($inst_file) = $self->inst_file;
    my($have) = 0;
    if (defined $inst_file) {
	$have = $self->inst_version;
    }
    local($^W)=0;
    if ($inst_file
	&&
	$have >= $latest
       ) {
      return 1;
    }
    return;
}
#-> sub CPAN::Module::install ;
sub install {
    my($self) = @_;
    my($doit) = 0;
    if ($self->uptodate
	&&
	not exists $self->{'force_update'}
       ) {
	$CPAN::Frontend->myprint( $self->id. " is up to date.\n");
    } else {
	$doit = 1;
    }
    $self->rematein('install') if $doit;
}
#-> sub CPAN::Module::clean ;
sub clean  { shift->rematein('clean') }

#-> sub CPAN::Module::inst_file ;
sub inst_file {
    my($self) = @_;
    my($dir,@packpath);
    @packpath = split /::/, $self->{ID};
    $packpath[-1] .= ".pm";
    foreach $dir (@INC) {
	my $pmfile = MM->catfile($dir,@packpath);
	if (-f $pmfile){
	    return $pmfile;
	}
    }
    return;
}

#-> sub CPAN::Module::xs_file ;
sub xs_file {
    my($self) = @_;
    my($dir,@packpath);
    @packpath = split /::/, $self->{ID};
    push @packpath, $packpath[-1];
    $packpath[-1] .= "." . $Config::Config{'dlext'};
    foreach $dir (@INC) {
	my $xsfile = MM->catfile($dir,'auto',@packpath);
	if (-f $xsfile){
	    return $xsfile;
	}
    }
    return;
}

#-> sub CPAN::Module::inst_version ;
sub inst_version {
    my($self) = @_;
    my $parsefile = $self->inst_file or return;
    local($^W) = 0 if $] < 5.00303 && $ExtUtils::MakeMaker::VERSION < 5.38;
    # warn "HERE";
    my $have = MM->parse_version($parsefile) || "undef";
    $have =~ s/\s+//g;
    $have;
}

package CPAN::Tarzip;

sub gzip {
  my($class,$read,$write) = @_;
  if ($CPAN::META->has_inst("Compress::Zlib")) {
    my($buffer,$fhw);
    $fhw = FileHandle->new($read)
	or $CPAN::Frontend->mydie("Could not open $read: $!");
    my $gz = Compress::Zlib::gzopen($write, "wb")
	or $CPAN::Frontend->mydie("Cannot gzopen $write: $!\n");
    $gz->gzwrite($buffer)
	while read($fhw,$buffer,4096) > 0 ;
    $gz->gzclose() ;
    $fhw->close;
    return 1;
  } else {
    system("$CPAN::Config->{'gzip'} -c $read > $write")==0;
  }
}

sub gunzip {
  my($class,$read,$write) = @_;
  if ($CPAN::META->has_inst("Compress::Zlib")) {
    my($buffer,$fhw);
    $fhw = FileHandle->new(">$write")
	or $CPAN::Frontend->mydie("Could not open >$write: $!");
    my $gz = Compress::Zlib::gzopen($read, "rb")
	or $CPAN::Frontend->mydie("Cannot gzopen $read: $!\n");
    $fhw->print($buffer)
	while $gz->gzread($buffer) > 0 ;
    $CPAN::Frontend->mydie("Error reading from $read: $!\n")
	if $gz->gzerror != Compress::Zlib::Z_STREAM_END();
    $gz->gzclose() ;
    $fhw->close;
    return 1;
  } else {
    system("$CPAN::Config->{'gzip'} -dc $read > $write")==0;
  }
}

sub gtest {
  my($class,$read) = @_;
  if ($CPAN::META->has_inst("Compress::Zlib")) {
    my($buffer);
    my $gz = Compress::Zlib::gzopen($read, "rb")
	or $CPAN::Frontend->mydie("Cannot open $read: $!\n");
    1 while $gz->gzread($buffer) > 0 ;
    $CPAN::Frontend->mydie("Error reading from $read: $!\n")
	if $gz->gzerror != Compress::Zlib::Z_STREAM_END();
    $gz->gzclose() ;
    return 1;
  } else {
    return system("$CPAN::Config->{'gzip'} -dt $read")==0;
  }
}

sub TIEHANDLE {
  my($class,$file) = @_;
  my $ret;
  $class->debug("file[$file]");
  if ($CPAN::META->has_inst("Compress::Zlib")) {
    my $gz = Compress::Zlib::gzopen($file,"rb") or
	die "Could not gzopen $file";
    $ret = bless {GZ => $gz}, $class;
  } else {
    my $pipe = "$CPAN::Config->{'gzip'} --decompress --stdout $file |";
    my $fh = FileHandle->new($pipe) or die "Could pipe[$pipe]: $!";
    binmode $fh;
    $ret = bless {FH => $fh}, $class;
  }
  $ret;
}

sub READLINE {
  my($self) = @_;
  if (exists $self->{GZ}) {
    my $gz = $self->{GZ};
    my($line,$bytesread);
    $bytesread = $gz->gzreadline($line);
    return undef if $bytesread == 0;
    return $line;
  } else {
    my $fh = $self->{FH};
    return scalar <$fh>;
  }
}

sub READ {
  my($self,$ref,$length,$offset) = @_;
  die "read with offset not implemented" if defined $offset;
  if (exists $self->{GZ}) {
    my $gz = $self->{GZ};
    my $byteread = $gz->gzread($$ref,$length);# 30eaf79e8b446ef52464b5422da328a8
    return $byteread;
  } else {
    my $fh = $self->{FH};
    return read($fh,$$ref,$length);
  }
}

sub DESTROY {
  my($self) = @_;
  if (exists $self->{GZ}) {
    my $gz = $self->{GZ};
    $gz->gzclose();
  } else {
    my $fh = $self->{FH};
    $fh->close;
  }
  undef $self;
}

sub untar {
  my($class,$file) = @_;
  # had to disable, because version 0.07 seems to be buggy
  if (MM->maybe_command($CPAN::Config->{'gzip'})
      &&
      MM->maybe_command($CPAN::Config->{'tar'})) {
    if ($^O =~ /win/i) { # irgggh
	# people find the most curious tar binaries that cannot handle
	# pipes
	my $system = "$CPAN::Config->{'gzip'} --decompress $file";
	if (system($system)==0) {
	    $CPAN::Frontend->myprint(qq{Uncompressed $file successfully\n});
	} else {
	    $CPAN::Frontend->mydie(
				   qq{Couldn\'t uncompress $file\n}
				  );
	}
	$file =~ s/\.gz$//;
	$system = "$CPAN::Config->{tar} xvf $file";
	if (system($system)==0) {
	    $CPAN::Frontend->myprint(qq{Untarred $file successfully\n});
	} else {
	    $CPAN::Frontend->mydie(qq{Couldn\'t untar $file\n});
	}
	return 1;
    } else {
	my $system = "$CPAN::Config->{'gzip'} --decompress --stdout " .
	    "< $file | $CPAN::Config->{tar} xvf -";
	return system($system) == 0;
    }
  } elsif ($CPAN::META->has_inst("Archive::Tar")
      &&
      $CPAN::META->has_inst("Compress::Zlib") ) {
    my $tar = Archive::Tar->new($file,1);
    $tar->extract($tar->list_files); # I'm pretty sure we have nothing
                                     # that isn't compressed

    ExtUtils::MM_MacOS::convert_files([$tar->list_files], 1)
        if ($^O eq 'MacOS');

    return 1;
  } else {
    $CPAN::Frontend->mydie(qq{
CPAN.pm needs either both external programs tar and gzip installed or
both the modules Archive::Tar and Compress::Zlib. Neither prerequisite
is available. Can\'t continue.
});
  }
}

package CPAN;

1;

__END__

=head1 NAME

CPAN - query, download and build perl modules from CPAN sites

=head1 SYNOPSIS

Interactive mode:

  perl -MCPAN -e shell;

Batch mode:

  use CPAN;

  autobundle, clean, install, make, recompile, test

=head1 DESCRIPTION

The CPAN module is designed to automate the make and install of perl
modules and extensions. It includes some searching capabilities and
knows how to use Net::FTP or LWP (or lynx or an external ftp client)
to fetch the raw data from the net.

Modules are fetched from one or more of the mirrored CPAN
(Comprehensive Perl Archive Network) sites and unpacked in a dedicated
directory.

The CPAN module also supports the concept of named and versioned
'bundles' of modules. Bundles simplify the handling of sets of
related modules. See BUNDLES below.

The package contains a session manager and a cache manager. There is
no status retained between sessions. The session manager keeps track
of what has been fetched, built and installed in the current
session. The cache manager keeps track of the disk space occupied by
the make processes and deletes excess space according to a simple FIFO
mechanism.

For extended searching capabilities there's a plugin for CPAN available,
L<CPAN::WAIT>. C<CPAN::WAIT> is a full-text search engine that indexes
all documents available in CPAN authors directories. If C<CPAN::WAIT>
is installed on your system, the interactive shell of <CPAN.pm> will
enable the C<wq>, C<wr>, C<wd>, C<wl>, and C<wh> commands which send
queries to the WAIT server that has been configured for your
installation.

All other methods provided are accessible in a programmer style and in an
interactive shell style.

=head2 Interactive Mode

The interactive mode is entered by running

    perl -MCPAN -e shell

which puts you into a readline interface. You will have the most fun if
you install Term::ReadKey and Term::ReadLine to enjoy both history and
command completion.

Once you are on the command line, type 'h' and the rest should be
self-explanatory.

The most common uses of the interactive modes are

=over 2

=item Searching for authors, bundles, distribution files and modules

There are corresponding one-letter commands C<a>, C<b>, C<d>, and C<m>
for each of the four categories and another, C<i> for any of the
mentioned four. Each of the four entities is implemented as a class
with slightly differing methods for displaying an object.

Arguments you pass to these commands are either strings exactly matching
the identification string of an object or regular expressions that are
then matched case-insensitively against various attributes of the
objects. The parser recognizes a regular expression only if you
enclose it between two slashes.

The principle is that the number of found objects influences how an
item is displayed. If the search finds one item, the result is displayed
as object-E<gt>as_string, but if we find more than one, we display
each as object-E<gt>as_glimpse. E.g.

    cpan> a ANDK
    Author id = ANDK
	EMAIL        a.koenig@franz.ww.TU-Berlin.DE
	FULLNAME     Andreas König


    cpan> a /andk/
    Author id = ANDK
	EMAIL        a.koenig@franz.ww.TU-Berlin.DE
	FULLNAME     Andreas König


    cpan> a /and.*rt/
    Author          ANDYD (Andy Dougherty)
    Author          MERLYN (Randal L. Schwartz)

=item make, test, install, clean  modules or distributions

These commands take any number of arguments and investigates what is
necessary to perform the action. If the argument is a distribution
file name (recognized by embedded slashes), it is processed. If it is
a module, CPAN determines the distribution file in which this module
is included and processes that, following any dependencies named in
the module's Makefile.PL (this behavior is controlled by
I<prerequisites_policy>.)

Any C<make> or C<test> are run unconditionally. An

  install <distribution_file>

also is run unconditionally. But for

  install <module>

CPAN checks if an install is actually needed for it and prints
I<module up to date> in the case that the distribution file containing
the module doesnE<39>t need to be updated.

CPAN also keeps track of what it has done within the current session
and doesnE<39>t try to build a package a second time regardless if it
succeeded or not. The C<force> command takes as a first argument the
method to invoke (currently: C<make>, C<test>, or C<install>) and executes the
command from scratch.

Example:

    cpan> install OpenGL
    OpenGL is up to date.
    cpan> force install OpenGL
    Running make
    OpenGL-0.4/
    OpenGL-0.4/COPYRIGHT
    [...]

A C<clean> command results in a

  make clean

being executed within the distribution file's working directory.

=item readme, look module or distribution

These two commands take only one argument, be it a module or a
distribution file. C<readme> unconditionally runs, displaying the
README of the associated distribution file. C<Look> gets and
untars (if not yet done) the distribution file, changes to the
appropriate directory and opens a subshell process in that directory.

=item Signals

CPAN.pm installs signal handlers for SIGINT and SIGTERM. While you are
in the cpan-shell it is intended that you can press C<^C> anytime and
return to the cpan-shell prompt. A SIGTERM will cause the cpan-shell
to clean up and leave the shell loop. You can emulate the effect of a
SIGTERM by sending two consecutive SIGINTs, which usually means by
pressing C<^C> twice.

CPAN.pm ignores a SIGPIPE. If the user sets inactivity_timeout, a
SIGALRM is used during the run of the C<perl Makefile.PL> subprocess.

=back

=head2 CPAN::Shell

The commands that are available in the shell interface are methods in
the package CPAN::Shell. If you enter the shell command, all your
input is split by the Text::ParseWords::shellwords() routine which
acts like most shells do. The first word is being interpreted as the
method to be called and the rest of the words are treated as arguments
to this method. Continuation lines are supported if a line ends with a
literal backslash.

=head2 autobundle

C<autobundle> writes a bundle file into the
C<$CPAN::Config-E<gt>{cpan_home}/Bundle> directory. The file contains
a list of all modules that are both available from CPAN and currently
installed within @INC. The name of the bundle file is based on the
current date and a counter.

=head2 recompile

recompile() is a very special command in that it takes no argument and
runs the make/test/install cycle with brute force over all installed
dynamically loadable extensions (aka XS modules) with 'force' in
effect. The primary purpose of this command is to finish a network
installation. Imagine, you have a common source tree for two different
architectures. You decide to do a completely independent fresh
installation. You start on one architecture with the help of a Bundle
file produced earlier. CPAN installs the whole Bundle for you, but
when you try to repeat the job on the second architecture, CPAN
responds with a C<"Foo up to date"> message for all modules. So you
invoke CPAN's recompile on the second architecture and youE<39>re done.

Another popular use for C<recompile> is to act as a rescue in case your
perl breaks binary compatibility. If one of the modules that CPAN uses
is in turn depending on binary compatibility (so you cannot run CPAN
commands), then you should try the CPAN::Nox module for recovery.

=head2 The four C<CPAN::*> Classes: Author, Bundle, Module, Distribution

Although it may be considered internal, the class hierarchy does matter
for both users and programmer. CPAN.pm deals with above mentioned four
classes, and all those classes share a set of methods. A classical
single polymorphism is in effect. A metaclass object registers all
objects of all kinds and indexes them with a string. The strings
referencing objects have a separated namespace (well, not completely
separated):

         Namespace                         Class

   words containing a "/" (slash)      Distribution
    words starting with Bundle::          Bundle
          everything else            Module or Author

Modules know their associated Distribution objects. They always refer
to the most recent official release. Developers may mark their releases
as unstable development versions (by inserting an underbar into the
visible version number), so the really hottest and newest distribution
file is not always the default.  If a module Foo circulates on CPAN in
both version 1.23 and 1.23_90, CPAN.pm offers a convenient way to
install version 1.23 by saying

    install Foo

This would install the complete distribution file (say
BAR/Foo-1.23.tar.gz) with all accompanying material. But if you would
like to install version 1.23_90, you need to know where the
distribution file resides on CPAN relative to the authors/id/
directory. If the author is BAR, this might be BAR/Foo-1.23_90.tar.gz;
so you would have to say

    install BAR/Foo-1.23_90.tar.gz

The first example will be driven by an object of the class
CPAN::Module, the second by an object of class CPAN::Distribution.

=head2 ProgrammerE<39>s interface

If you do not enter the shell, the available shell commands are both
available as methods (C<CPAN::Shell-E<gt>install(...)>) and as
functions in the calling package (C<install(...)>).

There's currently only one class that has a stable interface -
CPAN::Shell. All commands that are available in the CPAN shell are
methods of the class CPAN::Shell. Each of the commands that produce
listings of modules (C<r>, C<autobundle>, C<u>) returns a list of the
IDs of all modules within the list.

=over 2

=item expand($type,@things)

The IDs of all objects available within a program are strings that can
be expanded to the corresponding real objects with the
C<CPAN::Shell-E<gt>expand("Module",@things)> method. Expand returns a
list of CPAN::Module objects according to the C<@things> arguments
given. In scalar context it only returns the first element of the
list.

=item Programming Examples

This enables the programmer to do operations that combine
functionalities that are available in the shell.

    # install everything that is outdated on my disk:
    perl -MCPAN -e 'CPAN::Shell->install(CPAN::Shell->r)'

    # install my favorite programs if necessary:
    for $mod (qw(Net::FTP MD5 Data::Dumper)){
        my $obj = CPAN::Shell->expand('Module',$mod);
        $obj->install;
    }

    # list all modules on my disk that have no VERSION number
    for $mod (CPAN::Shell->expand("Module","/./")){
	next unless $mod->inst_file;
        # MakeMaker convention for undefined $VERSION:
	next unless $mod->inst_version eq "undef";
	print "No VERSION in ", $mod->id, "\n";
    }

=back

=head2 Methods in the four Classes

=head2 Cache Manager

Currently the cache manager only keeps track of the build directory
($CPAN::Config->{build_dir}). It is a simple FIFO mechanism that
deletes complete directories below C<build_dir> as soon as the size of
all directories there gets bigger than $CPAN::Config->{build_cache}
(in MB). The contents of this cache may be used for later
re-installations that you intend to do manually, but will never be
trusted by CPAN itself. This is due to the fact that the user might
use these directories for building modules on different architectures.

There is another directory ($CPAN::Config->{keep_source_where}) where
the original distribution files are kept. This directory is not
covered by the cache manager and must be controlled by the user. If
you choose to have the same directory as build_dir and as
keep_source_where directory, then your sources will be deleted with
the same fifo mechanism.

=head2 Bundles

A bundle is just a perl module in the namespace Bundle:: that does not
define any functions or methods. It usually only contains documentation.

It starts like a perl module with a package declaration and a $VERSION
variable. After that the pod section looks like any other pod with the
only difference being that I<one special pod section> exists starting with
(verbatim):

	=head1 CONTENTS

In this pod section each line obeys the format

        Module_Name [Version_String] [- optional text]

The only required part is the first field, the name of a module
(e.g. Foo::Bar, ie. I<not> the name of the distribution file). The rest
of the line is optional. The comment part is delimited by a dash just
as in the man page header.

The distribution of a bundle should follow the same convention as
other distributions.

Bundles are treated specially in the CPAN package. If you say 'install
Bundle::Tkkit' (assuming such a bundle exists), CPAN will install all
the modules in the CONTENTS section of the pod. You can install your
own Bundles locally by placing a conformant Bundle file somewhere into
your @INC path. The autobundle() command which is available in the
shell interface does that for you by including all currently installed
modules in a snapshot bundle file.

=head2 Prerequisites

If you have a local mirror of CPAN and can access all files with
"file:" URLs, then you only need a perl better than perl5.003 to run
this module. Otherwise Net::FTP is strongly recommended. LWP may be
required for non-UNIX systems or if your nearest CPAN site is
associated with an URL that is not C<ftp:>.

If you have neither Net::FTP nor LWP, there is a fallback mechanism
implemented for an external ftp command or for an external lynx
command.

=head2 Finding packages and VERSION

This module presumes that all packages on CPAN

=over 2

=item *

declare their $VERSION variable in an easy to parse manner. This
prerequisite can hardly be relaxed because it consumes far too much
memory to load all packages into the running program just to determine
the $VERSION variable. Currently all programs that are dealing with
version use something like this

    perl -MExtUtils::MakeMaker -le \
        'print MM->parse_version(shift)' filename

If you are author of a package and wonder if your $VERSION can be
parsed, please try the above method.

=item *

come as compressed or gzipped tarfiles or as zip files and contain a
Makefile.PL (well, we try to handle a bit more, but without much
enthusiasm).

=back

=head2 Debugging

The debugging of this module is pretty difficult, because we have
interferences of the software producing the indices on CPAN, of the
mirroring process on CPAN, of packaging, of configuration, of
synchronicity, and of bugs within CPAN.pm.

In interactive mode you can try "o debug" which will list options for
debugging the various parts of the package. The output may not be very
useful for you as it's just a by-product of my own testing, but if you
have an idea which part of the package may have a bug, it's sometimes
worth to give it a try and send me more specific output. You should
know that "o debug" has built-in completion support.

=head2 Floppy, Zip, Offline Mode

CPAN.pm works nicely without network too. If you maintain machines
that are not networked at all, you should consider working with file:
URLs. Of course, you have to collect your modules somewhere first. So
you might use CPAN.pm to put together all you need on a networked
machine. Then copy the $CPAN::Config->{keep_source_where} (but not
$CPAN::Config->{build_dir}) directory on a floppy. This floppy is kind
of a personal CPAN. CPAN.pm on the non-networked machines works nicely
with this floppy.

=head1 CONFIGURATION

When the CPAN module is installed, a site wide configuration file is
created as CPAN/Config.pm. The default values defined there can be
overridden in another configuration file: CPAN/MyConfig.pm. You can
store this file in $HOME/.cpan/CPAN/MyConfig.pm if you want, because
$HOME/.cpan is added to the search path of the CPAN module before the
use() or require() statements.

Currently the following keys in the hash reference $CPAN::Config are
defined:

  build_cache        size of cache for directories to build modules
  build_dir          locally accessible directory to build modules
  index_expire       after this many days refetch index files
  cpan_home          local directory reserved for this package
  gzip		     location of external program gzip
  inactivity_timeout breaks interactive Makefile.PLs after this
                     many seconds inactivity. Set to 0 to never break.
  inhibit_startup_message
                     if true, does not print the startup message
  keep_source        keep the source in a local directory?
  keep_source_where  directory in which to keep the source (if we do)
  make               location of external make program
  make_arg	     arguments that should always be passed to 'make'
  make_install_arg   same as make_arg for 'make install'
  makepl_arg	     arguments passed to 'perl Makefile.PL'
  pager              location of external program more (or any pager)
  prerequisites_policy
                     what to do if you are missing module prerequisites
                     ('follow' automatically, 'ask' me, or 'ignore')
  scan_cache	     controls scanning of cache ('atstart' or 'never')
  tar                location of external program tar
  unzip              location of external program unzip
  urllist	     arrayref to nearby CPAN sites (or equivalent locations)
  wait_list          arrayref to a wait server to try (See CPAN::WAIT)
  ftp_proxy,      }  the three usual variables for configuring
    http_proxy,   }  proxy requests. Both as CPAN::Config variables
    no_proxy      }  and as environment variables configurable.

You can set and query each of these options interactively in the cpan
shell with the command set defined within the C<o conf> command:

=over 2

=item o conf E<lt>scalar optionE<gt>

prints the current value of the I<scalar option>

=item o conf E<lt>scalar optionE<gt> E<lt>valueE<gt>

Sets the value of the I<scalar option> to I<value>

=item o conf E<lt>list optionE<gt>

prints the current value of the I<list option> in MakeMaker's
neatvalue format.

=item o conf E<lt>list optionE<gt> [shift|pop]

shifts or pops the array in the I<list option> variable

=item o conf E<lt>list optionE<gt> [unshift|push|splice] E<lt>listE<gt>

works like the corresponding perl commands.

=back

=head2 urllist parameter has CD-ROM support

The C<urllist> parameter of the configuration table contains a list of
URLs that are to be used for downloading. If the list contains any
C<file> URLs, CPAN always tries to get files from there first. This
feature is disabled for index files. So the recommendation for the
owner of a CD-ROM with CPAN contents is: include your local, possibly
outdated CD-ROM as a C<file> URL at the end of urllist, e.g.

  o conf urllist push file://localhost/CDROM/CPAN

CPAN.pm will then fetch the index files from one of the CPAN sites
that come at the beginning of urllist. It will later check for each
module if there is a local copy of the most recent version.

Another peculiarity of urllist is that the site that we could
successfully fetch the last file from automatically gets a preference
token and is tried as the first site for the next request. So if you
add a new site at runtime it may happen that the previously preferred
site will be tried another time. This means that if you want to disallow
a site for the next transfer, it must be explicitly removed from
urllist.

=head1 SECURITY

There's no strong security layer in CPAN.pm. CPAN.pm helps you to
install foreign, unmasked, unsigned code on your machine. We compare
to a checksum that comes from the net just as the distribution file
itself. If somebody has managed to tamper with the distribution file,
they may have as well tampered with the CHECKSUMS file. Future
development will go towards strong authentication.

=head1 EXPORT

Most functions in package CPAN are exported per default. The reason
for this is that the primary use is intended for the cpan shell or for
oneliners.

=head1 POPULATE AN INSTALLATION WITH LOTS OF MODULES

To populate a freshly installed perl with my favorite modules is pretty
easiest by maintaining a private bundle definition file. To get a useful
blueprint of a bundle definition file, the command autobundle can be used
on the CPAN shell command line. This command writes a bundle definition
file for all modules that re installed for the currently running perl
interpreter. It's recommended to run this command only once and from then
on maintain the file manually under a private name, say
Bundle/my_bundle.pm. With a clever bundle file you can then simply say

    cpan> install Bundle::my_bundle

then answer a few questions and then go out.

Maintaining a bundle definition file means to keep track of two things:
dependencies and interactivity. CPAN.pm (currently) does not take into
account dependencies between distributions, so a bundle definition file
should specify distributions that depend on others B<after> the others.
On the other hand, it's a bit annoying that many distributions need some
interactive configuring. So what I try to accomplish in my private bundle
file is to have the packages that need to be configured early in the file
and the gentle ones later, so I can go out after a few minutes and leave
CPAN.pm unattained.

=head1 WORKING WITH CPAN.pm BEHIND FIREWALLS

Thanks to Graham Barr for contributing the firewall following howto.

Firewalls can be categorized into three basic types.

=over

=item http firewall

This is where the firewall machine runs a web server and to access the
outside world you must do it via the web server. If you set environment
variables like http_proxy or ftp_proxy to a values beginning with http://
or in your web browser you have to set proxy information then you know
you are running a http firewall.

To access servers outside these types of firewalls with perl (even for
ftp) you will need to use LWP.

=item ftp firewall

This where the firewall machine runs a ftp server. This kind of firewall will
only let you access ftp serves outside the firewall. This is usually done by
connecting to the firewall with ftp, then entering a username like
"user@outside.host.com"

To access servers outside these type of firewalls with perl you
will need to use Net::FTP.

=item One way visibility

I say one way visibility as these firewalls try to make themselve look
invisible to the users inside the firewall. An FTP data connection is
normally created by sending the remote server your IP address and then
listening for the connection. But the remote server will not be able to
connect to you because of the firewall. So for these types of firewall
FTP connections need to be done in a passive mode.

There are two that I can think off.

=over

=item SOCKS

If you are using a SOCKS firewall you will need to compile perl and link
it with the SOCKS library, this is what is normally called a ``socksified''
perl. With this executable you will be able to connect to servers outside
the firewall as if it is not there.

=item IP Masquerade

This is the firewall implemented in the Linux kernel, it allows you to
hide a complete network behind one IP address. With this firewall no
special compiling is need as you can access hosts directly.

=back

=back

=head1 BUGS

We should give coverage for _all_ of the CPAN and not just the PAUSE
part, right? In this discussion CPAN and PAUSE have become equal --
but they are not. PAUSE is authors/ and modules/. CPAN is PAUSE plus
the clpa/, doc/, misc/, ports/, src/, scripts/.

Future development should be directed towards a better integration of
the other parts.

If a Makefile.PL requires special customization of libraries, prompts
the user for special input, etc. then you may find CPAN is not able to
build the distribution. In that case, you should attempt the
traditional method of building a Perl module package from a shell.

=head1 AUTHOR

Andreas König E<lt>a.koenig@kulturbox.deE<gt>

=head1 SEE ALSO

perl(1), CPAN::Nox(3)

=cut

