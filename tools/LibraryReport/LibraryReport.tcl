#!/bin/sh
# tcl magic \
exec tclsh $0 $*
################################################################################
#
# LibraryReport; produce a list of shared libraries on the system, and a list of
# all executables that use them.
#
################################################################################
#
# Stage 1 looks for shared libraries; the output of 'ldconfig -r' is examined
# for hints as to where to look for libraries (but not trusted as a complete
# list).
#
# These libraries each get an entry in the global 'Libs()' array.
#
# Stage 2 walks the entire system directory heirachy looking for executable
# files, applies 'ldd' to them and attempts to determine which libraries are
# used.  The path of the executable is then added to the 'Libs()' array 
# for each library used.
#
# Stage 3 reports on the day's findings.
#
################################################################################
#
# $Id$
#

#########################################################################################
# findLibs
#
# Ask ldconfig where it thinks libraries are to be found.  Go look for them, and
# add an element to 'Libs' for everything that looks like a library.
#
proc findLibs {} {

    global Libs stats verbose;

    # Older ldconfigs return a junk value when asked for a report
    if {[catch {set liblist [exec ldconfig -r]} err]} {	# get ldconfig output
	puts stderr "ldconfig returned nonzero, persevering.";
	set liblist $err;				# there's junk in this
    }

    # remove hintsfile name, convert to list
    set liblist [lrange [split $liblist "\n"] 1 end];

    set libdirs "";				# no directories yet
    foreach line $liblist {
	# parse ldconfig output
	if {[scan $line "%s => %s" junk libname] == 2} {
	    # find directory name
	    set libdir [file dirname $libname];
	    # have we got this one already?
	    if {[lsearch -exact $libdirs $libdir] == -1} {
		lappend libdirs $libdir;
	    }
	} else {
	    puts stderr "Unparseable ldconfig output line :";
	    puts stderr $line;
	}
    }
    
    # libdirs is now a list of directories that we might find libraries in
    foreach dir $libdirs {
	# get the names of anything that looks like a library
	set libnames [glob -nocomplain "$dir/lib*.so.*"]
	foreach lib $libnames {
	    set Libs($lib) "";			# add it to our list
	    if {$verbose} {puts "+ $lib";}
	}
    }
    set stats(libs) [llength [array names Libs]];
}

################################################################################
# findLibUsers
#
# Look in the directory (dir) for executables.  If we find any, call 
# examineExecutable to see if it uses any shared libraries.  Call ourselves
# on any directories we find.
#
# Note that the use of "*" as a glob pattern means we miss directories and
# executables starting with '.'.  This is a Feature.
#
proc findLibUsers {dir} {

    global stats verbose;

    if {[catch {
	set ents [glob -nocomplain "$dir/*"];
    } msg]} {
	if {$msg == ""} {
	    set msg "permission denied";
	}
	puts stderr "Can't search under '$dir' : $msg";
	return ;
    }

    if {$verbose} {puts "===>> $dir";}
    incr stats(dirs);

    # files?
    foreach f $ents {
	# executable?
	if {[file executable $f]} {
	    # really a file?
	    if {[file isfile $f]} {
		incr stats(files);
		examineExecutable $f;
	    }
	}
    }
    # subdirs?
    foreach f $ents {
	# maybe a directory with more files?
	# don't use 'file isdirectory' because that follows symlinks
	if {[catch {set type [file type $f]}]} {
	    continue ;		# may not be able to stat
	}
	if {$type == "directory"} {
	    findLibUsers $f;
	}
    }
}

################################################################################
# examineExecutable
#
# Look at (fname) and see if ldd thinks it references any shared libraries.
# If it does, update Libs with the information.
#
proc examineExecutable {fname} {

    global Libs stats verbose;

    # ask Mr. Ldd.
    if {[catch {set result [exec ldd $fname]} msg]} {
	return ;	# not dynamic
    }

    if {$verbose} {puts -nonewline "$fname : ";}
    incr stats(execs);

    # For a non-shared executable, we get a single-line error message.
    # For a shared executable, we get a heading line, so in either case
    # we can discard the first line and any subsequent lines are libraries
    # that are required.
    set llist [lrange [split $result "\n"] 1 end];
    set uses "";

    foreach line $llist {
	if {[scan $line "%s => %s %s" junk1 lib junk2] == 3} {
	    lappend Libs($lib) $fname;
	    lappend uses $lib;
	} else {
	    puts stderr "Unparseable ldd putput line :";
	    puts stderr $line;
	}
    }
    if {$verbose} {puts "$uses";}
}

################################################################################
# emitLibDetails
#
# Emit a listing of libraries and the executables that use them.
#
proc emitLibDetails {} {

    global Libs;

    # divide into used/unused
    set used "";
    set unused "";
    foreach lib [array names Libs] {
	if {$Libs($lib) == ""} {
	    lappend unused $lib;
	} else {
	    lappend used $lib;
	}
    }

    # emit used list
    puts "== Current Shared Libraries ==================================================";
    foreach lib [lsort $used] {
	# sort executable names
	set users [lsort $Libs($lib)];
	puts [format "%-30s  %s" $lib $users];
    }
    # emit unused
    puts "== Stale Shared Libraries ====================================================";
    foreach lib [lsort $unused] {
	# sort executable names
	set users [lsort $Libs($lib)];
	puts [format "%-30s  %s" $lib $users];
    }
}

################################################################################
# Run the whole shebang
#
proc main {} {

    global stats verbose argv;

    set verbose 0;
    foreach arg $argv {
	switch -- $arg {
	    -v {
		set verbose 1;
	    }
	    default {
		puts stderr "Unknown option '$arg'";
		exit ;
	    }
	}
    }

    set stats(libs) 0;
    set stats(dirs) 0;
    set stats(files) 0;
    set stats(execs) 0

    findLibs;
    findLibUsers "/";
    emitLibDetails;

    puts [format "Searched %d directories, %d executables (%d dynamic) for %d libraries" \
	      $stats(dirs) $stats(files) $stats(execs) $stats(libs)];
}

################################################################################
main;
