#!/usr/local/bin/perl
 
use Config;
use File::Basename qw(&basename &dirname);
use File::Spec;
use Cwd;
 
# List explicitly here the variables you want Configure to
# generate.  Metaconfig only looks for shell variables, so you
# have to mention them as if they were shell variables, not
# %Config entries.  Thus you write
#  $startperl
# to ensure Configure will look for $Config{startperl}.
# Wanted:  $archlibexp
 
# This forces PL files to create target in same directory as PL file.
# This is so that make depend always knows where to find PL derivatives.
$origdir = cwd;
chdir dirname($0);
$file = basename($0, '.PL');
$file .= '.com' if $^O eq 'VMS';
 
open OUT,">$file" or die "Can't create $file: $!";
 
print "Extracting $file (with variable substitutions)\n";
 
# In this section, perl variables will be expanded during extraction.
# You can use $Config{...} to use Configure variables.
 
print OUT <<"!GROK!THIS!";
$Config{startperl}
    eval 'exec $Config{perlpath} -S \$0 \${1+"\$@"}'
    if \$running_under_some_shell;
--\$running_under_some_shell;
!GROK!THIS!
 
# In the following, perl variables are not expanded during extraction.
 
print OUT <<'!NO!SUBS!';

# Version 2.0, Simon Cozens, Thu Mar 30 17:52:45 JST 2000 
# Version 2.01, Tom Christiansen, Thu Mar 30 08:25:14 MST 2000
# Version 2.02, Simon Cozens, Sun Apr 16 01:53:36 JST 2000
# Version 2.03, Edward Peschko, Mon Feb 26 12:04:17 PST 2001

use strict;
use warnings;
use v5.6.0;

use FileHandle;
use Config;
use Fcntl qw(:DEFAULT :flock);
use File::Temp qw(tempfile);
use Cwd;
our $VERSION = 2.03;
$| = 1;

$SIG{INT} = sub { exit(); }; # exit gracefully and clean up after ourselves.

use subs qw{
    cc_harness check_read check_write checkopts_byte choose_backend
    compile_byte compile_cstyle compile_module generate_code
    grab_stash parse_argv sanity_check vprint yclept spawnit
};
sub opt(*); # imal quoting

our ($Options, $BinPerl, $Backend);
our ($Input => $Output);
our ($logfh);
our ($cfile);

# eval { main(); 1 } or die;

main();

sub main {
    parse_argv();
    check_write($Output);
    choose_backend();
    generate_code();
    run_code();
    _die("XXX: Not reached?");
}

#######################################################################

sub choose_backend {
    # Choose the backend.
    $Backend = 'C';
    if (opt(B)) {
        checkopts_byte();
        $Backend = 'Bytecode';
    }
    if (opt(S) && opt(c)) {
        # die "$0: Do you want me to compile this or not?\n";
        delete $Options->{S};
    }
    $Backend = 'CC' if opt(O);
}


sub generate_code { 

    vprint 0, "Compiling $Input";

    $BinPerl  = yclept();  # Calling convention for perl.

    if (opt(shared)) {
        compile_module();
    } else {
        if ($Backend eq 'Bytecode') {
            compile_byte();
        } else {
            compile_cstyle();
        }
    }
    exit(0) if (!opt('r'));
}

sub run_code {
    vprint 0, "Running code";
    run("$Output @ARGV");
    exit(0);
}

# usage: vprint [level] msg args
sub vprint {
    my $level;
    if (@_ == 1) {
        $level = 1;
    } elsif ($_[0] =~ /^\d$/) {
        $level = shift;
    } else {
        # well, they forgot to use a number; means >0
        $level = 0;
    } 
    my $msg = "@_";
    $msg .= "\n" unless substr($msg, -1) eq "\n";
    if (opt(v) > $level)
    {
         print        "$0: $msg" if !opt('log');
	 print $logfh "$0: $msg" if  opt('log');
    }
}

