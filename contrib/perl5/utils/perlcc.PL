#!/usr/local/bin/perl
 
use Config;
use File::Basename qw(&basename &dirname);
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
!GROK!THIS!
 
# In the following, perl variables are not expanded during extraction.
 
print OUT <<'!NO!SUBS!';

use Config;
use strict;
use FileHandle;
use File::Basename qw(&basename &dirname);
use Cwd;

use Getopt::Long;

$Getopt::Long::bundling_override = 1;
$Getopt::Long::passthrough = 0;
$Getopt::Long::ignore_case = 0;

my $options = {};
my $_fh;

main();

sub main
{

    GetOptions
            (
            $options,   "L:s",
                        "I:s",
                        "C:s",
                        "o:s",
                        "e:s",
                        "regex:s",
                        "verbose:s",
                        "log:s",
						"argv:s",
                        "gen",
                        "sav",
                        "run",
                        "prog",
                        "mod"
            );


    my $key;

    local($") = "|";

    _usage() if (!_checkopts());
    push(@ARGV, _maketempfile()) if ($options->{'e'});

    _usage() if (!@ARGV);
                
    my $file;
    foreach $file (@ARGV)
    {
        _print("
--------------------------------------------------------------------------------
Compiling $file:
--------------------------------------------------------------------------------
", 36 );
        _doit($file);
    }
}
        
sub _doit
{
    my ($file) = @_;

    my ($program_ext, $module_ext) = _getRegexps();
    my ($obj, $objfile, $so, $type);

    if  (
            (($file =~ m"@$program_ext") && ($file !~ m"@$module_ext"))
            || (defined($options->{'prog'}) || defined($options->{'run'}))
        )
    {
        $objfile = ($options->{'C'}) ?     $options->{'C'} : "$file.c";
        $type = 'program';

        $obj =         ($options->{'o'})?     $options->{'o'} : 
                                            _getExecutable( $file,$program_ext);

        return() if (!$obj);

    }
    elsif (($file =~ m"@$module_ext") || ($options->{'mod'}))
    {
        die "Shared objects are not supported on Win32 yet!!!!\n"
                                      if ($Config{'osname'} eq 'MSWin32');

        $obj =         ($options->{'o'})?    $options->{'o'} :
                                            _getExecutable($file, $module_ext);
        $so = "$obj.$Config{so}";
        $type = 'sharedlib';
        return() if (!$obj);
        $objfile = ($options->{'C'}) ?     $options->{'C'} : "$file.c";
    }
    else
    {
        _error("noextension", $file, $program_ext, $module_ext);
        return();
    }

    if ($type eq 'program')
    {
        _print("Making C($objfile) for $file!\n", 36 );

        my $errcode = _createCode($objfile, $file);
        (_print( "ERROR: In generating code for $file!\n", -1), return()) 
                                                                if ($errcode);

        _print("Compiling C($obj) for $file!\n", 36 ) if (!$options->{'gen'});
        $errcode = _compileCode($file, $objfile, $obj) 
                                            if (!$options->{'gen'});

        if ($errcode)
		{
			_print( "ERROR: In compiling code for $objfile !\n", -1);
			my $ofile = File::Basename::basename($objfile);
			$ofile =~ s"\.c$"\.o"s;
			
			_removeCode("$ofile"); 
			return()
		}
    
        _runCode($obj) if ($options->{'run'});

        _removeCode($objfile) if (!$options->{'sav'} || 
                                    ($options->{'e'} && !$options->{'C'}));

        _removeCode($file) if ($options->{'e'}); 

        _removeCode($obj) if (($options->{'e'}
			       && !$options->{'sav'}
			       && !$options->{'o'})
			      || ($options->{'run'} && !$options->{'sav'}));
    }
    else
    {
        _print( "Making C($objfile) for $file!\n", 36 );
        my $errcode = _createCode($objfile, $file, $obj);
        (_print( "ERROR: In generating code for $file!\n", -1), return()) 
                                                                if ($errcode);
    
        _print( "Compiling C($so) for $file!\n", 36 ) if (!$options->{'gen'});

        my $errorcode = 
            _compileCode($file, $objfile, $obj, $so ) if (!$options->{'gen'});

        (_print( "ERROR: In compiling code for $objfile!\n", -1), return()) 
                                                                if ($errcode);
    }
}

sub _getExecutable
{
    my ($sourceprog, $ext) = @_;
    my ($obj);

    if (defined($options->{'regex'}))
    {
        eval("(\$obj = \$sourceprog) =~ $options->{'regex'}");
        return(0) if (_error('badeval', $@));
        return(0) if (_error('equal', $obj, $sourceprog));
    }
    elsif (defined ($options->{'ext'}))
    {
        ($obj = $sourceprog) =~ s"@$ext"$options->{ext}"g;        
        return(0) if (_error('equal', $obj, $sourceprog));
    }
	elsif (defined ($options->{'run'}))
	{
		$obj = "perlc$$";
	}
    else
    {
        ($obj = $sourceprog) =~ s"@$ext""g;
        return(0) if (_error('equal', $obj, $sourceprog));
    }
    return($obj);
}

sub _createCode
{
    my ( $generated_cfile, $file, $final_output ) = @_;
    my $return;

    local($") = " -I";

    if (@_ == 2)                                   # compiling a program   
    {
        _print( "$^X -I@INC -MO=CC,-o$generated_cfile $file\n", 36);
        $return =  _run("$ -I@INC -MO=CC,-o$generated_cfile $file", 9);
        $return;
    }
    else                                           # compiling a shared object
    {            
        _print( 
            "$ -I@INC -MO=CC,-m$final_output,-o$generated_cfile $file\n", 36);
        $return = 
        _run("$ -I@INC -MO=CC,-m$final_output,-o$generated_cfile $file", 9);
        $return;
    }
}

sub _compileCode
{
    my ($sourceprog, $generated_cfile, $output_executable, $shared_object) = @_;
    my @return;

    if (@_ == 3)                            # just compiling a program 
    {
        $return[0] = 
        _ccharness('static', $sourceprog, "-o", $output_executable, $generated_cfile);  
        $return[0];
    }
    else
    {
        my $object_file = $generated_cfile;
        $object_file =~ s"\.c$"$Config{_o}";   

        $return[0] = _ccharness('compile', $sourceprog, "-c", $generated_cfile);
        $return[1] = _ccharness
                            (
				'dynamic', 
                                $sourceprog, "-o", 
                                $shared_object, $object_file 
                            );
        return(1) if (grep ($_, @return));
        return(0);
    }
}

sub _runCode
{
    my ($executable) = @_;
    _print("$executable $options->{'argv'}\n", 36);
    _run("$executable $options->{'argv'}", -1 );
}

sub _removeCode
{
    my ($file) = @_;
    unlink($file) if (-e $file);
}

sub _ccharness
{
    my $type = shift;
    my (@args) = @_;
    local($") = " ";

    my $sourceprog = shift(@args);
    my ($libdir, $incdir);

    if (-d "$Config{installarchlib}/CORE")
    {
        $libdir = "-L$Config{installarchlib}/CORE";
        $incdir = "-I$Config{installarchlib}/CORE";
    }
    else
    {
        $libdir = "-L.. -L."; 
        $incdir = "-I.. -I.";
    }

    $libdir .= " -L$options->{L}" if (defined($options->{L}));
    $incdir .= " -I$options->{L}" if (defined($options->{L}));

    my $linkargs = '';

    if (!grep(/^-[cS]$/, @args))
    {
	my $lperl = $^O eq 'os2' ? '-llibperl' : '-lperl';
	my $flags = $type eq 'dynamic' ? $Config{lddlflags} : $Config{ldflags};
        $linkargs = "$flags $libdir $lperl @Config{libs}";
    }

    my @sharedobjects = _getSharedObjects($sourceprog); 

    my $cccmd = 
        "$Config{cc} @Config{qw(ccflags optimize)} $incdir @sharedobjects @args $linkargs";


    _print ("$cccmd\n", 36);
    _run("$cccmd", 18 );
}

sub _getSharedObjects
{
    my ($sourceprog) = @_;
    my ($tmpfile, $incfile);
    my (@return);
    local($") = " -I";

    if ($Config{'osname'} eq 'MSWin32') 
    { 
        # _addstuff;    
    }
    else
    {
        my ($tmpprog);
        ($tmpprog = $sourceprog) =~ s"(.*)[\/\\](.*)"$2";
        $tmpfile = "/tmp/$tmpprog.tst";
        $incfile = "/tmp/$tmpprog.val";
    }

    my $fd = new FileHandle("> $tmpfile") || die "Couldn't open $tmpfile!\n";
    my $fd2 = 
        new FileHandle("$sourceprog") || die "Couldn't open $sourceprog!\n";

    my $perl = <$fd2>;  # strip off header;

    print $fd 
<<"EOF";
        use FileHandle;
        my \$fh3  = new FileHandle("> $incfile") 
                                        || die "Couldn't open $incfile\\n";

        my \$key;
        foreach \$key (keys(\%INC)) { print \$fh3 "\$key:\$INC{\$key}\\n"; }
        close(\$fh3);
        exit();
EOF

    print $fd (   <$fd2>    );
    close($fd);

    _print("$ -I@INC $tmpfile\n", 36);
    _run("$ -I@INC $tmpfile", 9 );

    $fd = new FileHandle ("$incfile"); 
    my @lines = <$fd>;    

    unlink($tmpfile);
    unlink($incfile);

    my $line;
    my $autolib;

    foreach $line (@lines) 
    {
        chomp($line);
        my ($modname, $modpath) = split(':', $line);
        my ($dir, $file) = ($modpath=~ m"(.*)[\\/]($modname)");
        
        if ($autolib = _lookforAuto($dir, $file))
        {
            push(@return, $autolib);
        }
    }

    return(@return);
}

sub _maketempfile
{
    my $return;

#    if ($Config{'osname'} eq 'MSWin32') 
#            { $return = "C:\\TEMP\\comp$$.p"; }
#    else
#            { $return = "/tmp/comp$$.p"; }

    $return = "comp$$.p"; 

    my $fd = new FileHandle( "> $return") || die "Couldn't open $return!\n";
    print $fd $options->{'e'};
    close($fd);

    return($return);
}
    
    
sub _lookforAuto
{
    my ($dir, $file) = @_;    

    my $relshared;
    my $return;

    ($relshared = $file) =~ s"(.*)\.pm"$1";

    my ($tmp, $modname) = ($relshared =~ m"(?:(.*)[\\/]){0,1}(.*)"s);

    $relshared .= 
        ($Config{'osname'} eq 'MSWin32')? "\\$modname.dll" : "/$modname.so";
    


    if (-e ($return = "$Config{'installarchlib'}/auto/$relshared") )
    {
        return($return);    
    }
    elsif (-e ($return = "$Config{'installsitearch'}/auto/$relshared"))
    {
        return($return);
    }
    elsif (-e ($return = "$dir/arch/auto/$relshared"))
    {
        return($return);    
    }
    else
    {
        return(undef);
    }
}

sub _getRegexps    # make the appropriate regexps for making executables, 
{                  # shared libs

    my ($program_ext, $module_ext) = ([],[]); 


    @$program_ext = ($ENV{PERL_SCRIPT_EXT})? split(':', $ENV{PERL_SCRIPT_EXT}) :
                                            ('.p$', '.pl$', '.bat$');


    @$module_ext  = ($ENV{PERL_MODULE_EXT})? split(':', $ENV{PERL_MODULE_EXT}) :
                                            ('.pm$');


    _mungeRegexp( $program_ext );
    _mungeRegexp( $module_ext  );    

    return($program_ext, $module_ext);
}

sub _mungeRegexp
{
    my ($regexp) = @_;

    grep(s:(^|[^\\])\.:$1\x00\\.:g, @$regexp);
    grep(s:(^|[^\x00])\\\.:$1\.:g,  @$regexp);
    grep(s:\x00::g,                 @$regexp);
}


sub _error
{
    my ($type, @args) = @_;

    if ($type eq 'equal')
    {
            
        if ($args[0] eq $args[1])
        {
            _print ("ERROR: The object file '$args[0]' does not generate a legitimate executable file! Skipping!\n", -1);
            return(1);
        }
    }
    elsif ($type eq 'badeval')
    {
        if ($args[0])
        {
            _print ("ERROR: $args[0]\n", -1);
            return(1);
        }
    }
    elsif ($type eq 'noextension')
    {
        my $progext = join(',', @{$args[1]});
        my $modext  = join(',', @{$args[2]});

        $progext =~ s"\\""g;
        $modext  =~ s"\\""g;

        $progext =~ s"\$""g;
        $modext  =~ s"\$""g;

        _print 
        (
"
ERROR: '$args[0]' does not have a proper extension! Proper extensions are:

    PROGRAM:       $progext 
    SHARED OBJECT: $modext

Use the '-prog' flag to force your files to be interpreted as programs.
Use the '-mod' flag to force your files to be interpreted as modules.
", -1
        );
        return(1);
    }

    return(0);
}

sub _checkopts
{
    my @errors;
    local($") = "\n";

    if ($options->{'log'})
    {
        $_fh = new FileHandle(">> $options->{'log'}") || push(@errors, "ERROR: Couldn't open $options->{'log'}\n");
    }

    if (($options->{'c'}) && (@ARGV > 1) && ($options->{'sav'} ))
    {
        push(@errors, 
"ERROR: The '-sav' and '-C' options are incompatible when you have more than 
       one input file! ('-C' explicitly names resulting C code, '-sav' saves it,
       and hence, with more than one file, the c code will be overwritten for 
       each file that you compile)\n");
    }
    if (($options->{'o'}) && (@ARGV > 1))
    {
        push(@errors, 
"ERROR: The '-o' option is incompatible when you have more than one input file! 
       (-o explicitly names the resulting executable, hence, with more than 
       one file the names clash)\n");
    }

    if ($options->{'e'} && $options->{'sav'} && !$options->{'o'} && 
                                                            !$options->{'C'})
    {
        push(@errors, 
"ERROR: You need to specify where you are going to save the resulting 
       executable or C code,  when using '-sav' and '-e'. Use '-o' or '-C'.\n");
    }

    if (($options->{'regex'} || $options->{'run'} || $options->{'o'}) 
                                                    && $options->{'gen'})
    {
        push(@errors, 
"ERROR: The options '-regex', '-run', and '-o' are incompatible with '-gen'. 
       '-gen' says to stop at C generation, and the other three modify the 
       compilation and/or running process!\n");
    }

    if ($options->{'run'} && $options->{'mod'})
    {
        push(@errors, 
"ERROR: Can't run modules that you are compiling! '-run' and '-mod' are 
       incompatible!\n"); 
    }

    if ($options->{'e'} && @ARGV)
    {
        push (@errors, 
"ERROR: The option '-e' needs to be all by itself without any other 
       file arguments!\n");
    }
    if ($options->{'e'} && !($options->{'o'} || $options->{'run'}))
    {
        $options->{'run'} = 1;
    }

    if (!defined($options->{'verbose'})) 
    { 
        $options->{'verbose'} = ($options->{'log'})? 64 : 7; 
    }

    my $verbose_error;

    if ($options->{'verbose'} =~ m"[^tagfcd]" && 
            !( $options->{'verbose'} eq '0' || 
                ($options->{'verbose'} < 64 && $options->{'verbose'} > 0)))
    {
        $verbose_error = 1;
        push(@errors, 
"ERROR: Illegal verbosity level.  Needs to have either the letters 
       't','a','g','f','c', or 'd' in it or be between 0 and 63, inclusive.\n");
    }

    $options->{'verbose'} = ($options->{'verbose'} =~ m"[tagfcd]")? 
                            ($options->{'verbose'} =~ m"d") * 32 +     
                            ($options->{'verbose'} =~ m"c") * 16 +     
                            ($options->{'verbose'} =~ m"f") * 8     +     
                            ($options->{'verbose'} =~ m"t") * 4     +     
                            ($options->{'verbose'} =~ m"a") * 2     +     
                            ($options->{'verbose'} =~ m"g") * 1     
                                                    : $options->{'verbose'};

    if     (!$verbose_error && (    $options->{'log'} && 
                                !(
                                    ($options->{'verbose'} & 8)   || 
                                    ($options->{'verbose'} & 16)  || 
                                    ($options->{'verbose'} & 32 ) 
                                )
                            )
        )
    {
        push(@errors, 
"ERROR: The verbosity level '$options->{'verbose'}' does not output anything 
       to a logfile, and you specified '-log'!\n");
    } # }

    if     (!$verbose_error && (    !$options->{'log'} && 
                                (
                                    ($options->{'verbose'} & 8)   || 
                                    ($options->{'verbose'} & 16)  || 
                                    ($options->{'verbose'} & 32)  || 
                                    ($options->{'verbose'} & 64)
                                )
                            )
        )
    {
        push(@errors, 
"ERROR: The verbosity level '$options->{'verbose'}' requires that you also 
       specify a logfile via '-log'\n");
    } # }


    (_print( "\n". join("\n", @errors), -1), return(0)) if (@errors);
    return(1);
}

sub _print
{
    my ($text, $flag ) = @_;
    
    my $logflag = int($flag/8) * 8;
    my $regflag = $flag % 8;

    if ($flag == -1 || ($flag & $options->{'verbose'}))
    {
        my $dolog = ((($logflag & $options->{'verbose'}) || $flag == -1) 
                                                        && $options->{'log'}); 

        my $doreg = (($regflag & $options->{'verbose'}) || $flag == -1);
        
        if ($doreg) { print( STDERR $text ); }
        if ($dolog) { print $_fh $text; }
    }
}

sub _run
{
    my ($command, $flag) = @_;

    my $logflag = ($flag != -1)? int($flag/8) * 8 : 0;
    my $regflag = $flag % 8;

    if ($flag == -1 || ($flag & $options->{'verbose'}))
    {
        my $dolog = ($logflag & $options->{'verbose'} && $options->{'log'});
        my $doreg = (($regflag & $options->{'verbose'}) || $flag == -1);

        if ($doreg && !$dolog) 
            { system("$command"); }

        elsif ($doreg && $dolog) 
            { my $text = `$command 2>&1`; print $_fh $text; print STDERR $text;}
        else 
            { my $text = `$command 2>&1`; print $_fh $text; }
    }
    else 
    {
        `$command 2>&1`; 
    }
    return($?);
}

sub _usage
{
    _print
    ( 
    <<"EOF"

Usage: $0 <file_list> 

    Flags with arguments
        -L       < extra library dirs for installation (form of 'dir1:dir2') >
        -I       < extra include dirs for installation (form of 'dir1:dir2') >
        -C       < explicit name of resulting C code > 
        -o       < explicit name of resulting executable >
        -e       < to compile 'one liners'. Need executable name (-o) or '-run'>
        -regex   < rename regex, -regex 's/\.p/\.exe/' compiles a.p to a.exe >
        -verbose < verbose level (1-63, or following letters 'gatfcd' >
        -argv    < arguments for the executables to be run via '-run' or '-e' > 

    Boolean flags
        -gen     ( to just generate the c code. Implies '-sav' )
        -sav     ( to save intermediate c code, (and executables with '-run'))
        -run     ( to run the compiled program on the fly, as were interpreted.)
        -prog    ( to indicate that the files on command line are programs )
        -mod     ( to indicate that the files on command line are modules  )

EOF
, -1

    );
    exit(255);
}


__END__

=head1 NAME

perlcc - frontend for perl compiler

=head1 SYNOPSIS

    %prompt  perlcc a.p        # compiles into executable 'a'

    %prompt  perlcc A.pm       # compile into 'A.so'

    %prompt  perlcc a.p -o execute  # compiles 'a.p' into 'execute'.

    %prompt  perlcc a.p -o execute -run # compiles 'a.p' into execute, runs on
                                        # the fly

    %prompt  perlcc a.p -o execute -run -argv 'arg1 arg2 arg3' 
                                        # compiles into execute, runs with 
                                        # arg1 arg2 arg3 as @ARGV

    %prompt perlcc a.p b.p c.p -regex 's/\.p/\.exe'
                                        # compiles into 'a.exe','b.exe','c.exe'.

    %prompt perlcc a.p -log compilelog  # compiles into 'a', saves compilation
                                        # info into compilelog, as well
                                        # as mirroring to screen

    %prompt perlcc a.p -log compilelog -verbose cdf 
                                        # compiles into 'a', saves compilation
                                        # info into compilelog, being silent
                                        # on screen.

    %prompt perlcc a.p -C a.c -gen      # generates C code (into a.c) and 
                                        # stops without compile.

    %prompt perlcc a.p -L ../lib a.c 
                                        # Compiles with the perl libraries 
                                        # inside ../lib included.

=head1 DESCRIPTION

'perlcc' is the frontend into the perl compiler. Typing 'perlcc a.p'
compiles the code inside a.p into a standalone executable, and 
perlcc A.pm will compile into a shared object, A.so, suitable for inclusion 
into a perl program via "use A".

There are quite a few flags to perlcc which help with such issues as compiling 
programs in bulk, testing compiled programs for compatibility with the 
interpreter, and controlling.

=head1 OPTIONS 

=over 4

=item -L < library_directories >

Adds directories in B<library_directories> to the compilation command.

=item -I  < include_directories > 

Adds directories inside B<include_directories> to the compilation command.

=item -C   < c_code_name > 

Explicitly gives the name B<c_code_name> to the generated c code which is to 
be compiled. Can only be used if compiling one file on the command line.

=item -o   < executable_name >

Explicitly gives the name B<executable_name> to the executable which is to be
compiled. Can only be used if compiling one file on the command line.

=item -e   < perl_line_to_execute>

Compiles 'one liners', in the same way that B<perl -e> runs text strings at 
the command line. Default is to have the 'one liner' be compiled, and run all
in one go (see B<-run>); giving the B<-o> flag saves the resultant executable, 
rather than throwing it away. Use '-argv' to pass arguments to the executable
created.

=item -regex   <rename_regex>

Gives a rule B<rename_regex> - which is a legal perl regular expression - to 
create executable file names.

=item -verbose <verbose_level>

Show exactly what steps perlcc is taking to compile your code. You can change 
the verbosity level B<verbose_level> much in the same way that the '-D' switch 
changes perl's debugging level, by giving either a number which is the sum of 
bits you want or a list of letters representing what you wish to see. Here are 
the verbosity levels so far :

    Bit 1(g):      Code Generation Errors to STDERR
    Bit 2(a):      Compilation Errors to STDERR
    Bit 4(t):      Descriptive text to STDERR 
    Bit 8(f):      Code Generation Errors to file (B<-log> flag needed)
    Bit 16(c):     Compilation Errors to file (B<-log> flag needed)
    Bit 32(d):     Descriptive text to file (B<-log> flag needed) 

If the B<-log> tag is given, the default verbose level is 63 (ie: mirroring 
all of perlcc's output to both the screen and to a log file). If no B<-log>
tag is given, then the default verbose level is 7 (ie: outputting all of 
perlcc's output to STDERR).

NOTE: Because of buffering concerns, you CANNOT shadow the output of '-run' to
both a file, and to the screen! Suggestions are welcome on how to overcome this
difficulty, but for now it simply does not work properly, and hence will only go
to the screen.

=item -log <logname>

Opens, for append, a logfile to save some or all of the text for a given 
compile command. No rewrite version is available, so this needs to be done 
manually.

=item -argv <arguments>

In combination with '-run' or '-e', tells perlcc to run the resulting 
executable with the string B<arguments> as @ARGV.

=item -sav

Tells perl to save the intermediate C code. Usually, this C code is the name
of the perl code, plus '.c'; 'perlcode.p' gets generated in 'perlcode.p.c',
for example. If used with the '-e' operator, you need to tell perlcc where to 
save resulting executables.

=item -gen

Tells perlcc to only create the intermediate C code, and not compile the 
results. Does an implicit B<-sav>, saving the C code rather than deleting it.

=item -run

Immediately run the perl code that has been generated. NOTE: IF YOU GIVE THE 
B<-run> FLAG TO B<perlcc>, THEN THE REST OF @ARGV WILL BE INTERPRETED AS 
ARGUMENTS TO THE PROGRAM THAT YOU ARE COMPILING.

=item -prog

Indicate that the programs at the command line are programs, and should be
compiled as such. B<perlcc> will automatically determine files to be 
programs if they have B<.p>, B<.pl>, B<.bat> extensions.

=item -mod

Indicate that the programs at the command line are modules, and should be
compiled as such. B<perlcc> will automatically determine files to be 
modules if they have the extension B<.pm>.

=back

=head1 ENVIRONMENT

Most of the work of B<perlcc> is done at the command line. However, you can 
change the heuristic which determines what is a module and what is a program.
As indicated above, B<perlcc> assumes that the extensions:

.p$, .pl$, and .bat$

indicate a perl program, and:

.pm$

indicate a library, for the purposes of creating executables. And furthermore,
by default, these extensions will be replaced (and dropped ) in the process of 
creating an executable. 

To change the extensions which are programs, and which are modules, set the
environmental variables:

PERL_SCRIPT_EXT
PERL_MODULE_EXT

These two environmental variables take colon-separated, legal perl regular 
expressions, and are used by perlcc to decide which objects are which. 
For example:

setenv PERL_SCRIPT_EXT  '.prl$:.perl$'
prompt%   perlcc sample.perl

will compile the script 'sample.perl' into the executable 'sample', and

setenv PERL_MODULE_EXT  '.perlmod$:.perlmodule$'

prompt%   perlcc sample.perlmod

will  compile the module 'sample.perlmod' into the shared object 
'sample.so'

NOTE: the '.' in the regular expressions for PERL_SCRIPT_EXT and PERL_MODULE_EXT
is a literal '.', and not a wild-card. To get a true wild-card, you need to 
backslash the '.'; as in:

setenv PERL_SCRIPT_EXT '\.\.\.\.\.'

which would have the effect of compiling ANYTHING (except what is in 
PERL_MODULE_EXT) into an executable with 5 less characters in its name.

=head1 FILES

'perlcc' uses a temporary file when you use the B<-e> option to evaluate 
text and compile it. This temporary file is 'perlc$$.p'. The temporary C code is
perlc$$.p.c, and the temporary executable is perlc$$.

When you use '-run' and don't save your executable, the temporary executable is
perlc$$

=head1 BUGS

perlcc currently cannot compile shared objects on Win32. This should be fixed
by perl5.005.

=cut

!NO!SUBS!

close OUT or die "Can't close $file: $!";
chmod 0755, $file or die "Can't reset permissions for $file: $!\n";
exec("$Config{'eunicefix'} $file") if $Config{'eunicefix'} ne ':';
chdir $origdir;
