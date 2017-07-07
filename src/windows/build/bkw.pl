#!perl -w

#use strict;
use FindBin;
use File::Spec;
use File::Basename;
use lib "$FindBin::Bin/build/lib";
use Getopt::Long;
use Cwd;
use XML::Simple;
use Data::Dumper;
use Archive::Zip;
use Logger;
require "copyfiles.pl";
require "prunefiles.pl";
require "signfiles.pl";
require "zipXML.pl";

my $BAIL;
$0 = fileparse($0);
my $OPT = {foo => 'bar'};
my $MAKE = 'NMAKE';
our $config;

sub get_info {
    my $cmd = shift || die;
    my $which = $^X.' which.pl';
    my $full = `$which $cmd`;
    return 0 if ($? / 256);
    chomp($full);
    $full = "\"".$full."\"";
    return { cmd => $cmd, full => $full};
    }

sub usage {
    print <<USAGE;
Usage: $0 [options] NMAKE-options

  Options are case insensitive.

  Options:
    /help /?           usage information (what you now see).
    /config /f path    Path to config file.  Default is bkwconfig.xml.
    /srcdir /s dir     Source directory to use.  Should contain 
                       pismere/athena.  If cvstag or svntag is null, 
                       the directory should be prepopulated.
    /outdir /o dir     Directory to be created where build results will go
    /repository checkout | co \\  What repository action to take.
       /r       update   | up \\  Options are to checkout, update, export
                export   | ex \\  or skip (take no action).
                skip        
    /username /u name  username used to access svn if checking out.
    /cvstag /c tag     use -r <tag> in cvs command
    /svnbranch /b tag  use /branches/<tag> instead of /trunk.
    /svntag /t tag     use /tags/<tag> instead of /trunk.
    /debug /d          Do debug make instead of release make.
    /[no]make          Control the make step.
    /clean             Build clean target.
    /[no]package       Control the packaging step.
    /[no]sign          Control signing of executable files.
    /verbose /v        Debug mode - verbose output.
    /logfile /l path   Where to write output.  Default is bkw.pl.log.
    /nolog             Don't save output.
  Other:
    NMAKE-options      any options you want to pass to NMAKE, which can be:
                       (note: /nologo is always used)

USAGE
    system("$MAKE /?");
    }

sub handler {
    my $sig = shift;
    my $bailmsg = "Bailing out due to SIG$sig!\n";
    my $warnmsg = <<EOH;
*********************************
* FUTURE BUILDS MAY FAIL UNLESS *
* BUILD DIRECTORIES ARE CLEANED *
*********************************
EOH
    $BAIL = $bailmsg.$warnmsg;
}