sub parse_argv {

    use Getopt::Long; 
#    Getopt::Long::Configure("bundling"); turned off. this is silly because 
#                                         it doesn't allow for long switches.
    Getopt::Long::Configure("no_ignore_case");

    # no difference in exists and defined for %ENV; also, a "0"
    # argument or a "" would not help cc, so skip
    unshift @ARGV, split ' ', $ENV{PERLCC_OPTS} if $ENV{PERLCC_OPTS};

    $Options = {};
    Getopt::Long::GetOptions( $Options,
        'L:s',          # lib directory
        'I:s',          # include directories (FOR C, NOT FOR PERL)
        'o:s',          # Output executable
        'v:i',           # Verbosity level
        'e:s',          # One-liner
	'r',            # run resulting executable
        'B',            # Byte compiler backend
        'O',            # Optimised C backend
        'c',            # Compile only
        'h',            # Help me
        'S',            # Dump C files
	'r',            # run the resulting executable
        'static',       # Dirty hack to enable -shared/-static
        'shared',       # Create a shared library (--shared for compat.)
	'log:s'         # where to log compilation process information
    );
        
    # This is an attempt to make perlcc's arg. handling look like cc.
    # if ( opt('s') ) {  # must quote: looks like s)foo)bar)!
    #   if (opt('s') eq 'hared') {
    #        $Options->{shared}++; 
    #    } elsif (opt('s') eq 'tatic') {
    #        $Options->{static}++; 
    #    } else {
    #        warn "$0: Unknown option -s", opt('s');
    #    }
    # }

    $Options->{v} += 0;

    helpme() if opt(h); # And exit

    $Output = opt(o) || 'a.out';
    $Output = relativize($Output);
    $logfh  = new FileHandle(">> " . opt('log')) if (opt('log'));

    if (opt(e)) {
        warn "$0: using -e 'code' as input file, ignoring @ARGV\n" if @ARGV;
        # We don't use a temporary file here; why bother?
        # XXX: this is not bullet proof -- spaces or quotes in name!
        $Input = "-e '".opt(e)."'"; # Quotes eaten by shell
    } else {
        $Input = shift @ARGV;  # XXX: more files?
        _usage_and_die("$0: No input file specified\n") unless $Input;
        # DWIM modules. This is bad but necessary.
        $Options->{shared}++ if $Input =~ /\.pm\z/;
        warn "$0: using $Input as input file, ignoring @ARGV\n" if @ARGV;
        check_read($Input);
        check_perl($Input);
        sanity_check();
    }

}

sub opt(*) {
    my $opt = shift;
    return exists($Options->{$opt}) && ($Options->{$opt} || 0);
} 

sub compile_module { 
    die "$0: Compiling to shared libraries is currently disabled\n";
}

sub compile_byte {
    require ByteLoader;
    my $stash = grab_stash();
    my $command = "$BinPerl -MO=Bytecode,$stash $Input";
    # The -a option means we'd have to close the file and lose the
    # lock, which would create the tiniest of races. Instead, append
    # the output ourselves. 
    vprint 1, "Writing on $Output";

    my $openflags = O_WRONLY | O_CREAT;
    $openflags |= O_BINARY if eval { O_BINARY; 1 };
    $openflags |= O_EXLOCK if eval { O_EXLOCK; 1 };

    # these dies are not "$0: .... \n" because they "can't happen"

    sysopen(OUT, $Output, $openflags)
        or die "can't write to $Output: $!";

    # this is blocking; hold on; why are we doing this??
    # flock OUT, LOCK_EX or die "can't lock $Output: $!"
    #    unless eval { O_EXLOCK; 1 };

    truncate(OUT, 0)
        or die "couldn't trunc $Output: $!";

    print OUT <<EOF;
#!$^X
use ByteLoader $ByteLoader::VERSION;
EOF

    # Now the compile:
    vprint 1, "Compiling...";
    vprint 3, "Calling $command";

    my ($output_r, $error_r) = spawnit($command);

    if (@$error_r && $? != 0) {
	_die("$0: $Input did not compile, which can't happen:\n@$error_r\n");
    } else {
	my @error = grep { !/^$Input syntax OK$/o } @$error_r;
	warn "$0: Unexpected compiler output:\n@error" if @error;
    }
	
    # Write it and leave.
    print OUT @$output_r               or _die("can't write $Output: $!");
    close OUT                          or _die("can't close $Output: $!");

    # wait, how could it be anything but what you see next?
    chmod 0777 & ~umask, $Output    or _die("can't chmod $Output: $!");
    exit 0;
}

