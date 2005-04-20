#!/usr/bin/perl
# -----------------------------------------------------------------
#  FreeBSD Release Checking Utility - Package Index Check
#
#  This program checks the packages/INDEX file to verify that
#  the index is in the correct format and that every package
#  needed by a release is included on the CD.
#
#  Copyright(c) 2000 BSDi
#  Murray Stokely
# -----------------------------------------------------------------
# 08 Apr 2000
#
# $FreeBSD$
#

use Getopt::Long;

#
# Display the usage instructions
#

sub printHelp {
    print<<end;
usage : checkindex -s <sysinstall src dir> <INDEX>

  This program checks the packages INDEX file to verify that the 
index is correct and that every package needed by sysinstall is
included in the index.

  Options

     -help                Display usage instructions
     -s <src dir>         Specify the sysinstall source directory.  Use
	                  this so to make sure every package referenced
                          in the code is in your INDEX
     -newindex            Generate a new index consisting of only those
                          packages that actually exist in pkgdir/All
     -depends <pkg>       Lists all packages in the index that depend
                          on <pkg>.

end
}

##
## Attempts to find the value of a variable in C code by backtracking
## up the source looking for a previous declaration.
## 
## This is a bit overkill for the purpose of this script,
## stick with grepping for likely packages for now.

sub findAssignment($$) {
	    ## This code deals with the small (5%) number of matches
	    ## in which package_add refers to a variable rather than
	    ## a inline string, so we have to find the value of that
	    ## variable so that we can push it onto the list
#	    my($fileName,$code) = split(/:/,$match);
#	    open(FILE,$fileName) || die "Could not open $fileName : $!\n";
#	    my(@lines) = <FILE>;
#	    my($cnt)  = 1;
#	    my($lineMatch) = 0;
#	    chomp(@lines);
#	    foreach $line (@lines) {
#		$lineMatch = $cnt if ($line eq $code);
#		$cnt++;
#	    }
#	    $code =~ /package_add\((\S+)\)/;
#	    my($varName) = $1;
#	    print STDERR "$lineMatch of $fileName is wierd\n";
#	    print STDERR "Trying to find '$varName'\n";
#	    while ($cnt > 0) {
#		$cnt--;
#	    }


}

##
## Returns a list of all the packages referenced in the sysinstall source
## code
##

sub getPackages($) {
    my($srcDir) = $_[0];
    my(@matches) = `grep package_add $opt_s/*.c`;
    my(@packages);
    foreach $match (@matches) {
	chomp $match;
	next if ($match =~ m|$opt_s/package.c|);
	if ($match =~ /package_add\(\"(\S+)\"\)/) {
	    push(@packages,$1);
	} elsif ($match =~ /package_add\(char/) {
	    # function definition or prototype
	    next;
	} else {
	    # package_add(variable or DEFINE)
	    my(@varMatches) = `grep variable_set2 $opt_s/*.c`;
	    chomp @varMatches;
	    foreach $varMatch (@varMatches) {
		if ($varMatch =~ /variable_set2\(\S+_PACKAGE,\s+\"(\S+)\"/) {
		    push(@packages,$1);
		}
	    }
	}
    }
    @packages;
}


&GetOptions("help","s=s","newindex","depends=s");
if ($opt_help) {
    &printHelp;
} else{
    my ($indexName) = $ARGV[0];
    my ($mistakes) = 0;
    my ($counter)  = 0;
    print STDERR "Packages Referenced :\n---------------------\n";
    open(INDEX,$indexName) || die "Could not open $indexName : $!";
    @index = <INDEX>;
    close(INDEX);

    ## Check to ensure that every file in the index exists physically.
    print STDERR "Check to ensure that every file in the index exists physically\n";
    foreach $line (@index) {
	chomp $line;
	($file,$pathto,$prefix,$comment,$descr,$maint,$cats,$junk,$rdeps,$junk) = split(/\|/,$line,10);
	$DEPENDS{$file} = $rdeps if (-e "All/$file.tgz");
    }

    if ($opt_newindex) {
	foreach $pkg (keys %DEPENDS) {
	    $new = quotemeta $pkg;
	    @lines = grep(/^$new\|/,@index);
	    chomp $lines;
	    ($#lines == 0) || die "Multiple lines for '$pkg' in index!";
	    printf "%s\n",$lines[0];
	}
    } elsif ($opt_depends) {
	foreach $key (keys %DEPENDS) {
	    foreach $dependency (split ' ',$DEPENDS{$key}) {
		if ($opt_depends eq $dependency) {
		    print "$opt_depends is needed by $key\n";
		    $counter++;
		}
	    }
	}
	print "$opt_depends is not needed by any packages in the index!\n"
	    unless ($counter);
    } else {

    ## Check to ensure that all the dependencies are there.
    print "Check to make sure that every dependency of every file exists\n",
  	"in the Index and physically.\n";
    foreach $file (keys %DEPENDS) {
#	print "Checking $file\n";
	foreach $depend (split(' ',$DEPENDS{$file})) {
	    unless (-e "All/$depend.tgz") {
		# instead of a hash counter, make it a hash of arrays
		# where the arrays are the files depended on.
		push @{ $MISSING{$depend} }, $file;
		$mistakes++;
	    }
	}
    }

    ## This makes sure that the index file contains everything
    ## that sysinstall uses.
    if ($opt_s) {
	@packages = getPackages($opt_s);
	foreach $pkg (@packages) {
	    unless (grep(/^$pkg/,@index)) {
		push @{ $MISSING{$pkg} }, "sysinstall";
		$mistakes++;
	    }
	}
    }


    ## If there were mistakes, print out the missing packages.
    if ($mistakes) {
	print "--------------------------------------------------------\n",
	      " Packages Missing : \n",
              "--------------------------------------------------------\n";
	foreach $pkg (keys %MISSING) {
	    @files = @{ $MISSING{$pkg} };
	    print "$pkg (@files)\n";
	}
    } else {
	print "Everything looks good!\n";
    }
}
}