sub main {
    local $cmdline = "bkw.pl";
    foreach $arg (@ARGV) {$cmdline .= " $arg";}

    Getopt::Long::Configure('no_bundling', 'no_auto_abbrev',
           'no_getopt_compat', 'require_order',
           'ignore_case', 'pass_through',
           'prefix_pattern=(--|-|\+|\/)',
           );

    local @goargs   = ('config|f=s');
    if (!GetOptions($OPT, @goargs)) {
        Usage();
        exit(0);
        }

    if (! exists $OPT->{config}) {$OPT->{config}  = "bkwconfig.xml";}
    my $configfile  = $OPT->{config};
    print "Info -- Reading configuration from $configfile.\n";
    my $xml         = new XML::Simple();
    $config         = $xml->XMLin($configfile); ## Read in configuration file.

    # Set up convenience variables:
    local $odr  = $config->{Config};    ## Options, directories, repository, environment.

    # Build argument description from Config section of the XML,
    #  to parse the rest of the arguments:
    local @xmlargs;
    while (($sw, $val) = each %$odr) {
        local $arg  = $sw;
        if (exists $val->{abbr})    {$arg .= "|$val->{abbr}";}
        if (exists $val->{value})   {       ## Can't do both negations and string values.
            $arg .= ":s";
            }
        else {    
            if (! ($val->{def} =~ /A/)) {$arg .= "!";}
            }
        push @xmlargs, $arg;
        }

    if (!GetOptions($OPT, @xmlargs)) {$OPT->{help} = 1;}

    if ( $OPT->{help} ) {
        usage();
        exit(0);
        }
        
    delete $OPT->{foo};        

##++ Validate required conditions:

    # List of programs which must be in PATH:'
    # 'cvs', 'svn', 'hhc', 'makensis', 'plink', 'filever'
    my @required_list = ('sed', 'awk', 'which', 'cat', 'rm', 'doxygen',
                         'candle', 'light', 'nmake');
    my $requirements_met    = 1;
    my $first_missing       = 0;
    my $error_list          = "";
    foreach my $required (@required_list) {
        if (!get_info($required)) {
            $requirements_met = 0;
            if (!$first_missing) {
                $first_missing = 1;
                $error_list = "Fatal -- Environment problem!  The following program(s) are not in PATH:\n";
                }
            $error_list .= "$required\n";
            }
        }
    if (!$requirements_met) {
        print $error_list;
        print "Info -- Update PATH or install the programs and try again.\n";
        exit(0);
        }

##-- Validate required conditions.
    
    use Time::gmtime;
    $ENV{DATE} = gmctime()." GMT";
    our $originalDir = `cd`;
    $originalDir =~ s/\n//g;

##++ Assemble configuration from config file and command line:

    my $bOutputCleaned  = 0;
    # Scan the configuration for switch definitions:
    while (($sw, $val) = each %$odr) {
        next if (! exists $val->{def}); ## ?? Should always exist.

        # Set/clear environment variables:
        if ($val->{env}) {
            if ($val->{def})    {$ENV{$sw}   = (exists $val->{value}) ? $val->{value} : 1; }
            else                {
                delete $ENV{$sw};  
                undef  $sw;
                }
            }

        # If the switch is in the command line, override the stored value:
        if (exists $OPT->{$sw}) {
            if (exists $val->{value}) {
                $val->{value}   = $OPT->{$sw};  
                $val->{def}     = 1;
                }
            else {
                $val->{def}   = $OPT->{$sw};  ## If -NO<switch>, value will be zero.
                }
            }
        # If the switch can be negated, test that, too:
        if ( ! ($val->{def} =~ /A/)) {
            local $nosw = "no".$sw;
            if (exists $OPT->{$nosw}) {         ## -NO<environment variable> ?
                if ($val->{env}) {              
                    if (!$val->{def}) {
                        print "Deleting environment variable $sw\n";
                        delete $ENV{$sw};           
                        undef $sw;
                        }
                    }
                }
            }
    
        # For any switch definition with fixed values ("options"), validate:
        if (exists $val->{options}) {
            local $bValid   = 0;
            # options can be like value1|syn1 value2|syn2|syn3
            foreach $option (split(/ /, $val->{options})) {
                local $bFirst   = 1;
                local $sFirst;
                foreach $opt (split(/\|/, $option)) {
                    # opt will be like value2, syn2, syn3
                    if ($bFirst) {
                        $sFirst = $opt; ## Remember the full name of the option.
                        $bFirst = 0;
                        }
                    if ($val->{value} =~ /$opt/i) {
                        $val->{value} = $sFirst;    ## Save the full name.
                        $bValid = 1;
                        }
                    }
                }
            if (! $bValid) {
                print "Fatal -- invalid $sw value $val->{value}.  Possible values are $val->{options}.\n";
                usage();
                die;
                }
            }
        }

    # Set up convenience variables:
    our $verbose    = $odr->{verbose}->{def};
    our $vverbose   = $odr->{vverbose}->{def};
    our $clean      = $clean->{clean}->{def};
    local $src      = $odr->{src}->{value};
    local $out      = $odr->{out}->{value};

    if ($clean && $odr->{package}->{def}) {
        print "Info -- /clean forces /nopackage.\n";
        $odr->{package}->{def} = 0;
        }

    if ($vverbose) {print "Debug -- Config: ".Dumper($config);}
    
    # Test the unix find command:
    # List of directories where it might be:
    my @find_dirs = ('c:\\cygwin\\bin', 'c:\\tools\\cygwin\\bin');
    if (exists $odr->{unixfind}->{value})    {    ## Was an additional place to look specified?
        push (@find_dirs, $odr->{unixfind}->{value});
        }
    my $bFindFound      = 0;
    foreach my $dir (@find_dirs) {
        if (-d $dir) {
            local $savedPATH    = $ENV{PATH};
            $ENV{PATH}          = $dir.";".$savedPATH;
            if (-e "a.tmp") {!system("rm a.tmp")        or die "Fatal -- Couldn't clean temporary file a.tmp.";}
            !system("find . -maxdepth 0 -name a.tmp > b.tmp 2>&1")  or die "Fatal -- find test failed.";
            local $filesize = -s "b.tmp";
            $ENV{PATH} = $savedPATH;
            if ($filesize <= 0) {
                $bFindFound                 = 1;
                $odr->{unixfind}->{value}   = $dir;
                last;
                }
            }
        }
    if (! $bFindFound) {
        print "Fatal -- unix find command not found in \n";
        map {print " $_ "} @find_dirs;
        print "\n";
        die;
        }
                
    # Don't allow /svntag and /svnbranch simultaneously:
    if ( (length $odr->{svntag}->{value} > 0)   && 
         (length $odr->{svnbranch}->{value} > 0) ) {
        die "Fatal -- Can't specify both /SVNTAG and /SVNBRANCH.";
        }

    # /logfile and /nolog interact:
    if ($odr->{nolog}->{def})  {$odr->{logfile}->{def} = 0;}

##-- Assemble configuration from config file and command line.

    local $rverb = $odr->{repository}->{value};
    if ( (($rverb =~ /checkout/) || ($rverb =~ /export/)) && $clean) {
        print "Warning -- Because sources are being checked out, make clean will not be run.\n";
        $clean  = $odr->{clean}->{def}    = 0;
        }

    my $wd  = $src;

    if (! ($rverb =~ /skip/)) {
        local $len = 0;
        if (exists $odr->{username}->{value}) {
            $len = length $odr->{username}->{value};
            }
        if ($len < 1) {
            die "Fatal -- you won't get far accessing the repository without specifying a username.";
            }
        }

    #                (------------------------------------------------)
    if ( (-d $wd) && ( ($rverb =~ /export/) || ($rverb =~ /checkout/) ) ) {
        print "\n\nHEADS UP!!\n\n";
        print "/REPOSITORY ".uc($rverb)." will cause everything under $wd to be deleted.\n";
        print "If this is not what you intended, here's your chance to bail out!\n\n\n";
        print "Are you sure you want to remove everything under $wd? ";
        my $char = getc;
        if (! ($char =~ /y/i))  {die "Info -- operation aborted by user."}
        !system("rm -rf $wd/*") or die "Fatal -- Couldn't clean $wd.";
        !system("rmdir $wd")    or die "Fatal -- Couldn't remove $wd.";
        }

# Begin logging:
    my $l;
    if ($odr->{logfile}->{def}) {
        print "Info -- logging to $odr->{logfile}->{value}.\n";
        $l = new Logger $odr->{logfile}->{value};
        $l->start;
        $l->no_die_handler;        ## Needed so XML::Simple won't throw exceptions.
        }

    print "Command line options:\n";
    while ($v = each %$OPT) {print "$v: $OPT->{$v}\n";}

    print "Executing $cmdline\n";
    local $argvsize     = @ARGV;
    local $nmakeargs    = "";
    if ($argvsize > 0) {
        map {$nmakeargs .= " $_ "} @ARGV;
        print "Arguments for NMAKE: $nmakeargs\n";
        }
       
    print "Info -- Using unix find in $odr->{unixfind}->{value}\n"   if ($verbose);

##++ Begin repository action:
    if ($rverb =~ /skip/) {print "Info -- *** Skipping repository access.\n"    if ($verbose);}
    else {
        if ($verbose) {print "Info -- *** Begin fetching sources.\n";}
        local $cvspath = "$src";
        if (! -d $cvspath) {                        ## xcopy will create the entire path for us.
            !system("echo foo > a.tmp")                     or die "Fatal -- Couldn't create temporary file in ".`cd`;
            !system("echo F | xcopy a.tmp $cvspath\\a.tmp") or die "Fatal -- Couldn't xcopy to $cvspath.";
            !system("rm a.tmp")                             or die "Fatal -- Couldn't remove temporary file.";
            !system("rm $cvspath\\a.tmp")                   or die "Fatal -- Couldn't remove temporary file.";
            }
        
        # Set up cvs environment variables:
        $ENV{CVSROOT}       = $odr->{CVSROOT}->{value};
        local $krb5dir      = "$wd\\athena\\auth\\krb5";

        local $cvscmdroot   = "cvs $rverb";
        if (length $odr->{cvstag}->{value} > 0) {
            $cvscmdroot .= " -r $odr->{cvstag}->{value}";
            }

        if (($rverb =~ /checkout/) || ($rverb =~ /export/)) {        
            chdir($src)                                     or die "Fatal -- couldn't chdir to $src\n";
            print "Info -- chdir to ".`cd`."\n"             if ($verbose);
            my @cvsmodules    = (    
                'krb',  
                'pismere/athena/util/lib/delaydlls', 
                'pismere/athena/util/lib/getopt', 
                'pismere/athena/util/guiwrap'
                );
            foreach my $module (@cvsmodules) {
                local $cvscmd = $cvscmdroot." ".$module;
                if ($verbose) {print "Info -- cvs command: $cvscmd\n";}
                !system("$cvscmd") or die "Fatal -- command \"$cvscmd\" failed; return code $?\n";
                }
            }
        else {                ## Update.
            chdir($wd)                                      or die "Fatal -- couldn't chdir to $wd\n";
            print "Info -- chdir to ".`cd`."\n"             if ($verbose);
            if ($verbose) {print "Info -- cvs command: $cvscmdroot\n";}
            !system($cvscmdroot)    or die "Fatal -- command \"$cvscmdroot\" failed; return code $?\n";
            }

        # Set up svn environment variable:
        $ENV{SVN_SSH} = "plink.exe";
        # If  the directory structure doesn't exist, many cd commands will fail.
        if (! -d $krb5dir) {                                ## xcopy will create the entire path for us.
            !system("echo foo > a.tmp")                     or die "Fatal -- Couldn't create temporary file in ".`cd`;
            !system("echo F | xcopy a.tmp $krb5dir\\a.tmp") or die "Fatal -- Couldn't xcopy to $krb5dir.";
            !system("rm a.tmp")                             or die "Fatal -- Couldn't remove temporary file.";
            !system("rm $krb5dir\\a.tmp")                   or die "Fatal -- Couldn't remove temporary file.";
            }

        chdir($krb5dir)                                 or die "Fatal -- Couldn't chdir to $krb5dir";
        print "Info -- chdir to ".`cd`."\n"             if ($verbose);
        my $svncmd = "svn $rverb ";
        if (($rverb =~ /checkout/) || ($rverb =~ /export/)) {        # Append the rest of the checkout/export command:
            chdir("..");
            if ($rverb =~ /export/) {
                ## svn export will fail if the destination directory exists
                rmdir "krb5";
            }       
            $svncmd .= "svn+ssh://".$odr->{username}->{value}."@".$odr->{SVNURL}->{value}."/krb5/";
            if (length $odr->{svntag}->{value} > 0) {
                $svncmd .= "tags/$odr->{svntag}->{value}";
                }
            elsif (length $odr->{svnbranch}->{value} > 0) {
                $svncmd .= "branches/$odr->{svnbranch}->{value}";
                }
            else {
                $svncmd .= "trunk";
                }

            $svncmd .= " krb5";

            }
        if ($verbose) {print "Info -- svn command: $svncmd\n";}
        !system($svncmd)            or die "Fatal -- command \"$svncmd\" failed; return code $?\n";
        if ($verbose) {print "Info -- ***   End fetching sources.\n";}
        }
##-- End  repository action.
        
    ##++ Read in the version information to be able to update the 
    #  site-local files in the install build areas.
    # ** Do this now (after repository update and before first zip) 
    #    because making zip files requires some configuration data be set up.
    local $version_path = $config->{Stages}->{Package}->{Config}->{Paths}->{Versions}->{path};
    open(DAT, "$src/$version_path")     or die "Could not open $src/$version_path.";
    @raw = <DAT>;
    close DAT;
    foreach $line (@raw) {
        chomp $line;
        if ($line =~ /#define/) {                   # Process #define lines:
            $line =~ s/#define//;                   # Remove #define token
            $line =~ s/^\s+//;                      #  and leading & trailing whitespace
            $line =~ s/\s+$//;
            local @qr = split("\"", $line);         # Try splitting with quotes
            if (exists $qr[1]) {
                $qr[0] =~ s/^\s+//;                 #  Clean up whitespace
                $qr[0] =~ s/\s+$//;
                $config->{Versions}->{$qr[0]} = $qr[1]; # Save string
                }
            else {                                  # No quotes, so
                local @ar = split(" ", $line);      #  split with space
                $ar[0] =~ s/^\s+//;                 #  Clean up whitespace
                $ar[0] =~ s/\s+$//;
                $config->{Versions}->{$ar[0]} = $ar[1]; # and  save numeric value
                }
            }
        }
    
    # Check that the versions we will need for site-local have been defined:
    my @required_versions = ('VER_PROD_MAJOR', 'VER_PROD_MINOR', 'VER_PROD_REV', 
                             'VER_PROD_MAJOR_STR', 'VER_PROD_MINOR_STR', 'VER_PROD_REV_STR', 
                             'VER_PRODUCTNAME_STR');
    $requirements_met   = 1;
    $first_missing      = 0;
    $error_list         = "";
    foreach my $required (@required_versions) {
        if (! exists $config->{Versions}->{$required}) {
            $requirements_met = 0;
            if (!$first_missing) {
                $first_missing = 1;
                $error_list = "Fatal -- The following version(s) are not defined in $src/$version_path.\n";
                }
            $error_list .= "$required\n";
            }
        }
    if (!$requirements_met) {
        print $error_list;
        exit(0);
        }
    
    # Apply any of these tags to filestem:
    my $filestem    = $config->{Stages}->{PostPackage}->{Config}->{FileStem}->{name};
    $filestem       =~ s/%VERSION_MAJOR%/$config->{Versions}->{'VER_PROD_MAJOR_STR'}/;
    $filestem       =~ s/%VERSION_MINOR%/$config->{Versions}->{'VER_PROD_MINOR_STR'}/;
    $filestem       =~ s/%VERSION_PATCH%/$config->{Versions}->{'VER_PROD_REV_STR'}/;
    $config->{Stages}->{PostPackage}->{Config}->{FileStem}->{name}    = $filestem;
    ##-- Read in the version information & set config info.

##++ Repository action, part 2:
    if (($rverb =~ /checkout/) || ($rverb =~ /export/)) {        
       if (! $bOutputCleaned) {                    ## In case somebody cleaned $out before us.
           if (-d $out)    {!system("rm -rf $out/*")   or die "Fatal -- Couldn't clean $out."}    ## Clean output directory.
           else            {mkdir($out);}
           $bOutputCleaned = 1;
           }
       zipXML($config->{Stages}->{FetchSources}, $config); ## Make zips.
       }
##-- End  repository action, part 2.

##++ Make action:
    if (    ($odr->{make}->{def}) ) {
        if ($verbose) {print "Info -- *** Begin preparing for build.\n";}

        chdir("$wd") or die "Fatal -- couldn't chdir to $wd\n";
        print "Info -- chdir to ".`cd`."\n"             if ($verbose);
    
        my ($path, $destpath);
        
        # Copy athena\scripts\site\graft\krb5\Makefile.src to athena\auth\krb5:
        $path = "scripts\\site\\graft\\krb5\\Makefile.src";
        if (!-e  $path) {die "Fatal -- Expected file $wd\\$path not found.";}
        $destpath = "athena\\auth\\krb5\\Makefile.src";
        !system("echo F | xcopy /D $wd\\$path $wd\\$destpath /Y > NUL") or die "Fatal -- Copy of $wd\\$path to $wd\\$destpath failed.";
        print "Info -- copied $wd\\$path to $wd\\$destpath\n"   if ($verbose);;
        
        # Add DEBUG_SYMBOL to .../wshelper/Makefile.src:
        $path = "athena\\wshelper\\wshelper\\Makefile.src";
        if (!-e  $path) {die "Fatal -- Expected file $wd\\$path not found.";}
        if (system("grep DEBUG_SYMBOL $path > NUL") != 0) {
            !system ("echo DEBUG_SYMBOL=1 >> $wd\\$path") or die "Fatal -- Append line to file failed.\n";
            print "Info -- Added DEBUG_SYMBOL to $wd\\$path\n"  if ($verbose);
            }
        
        # Prune any unwanted directories before the build:
        pruneFiles($config->{Stages}->{Make}, $config);

        if ($verbose) {print "Info -- ***   End preparing for build.\n";}
    
        my ($buildtarget, $buildtext);
        if ($clean) {
            $buildtarget = "clean" ;
            $buildtext   = " clean."
            }
        else {
            $buildtarget = "" ;
            $buildtext   = "."
            }
        
        chdir("$wd\\athena") or die "Fatal -- couldn't chdir to source directory $wd\\athena\n";
        print "Info -- chdir to ".`cd`."\n"         if ($verbose);
        local $dbgswitch = ($odr->{debug}->{def}) ? " " : "NODEBUG=1";
        !system("perl ../scripts/build.pl --softdirs --nolog $buildtarget $dbgswitch BUILD_KFW=1 BUILD_OFFICIAL=1 DEBUG_SYMBOL=1 $nmakeargs")
            or die "Fatal -- build $buildtarget failed.";
            
        chdir("$wd")                        or die "Fatal -- couldn't chdir to $wd.";
        print "Info -- chdir to ".`cd`."\n" if ($verbose);
        if ($clean) {
            if (-d "staging") {
                !system("rm -rf staging")   or die "Fatal -- Couldn't remove $wd\\staging.";
                }
            }
    
        if ($verbose) {print "Info -- ***   End build".$buildtext."\n";}
        }                                           ## End make conditional.
    else {print "Info -- *** Skipping build.\n"    if ($verbose);}
##-- Make action.
        
##++ Package action:
    if (! $odr->{package}->{def}) {      ## If /clean, nopackage will be set.
        print "Info -- *** Skipping packaging.\n";
        if ((-d $out) && ! $bOutputCleaned) {
            print "Warning -- *** Output directory $out will not be cleaned.\n";
            }
        }
    else {
        if ($verbose) {print "Info -- *** Begin prepackage.\n";}

        if (! $bOutputCleaned) {                        ## In case somebody cleaned $out before us.
            if (-d $out)    {!system("rm -rf $out/*")   or die "Fatal -- Couldn't clean $out."}    ## Clean output directory.
            else            {mkdir($out);}
            $bOutputCleaned = 1;
            }

        # The build results are copied to a staging area, where the packager expects to find them.
        #  We put the staging area in the fixed area .../pismere/staging.
        #my $prepackage  = $config->{Stages}->{PrePackage};
        #my $staging     = "$wd\\staging";
        #chdir($wd)                          or die "Fatal -- couldn't chdir to $wd\n";
        #print "Info -- chdir to ".`cd`."\n" if ($verbose);
        #if (-d "staging") {
        #    !system("rm -rf $staging/*")        or die "Fatal -- Couldn't clean $staging.";
        #    }
        #else {
        #    mkdir($staging)                     or die "Fatal -- Couldn't create $staging.";
        #    }
        
        # Force Where From and To are relative to:
        #$prepackage->{CopyList}->{Config}->{From}->{root}   = "$wd\\athena";
        #$prepackage->{CopyList}->{Config}->{To}->{root}     = "$wd\\staging";
        #copyFiles($prepackage->{CopyList}, $config);        ## Copy any files [this step takes a while]

        # Sign files:
        #chdir($staging) or die "Fatal -- couldn't chdir to $staging\n";
        #print "Info -- chdir to ".`cd`."\n"     if ($verbose);
        #if ($odr->{sign}->{def}) {
        #    signFiles($config->{Stages}->{PostPackage}->{Config}->{Signing}, $config);
        #    }
            
        # Create working directories for building the installers:
        if (-d "$wd\\buildwix")    {!system("rm -rf $wd\\buildwix/*")               or die "Fatal -- Couldn't clean $wd\\buildwix."}    
        !system("echo D | xcopy /s $wd\\windows\\installer\\wix\\*.* $wd\\buildwix")  or die "Fatal -- Couldn't create $wd\\buildwix.";
        #if (-d "$wd\\buildnsi")    {!system("rm -rf $wd\\buildnsi/*")               or die "Fatal -- Couldn't clean $wd\\buildnsi."}
        #!system("echo D | xcopy /s $wd\\staging\\install\\nsis\\*.* $wd\\buildnsi") or die "Fatal -- Couldn't create $wd\\buildnsi.";

        chdir("$wd\\windows\\installer\\wix") or die "Fatal -- Couldn't cd to $wd\\windows\\installer\\wix";
        print "Info -- chdir to ".`cd`."\n"     if ($verbose);
        # Correct errors in files.wxi:
        #!system("sed 's/WorkingDirectory=\"\\[dirbin\\]\"/WorkingDirectory=\"dirbin\"/g' files.wxi > a.tmp") or die "Fatal -- Couldn't modify files.wxi.";
        #!system("mv a.tmp files.wxi") or die "Fatal -- Couldn't update files.wxi.";
            
        # Make sed script to run on the site-local configuration files:
        local $tmpfile      = "site-local.sed" ;
        if (-e $tmpfile) {system("del $tmpfile");}
        # Basic substitutions:
        local $dblback_wd   = $wd;
        $dblback_wd         =~ s/\\/\\\\/g;
        !system("echo s/%BUILDDIR%/$dblback_wd/ >> $tmpfile")               or die "Fatal -- Couldn't modify $tmpfile.";    
        local $dblback_staging  = "$wd\\staging";
        $dblback_staging        =~ s/\\/\\\\/g;
        !system("echo s/%TARGETDIR%/$dblback_staging/ >> $tmpfile")         or die "Fatal -- Couldn't modify $tmpfile.";    
        local $dblback_sample   = "$wd\\staging\\sample";
        $dblback_sample         =~ s/\\/\\\\/g;
        !system("echo s/%CONFIGDIR-WIX%/$dblback_sample/ >> $tmpfile")      or die "Fatal -- Couldn't modify $tmpfile.";    
        !system("echo s/%CONFIGDIR-NSI%/$dblback_staging/ >> $tmpfile")     or die "Fatal -- Couldn't modify $tmpfile.";    
        !system("echo s/%VERSION_MAJOR%/$config->{Versions}->{'VER_PROD_MAJOR_STR'}/ >> $tmpfile")  or die "Fatal -- Couldn't modify $tmpfile.";    
        !system("echo s/%VERSION_MINOR%/$config->{Versions}->{'VER_PROD_MINOR_STR'}/ >> $tmpfile")  or die "Fatal -- Couldn't modify $tmpfile.";    
        !system("echo s/%VERSION_PATCH%/$config->{Versions}->{'VER_PROD_REV_STR'}/ >> $tmpfile")    or die "Fatal -- Couldn't modify $tmpfile.";    
        # Strip out some defines so they can be replaced:  [used for site-local.nsi]
        !system("echo /\^!define\.\*RELEASE\.\*\$/d >> $tmpfile")           or die "Fatal -- Couldn't modify $tmpfile.";    
        !system("echo /\^!define\.\*DEBUG\.\*\$/d >> $tmpfile")             or die "Fatal -- Couldn't modify $tmpfile.";    
        !system("echo /\^!define\.\*BETA\.\*\$/d >> $tmpfile")              or die "Fatal -- Couldn't modify $tmpfile.";    

        # Run the script on site-local.wxi:
        !system("sed -f $tmpfile site-local-tagged.wxi > $wd\\buildwix\\site-local.wxi")   or die "Fatal -- Couldn't modify site-local.wxi.";

        # Now update site-local.nsi:
        #chdir "..\\nsis";
        #print "Info -- chdir to ".`cd`."\n"                                 if ($verbose);
        #!system("sed -f ..\\wix\\$tmpfile site-local-tagged.nsi > b.tmp")   or die "Fatal -- Couldn't modify site-local.wxi.";
        # Add DEBUG or RELEASE:
        #if ($odr->{debug}->{def}) {                    ## debug build
        #    !system("echo !define DEBUG >> b.tmp")     or die "Fatal -- Couldn't modify b.tmp.";
        #    }
        #else {                                         ## release build
        #    !system("echo !define RELEASE >> b.tmp")   or die "Fatal -- Couldn't modify b.tmp.";
        #    }
        # Add BETA if present:
        #if (exists $config->{Versions}->{'BETA_STR'}) {
        #    !system("echo !define BETA $config->{Versions}->{'BETA_STR'} >> b.tmp") or die "Fatal -- Couldn't modify b.tmp.";
        #    }
        #!system("mv -f b.tmp $wd\\buildnsi\\site-local.nsi")                        or die "Fatal -- Couldn't replace site-local.nsi.";

        # Run the script on nsi-includes-tagged.nsi:
        #!system("sed -f ..\\wix\\$tmpfile nsi-includes-tagged.nsi > $wd\\buildnsi\\nsi-includes.nsi")  or die "Fatal -- Couldn't modify nsi-includes.nsi.";
        #!system("rm ..\\wix\\$tmpfile")                                     or die "Fatal -- Couldn't remove $tmpfile.";

        #if ($verbose) {print "Info -- ***   End prepackage.\n";}
        
        #if ($verbose) {print "Info -- *** Begin package.\n";}
        # Make the msi:
        chdir("$wd\\buildwix")                      or die "Fatal -- Couldn't cd to $wd\\buildwix";
        print "Info -- *** Make .msi:\n"            if ($verbose);
        print "Info -- chdir to ".`cd`."\n"         if ($verbose);
        !system("$MAKE")                            or die "Error -- msi installer build failed.";
                
        #chdir("$wd\\buildnsi")                      or die "Fatal -- Couldn't cd to $wd\\buildnsi";
        #print "Info -- *** Make NSIS:\n"            if ($verbose);
        #print "Info -- chdir to ".`cd`."\n"         if ($verbose);
        #!system("cl.exe killer.cpp advapi32.lib")   or die "Error -- nsis killer.exe not built.";
        #!system("rename killer.exe Killer.exe")     or die "Error -- Couldn't rename killer.exe";
        #!system("makensis kfw.nsi")                 or die "Error -- executable installer build failed.";

# Begin packaging extra items:
        chdir($wd)                                  or die "Fatal -- Couldn't cd to $wd";
        print "Info -- chdir to ".`cd`."\n"         if ($verbose);

        zipXML($config->{Stages}->{PostPackage}, $config);                      ## Make zips.

        $config->{Stages}->{PostPackage}->{CopyList}->{Config} = $config->{Stages}->{PostPackage}->{Config};    ## Use the post package config.
        $config->{Stages}->{PostPackage}->{CopyList}->{Config}->{From}->{root}  = "$src\\pismere";
        $config->{Stages}->{PostPackage}->{CopyList}->{Config}->{To}->{root}    = $out;
        copyFiles($config->{Stages}->{PostPackage}->{CopyList}, $config);       ## Copy any files

        !system("rm -rf $wd\\buildwix")             or die "Fatal -- Couldn't remove $wd\\buildwix.";
        !system("rm -rf $wd\\buildnsi")             or die "Fatal -- Couldn't remove $wd\\buildnsi.";

        chdir($out)                                 or die "Fatal -- Couldn't cd to $out";
        print "Info -- chdir to ".`cd`."\n"         if ($verbose);
        if ($odr->{sign}->{def}) {
            signFiles($config->{Stages}->{PostPackage}->{Config}->{Signing}, $config);
            }

        if ($verbose) {print "Info -- ***   End package.\n";}
        }
##-- Package action.

    system("rm -rf $src/a.tmp");                ## Clean up junk.
    system("rm -rf $out/a.tmp");                ## Clean up junk.
    system("rm -rf $out/ziptemp");              ## Clean up junk.

# End logging:
    if ($odr->{logfile}->{def})   {$l->stop;}

    return 0;
    }                                           ## End subroutine main.

$SIG{'INT'} = \&handler;
$SIG{'QUIT'} = \&handler;

exit(main());