sub compile_cstyle {
    my $stash = grab_stash();
    
    # What are we going to call our output C file?
    my $lose = 0;
    my ($cfh);

    if (opt(S) || opt(c)) {
        # We need to keep it.
        if (opt(e)) {
            $cfile = "a.out.c";
        } else {
            $cfile = $Input;
            # File off extension if present
            # hold on: plx is executable; also, careful of ordering!
            $cfile =~ s/\.(?:p(?:lx|l|h)|m)\z//i;
            $cfile .= ".c";
            $cfile = $Output if opt(c) && $Output =~ /\.c\z/i;
        }
        check_write($cfile);
    } else {
        # Don't need to keep it, be safe with a tempfile.
        $lose = 1;
        ($cfh, $cfile) = tempfile("pccXXXXX", SUFFIX => ".c"); 
        close $cfh; # See comment just below
    }
    vprint 1, "Writing C on $cfile";

    my $max_line_len = '';
    if ($^O eq 'MSWin32' && $Config{cc} =~ /^cl/i) {
        $max_line_len = '-l2000,';
    }

    # This has to do the write itself, so we can't keep a lock. Life
    # sucks.
    my $command = "$BinPerl -MO=$Backend,$max_line_len$stash,-o$cfile $Input";
    vprint 1, "Compiling...";
    vprint 1, "Calling $command";

	my ($output_r, $error_r) = spawnit($command);
	my @output = @$output_r;
	my @error = @$error_r;

    if (@error && $? != 0) {
        _die("$0: $Input did not compile, which can't happen:\n@error\n");
    }

    cc_harness($cfile,$stash) unless opt(c);

    if ($lose) {
        vprint 2, "unlinking $cfile";
        unlink $cfile or _die("can't unlink $cfile: $!"); 
    }
}

sub cc_harness {
	my ($cfile,$stash)=@_;
	use ExtUtils::Embed ();
	my $command = ExtUtils::Embed::ccopts." -o $Output $cfile ";
	$command .= " -I".$_ for split /\s+/, opt(I);
	$command .= " -L".$_ for split /\s+/, opt(L);
	my @mods = split /-?u /, $stash;
	$command .= " ".ExtUtils::Embed::ldopts("-std", \@mods);
	vprint 3, "running $Config{cc} $command";
	system("$Config{cc} $command");
}

# Where Perl is, and which include path to give it.
sub yclept {
    my $command = "$^X ";

    # DWIM the -I to be Perl, not C, include directories.
    if (opt(I) && $Backend eq "Bytecode") {
        for (split /\s+/, opt(I)) {
            if (-d $_) {
                push @INC, $_;
            } else {
                warn "$0: Include directory $_ not found, skipping\n";
            }
        }
    }
            
    $command .= "-I$_ " for @INC;
    return $command;
}

# Use B::Stash to find additional modules and stuff.
{
    my $_stash;
    sub grab_stash {

        warn "already called get_stash once" if $_stash;

        my $command = "$BinPerl -MB::Stash -c $Input";
        # Filename here is perfectly sanitised.
        vprint 3, "Calling $command\n";

		my ($stash_r, $error_r) = spawnit($command);
		my @stash = @$stash_r;
		my @error = @$error_r;

    	if (@error && $? != 0) {
            _die("$0: $Input did not compile:\n@error\n");
        }

        $stash[0] =~ s/,-u\<none\>//;
        vprint 2, "Stash: ", join " ", split /,?-u/, $stash[0];
        chomp $stash[0];
        return $_stash = $stash[0];
    }

}

