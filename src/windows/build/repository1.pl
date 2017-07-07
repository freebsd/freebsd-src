#!perl -w

#use strict;

sub repository1 {
    local ($config) = @_;
    local $odr      = $config->{Config};    ## Options, directories, repository, environment.
    local $src      = $odr->{src}->{value};
    local $rverb    = $odr->{repository}->{value};
    local $wd       = $src."\\pismere";

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

        if ($rverb =~ /checkout/) {        
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
                !system($cvscmd)    or die "Fatal -- command \"$cvscmd\" failed; return code $?\n";
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
        if ($rverb =~ /checkout/) {        # Append the rest of the checkout command:
            chdir("..");
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
    }

return 1;