# Check the consistency of options if -B is selected.
# To wit, (-B|-O) ==> no -shared, no -S, no -c
sub checkopts_byte {

    _die("$0: Please choose one of either -B and -O.\n") if opt(O);

    if (opt(shared)) {
        warn "$0: Will not create a shared library for bytecode\n";
        delete $Options->{shared};
    }

    for my $o ( qw[c S] ) { 
        if (opt($o)) { 
            warn "$0: Compiling to bytecode is a one-pass process--",
                  "-$o ignored\n";
            delete $Options->{$o};
        }
    }

}

# Check the input and output files make sense, are read/writeable.
sub sanity_check {
    if ($Input eq $Output) {
        if ($Input eq 'a.out') {
            _die("$0: Compiling a.out is probably not what you want to do.\n");
            # You fully deserve what you get now. No you *don't*. typos happen.
        } else {
            warn "$0: Will not write output on top of input file, ",
                "compiling to a.out instead\n";
            $Output = "a.out";
        }
    }
}

sub check_read { 
    my $file = shift;
    unless (-r $file) {
        _die("$0: Input file $file is a directory, not a file\n") if -d _;
        unless (-e _) {
            _die("$0: Input file $file was not found\n");
        } else {
            _die("$0: Cannot read input file $file: $!\n");
        }
    }
    unless (-f _) {
        # XXX: die?  don't try this on /dev/tty
        warn "$0: WARNING: input $file is not a plain file\n";
    } 
}

sub check_write {
    my $file = shift;
    if (-d $file) {
        _die("$0: Cannot write on $file, is a directory\n");
    }
    if (-e _) {
        _die("$0: Cannot write on $file: $!\n") unless -w _;
    } 
    unless (-w cwd()) { 
        _die("$0: Cannot write in this directory: $!\n");
    }
}

sub check_perl {
    my $file = shift;
    unless (-T $file) {
        warn "$0: Binary `$file' sure doesn't smell like perl source!\n";
        print "Checking file type... ";
        system("file", $file);  
        _die("Please try a perlier file!\n");
    } 

    open(my $handle, "<", $file)    or _die("XXX: can't open $file: $!");
    local $_ = <$handle>;
    if (/^#!/ && !/perl/) {
        _die("$0: $file is a ", /^#!\s*(\S+)/, " script, not perl\n");
    } 

} 

# File spawning and error collecting
sub spawnit {
	my ($command) = shift;
	my (@error,@output);
	my $errname;
	(undef, $errname) = tempfile("pccXXXXX");
	{ 
	open (S_OUT, "$command 2>$errname |")
		or _die("$0: Couldn't spawn the compiler.\n");
	@output = <S_OUT>;
	}
	open (S_ERROR, $errname) or _die("$0: Couldn't read the error file.\n");
	@error = <S_ERROR>;
	close S_ERROR;
	close S_OUT;
	unlink $errname or _die("$0: Can't unlink error file $errname");
	return (\@output, \@error);
}

sub helpme {
       print "perlcc compiler frontend, version $VERSION\n\n";
       { no warnings;
       exec "pod2usage $0";
       exec "perldoc $0";
       exec "pod2text $0";
       }
}

sub relativize {
	my ($args) = @_;

	return() if ($args =~ m"^[/\\]");
	return("./$args");
}

sub _die {
    $logfh->print(@_) if opt('log');
    print STDERR @_;
    exit(); # should die eventually. However, needed so that a 'make compile'
            # can compile all the way through to the end for standard dist.
}

sub _usage_and_die {
    _die(<<EOU);
$0: Usage:
$0 [-o executable] [-r] [-O|-B|-c|-S] [-log log] [source[.pl] | -e oneliner]
EOU
}

sub run {
    my (@commands) = @_;

    print interruptrun(@commands) if (!opt('log'));
    $logfh->print(interruptrun(@commands)) if (opt('log'));
}

sub interruptrun
{
    my (@commands) = @_;

    my $command = join('', @commands);
    local(*FD);
    my $pid = open(FD, "$command |");
    my $text;
    
    local($SIG{HUP}) = sub { kill 9, $pid; exit };
    local($SIG{INT}) = sub { kill 9, $pid; exit };

    my $needalarm = 
          ($ENV{PERLCC_TIMEOUT} && 
	  $Config{'osname'} ne 'MSWin32' && 
	  $command =~ m"(^|\s)perlcc\s");

    eval 
    {
         local($SIG{ALRM}) = sub { die "INFINITE LOOP"; };
         alarm($ENV{PERLCC_TIMEOUT}) if ($needalarm);
	 $text = join('', <FD>);
	 alarm(0) if ($needalarm);
    };

    if ($@)
    {
        eval { kill 'HUP', $pid };
        vprint 0, "SYSTEM TIMEOUT (infinite loop?)\n";
    }

    close(FD);
    return($text);
}

END {
    unlink $cfile if ($cfile && !opt(S) && !opt(c));
}

__END__

=head1 NAME

perlcc - generate executables from Perl programs

=head1 SYNOPSIS

    $ perlcc hello              # Compiles into executable 'a.out'
    $ perlcc -o hello hello.pl  # Compiles into executable 'hello'

    $ perlcc -O file            # Compiles using the optimised C backend
    $ perlcc -B file            # Compiles using the bytecode backend

    $ perlcc -c file            # Creates a C file, 'file.c'
    $ perlcc -S -o hello file   # Creates a C file, 'file.c',
                                # then compiles it to executable 'hello'
    $ perlcc -c out.c file      # Creates a C file, 'out.c' from 'file'

    $ perlcc -e 'print q//'     # Compiles a one-liner into 'a.out'
    $ perlcc -c -e 'print q//'  # Creates a C file 'a.out.c'

    $ perlcc -r hello           # compiles 'hello' into 'a.out', runs 'a.out'.

    $ perlcc -r hello a b c     # compiles 'hello' into 'a.out', runs 'a.out'.
                                # with arguments 'a b c' 

    $ perlcc hello -log c       # compiles 'hello' into 'a.out' logs compile
                                # log into 'c'. 

=head1 DESCRIPTION

F<perlcc> creates standalone executables from Perl programs, using the
code generators provided by the L<B> module. At present, you may
either create executable Perl bytecode, using the C<-B> option, or 
generate and compile C files using the standard and 'optimised' C
backends.

The code generated in this way is not guaranteed to work. The whole
codegen suite (C<perlcc> included) should be considered B<very>
experimental. Use for production purposes is strongly discouraged.

=head1 OPTIONS

=over 4

=item -LI<library directories>

Adds the given directories to the library search path when C code is
passed to your C compiler.

=item -II<include directories>

Adds the given directories to the include file search path when C code is
passed to your C compiler; when using the Perl bytecode option, adds the
given directories to Perl's include path.

=item -o I<output file name>

Specifies the file name for the final compiled executable.

=item -c I<C file name>

Create C code only; do not compile to a standalone binary.

=item -e I<perl code>

Compile a one-liner, much the same as C<perl -e '...'>

=item -S

Do not delete generated C code after compilation.

=item -B

Use the Perl bytecode code generator.

=item -O

Use the 'optimised' C code generator. This is more experimental than
everything else put together, and the code created is not guaranteed to
compile in finite time and memory, or indeed, at all.

=item -v

Increase verbosity of output; can be repeated for more verbose output.

=item -r 

Run the resulting compiled script after compiling it.

=item -log

Log the output of compiling to a file rather than to stdout.

=back

=cut

!NO!SUBS!

close OUT or die "Can't close $file: $!";
chmod 0755, $file or die "Can't reset permissions for $file: $!\n";
exec("$Config{'eunicefix'} $file") if $Config{'eunicefix'} ne ':';
chdir $origdir;
