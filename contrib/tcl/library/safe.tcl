# safe.tcl --
#
# This file provide a safe loading/sourcing mechanism for safe interpreters.
# It implements a virtual path mecanism to hide the real pathnames from the
# slave. It runs in a master interpreter and sets up data structure and
# aliases that will be invoked when used from a slave interpreter.
# 
# See the safe.n man page for details.
#
# Copyright (c) 1996-1997 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# SCCS: @(#) safe.tcl 1.26 97/08/21 11:57:20

#
# The implementation is based on namespaces. These naming conventions
# are followed:
# Private procs starts with uppercase.
# Public  procs are exported and starts with lowercase
#

# Needed utilities package
package require opt 0.2;

# Create the safe namespace
namespace eval ::safe {

    # Exported API:
    namespace export interpCreate interpInit interpConfigure interpDelete \
	    interpAddToAccessPath interpFindInAccessPath \
	    setLogCmd ;

# Proto/dummy declarations for auto_mkIndex
proc ::safe::interpCreate {} {}
proc ::safe::interpInit {} {}
proc ::safe::interpConfigure {} {}


    ####
    #
    # Setup the arguments parsing
    #
    ####

    # Share the descriptions
    set temp [::tcl::OptKeyRegister {
	{-accessPath -list {} "access path for the slave"}
	{-noStatics "prevent loading of statically linked pkgs"}
	{-statics true "loading of statically linked pkgs"}
	{-nestedLoadOk "allow nested loading"}
	{-nested false "nested loading"}
	{-deleteHook -script {} "delete hook"}
    }]

    # create case (slave is optional)
    ::tcl::OptKeyRegister {
	{?slave? -name {} "name of the slave (optional)"}
    } ::safe::interpCreate ;
    # adding the flags sub programs to the command program
    # (relying on Opt's internal implementation details)
    lappend ::tcl::OptDesc(::safe::interpCreate) $::tcl::OptDesc($temp);

    # init and configure (slave is needed)
    ::tcl::OptKeyRegister {
	{slave -name {} "name of the slave"}
    } ::safe::interpIC;
    # adding the flags sub programs to the command program
    # (relying on Opt's internal implementation details)
    lappend ::tcl::OptDesc(::safe::interpIC) $::tcl::OptDesc($temp);
    # temp not needed anymore
    ::tcl::OptKeyDelete $temp;


    # Helper function to resolve the dual way of specifying staticsok
    # (either by -noStatics or -statics 0)
    proc InterpStatics {} {
	foreach v {Args statics noStatics} {
	    upvar $v $v
	}
	set flag [::tcl::OptProcArgGiven -noStatics];
	if {$flag && ($noStatics == $statics) 
	          && ([::tcl::OptProcArgGiven -statics])} {
	    return -code error\
		    "conflicting values given for -statics and -noStatics";
	}
	if {$flag} {
	    return [expr {!$noStatics}];
	} else {
	    return $statics
	}
    }

    # Helper function to resolve the dual way of specifying nested loading
    # (either by -nestedLoadOk or -nested 1)
    proc InterpNested {} {
	foreach v {Args nested nestedLoadOk} {
	    upvar $v $v
	}
	set flag [::tcl::OptProcArgGiven -nestedLoadOk];
	# note that the test here is the opposite of the "InterpStatics"
	# one (it is not -noNested... because of the wanted default value)
	if {$flag && ($nestedLoadOk != $nested) 
	          && ([::tcl::OptProcArgGiven -nested])} {
	    return -code error\
		    "conflicting values given for -nested and -nestedLoadOk";
	}
	if {$flag} {
	    # another difference with "InterpStatics"
	    return $nestedLoadOk
	} else {
	    return $nested
	}
    }

    ####
    #
    #  API entry points that needs argument parsing :
    #
    ####


    # Interface/entry point function and front end for "Create"
    proc interpCreate {args} {
	set Args [::tcl::OptKeyParse ::safe::interpCreate $args]
	InterpCreate $slave $accessPath \
		[InterpStatics] [InterpNested] $deleteHook;
    }

    proc interpInit {args} {
	set Args [::tcl::OptKeyParse ::safe::interpIC $args]
	if {![::interp exists $slave]} {
	    return -code error \
		    "\"$slave\" is not an interpreter";
	}
	InterpInit $slave $accessPath \
		[InterpStatics] [InterpNested] $deleteHook;
    }

    proc CheckInterp {slave} {
	if {![IsInterp $slave]} {
	    return -code error \
		    "\"$slave\" is not an interpreter managed by ::safe::" ;
	}
    }

    # Interface/entry point function and front end for "Configure"
    # This code is awfully pedestrian because it would need
    # more coupling and support between the way we store the
    # configuration values in safe::interp's and the Opt package
    # Obviously we would like an OptConfigure
    # to avoid duplicating all this code everywhere. -> TODO
    # (the app should share or access easily the program/value
    #  stored by opt)
    # This is even more complicated by the boolean flags with no values
    # that we had the bad idea to support for the sake of user simplicity
    # in create/init but which makes life hard in configure...
    # So this will be hopefully written and some integrated with opt1.0
    # (hopefully for tcl8.1 ?)
    proc interpConfigure {args} {
	switch [llength $args] {
	    1 {
		# If we have exactly 1 argument
		# the semantic is to return all the current configuration
		# We still call OptKeyParse though we know that "slave"
		# is our given argument because it also checks
		# for the "-help" option.
		set Args [::tcl::OptKeyParse ::safe::interpIC $args];
		CheckInterp $slave;
		set res {}
		lappend res [list -accessPath [Set [PathListName $slave]]]
		lappend res [list -statics    [Set [StaticsOkName $slave]]]
		lappend res [list -nested     [Set [NestedOkName $slave]]]
		lappend res [list -deleteHook [Set [DeleteHookName $slave]]]
		join $res
	    }
	    2 {
		# If we have exactly 2 arguments
		# the semantic is a "configure get"
		::tcl::Lassign $args slave arg;
		# get the flag sub program (we 'know' about Opt's internal
		# representation of data)
		set desc [lindex [::tcl::OptKeyGetDesc ::safe::interpIC] 2]
		set hits [::tcl::OptHits desc $arg];
                if {$hits > 1} {
                    return -code error [::tcl::OptAmbigous $desc $arg]
                } elseif {$hits == 0} {
                    return -code error [::tcl::OptFlagUsage $desc $arg]
                }
		CheckInterp $slave;
		set item [::tcl::OptCurDesc $desc];
		set name [::tcl::OptName $item];
		switch -exact -- $name {
		    -accessPath {
			return [list -accessPath [Set [PathListName $slave]]]
		    }
		    -statics {
			return [list -statics    [Set [StaticsOkName $slave]]]
		    }
		    -nested {
			return [list -nested     [Set [NestedOkName $slave]]]
		    }
		    -deleteHook {
			return [list -deleteHook [Set [DeleteHookName $slave]]]
		    }
		    -noStatics {
			# it is most probably a set in fact
			# but we would need then to jump to the set part
			# and it is not *sure* that it is a set action
			# that the user want, so force it to use the
			# unambigous -statics ?value? instead:
			return -code error\
				"ambigous query (get or set -noStatics ?)\
				use -statics instead";
		    }
		    -nestedLoadOk {
			return -code error\
				"ambigous query (get or set -nestedLoadOk ?)\
				use -nested instead";
		    }
		    default {
			return -code error "unknown flag $name (bug)";
		    }
		}
	    }
	    default {
		# Otherwise we want to parse the arguments like init and create
		# did
		set Args [::tcl::OptKeyParse ::safe::interpIC $args];
		CheckInterp $slave;
		# Get the current (and not the default) values of
		# whatever has not been given:
		if {![::tcl::OptProcArgGiven -accessPath]} {
		    set doreset 1
		    set accessPath [Set [PathListName $slave]]
		} else {
		    set doreset 0
		}
		if {    (![::tcl::OptProcArgGiven -statics]) 
                     && (![::tcl::OptProcArgGiven -noStatics]) } {
		    set statics    [Set [StaticsOkName $slave]]
		} else {
		    set statics    [InterpStatics]
		}
		if {    ([::tcl::OptProcArgGiven -nested]) 
                     || ([::tcl::OptProcArgGiven -nestedLoadOk]) } {
		    set nested     [InterpNested]
		} else {
		    set nested     [Set [NestedOkName $slave]]
		}
		if {![::tcl::OptProcArgGiven -deleteHook]} {
		    set deleteHook [Set [DeleteHookName $slave]]
		}
		# we can now reconfigure :
		InterpSetConfig $slave $accessPath \
			$statics $nested $deleteHook;
		# auto_reset the slave (to completly synch the new access_path)
		if {$doreset} {
		    if {[catch {::interp eval $slave {auto_reset}} msg]} {
			Log $slave "auto_reset failed: $msg";
		    } else {
			Log $slave "successful auto_reset" NOTICE;
		    }
		}
	    }
	}
    }


    ####
    #
    #  Functions that actually implements the exported APIs
    #
    ####


    #
    # safe::InterpCreate : doing the real job
    #
    # This procedure creates a safe slave and initializes it with the
    # safe base aliases.
    # NB: slave name must be simple alphanumeric string, no spaces,
    # no (), no {},...  {because the state array is stored as part of the name}
    #
    # Returns the slave name.
    #
    # Optional Arguments : 
    # + slave name : if empty, generated name will be used
    # + access_path: path list controlling where load/source can occur,
    #                if empty: the master auto_path will be used.
    # + staticsok  : flag, if 0 :no static package can be loaded (load {} Xxx)
    #                      if 1 :static packages are ok.
    # + nestedok: flag, if 0 :no loading to sub-sub interps (load xx xx sub)
    #                      if 1 : multiple levels are ok.
    
    # use the full name and no indent so auto_mkIndex can find us
    proc ::safe::InterpCreate {
	slave 
	access_path
	staticsok
	nestedok
	deletehook
    } {
	# Create the slave.
	if {[string compare "" $slave]} {
	    ::interp create -safe $slave;
	} else {
	    # empty argument: generate slave name
	    set slave [::interp create -safe];
	}
	Log $slave "Created" NOTICE;

	# Initialize it. (returns slave name)
	InterpInit $slave $access_path $staticsok $nestedok $deletehook;
    }


    #
    # InterpSetConfig (was setAccessPath) :
    #    Sets up slave virtual auto_path and corresponding structure
    #    within the master. Also sets the tcl_library in the slave
    #    to be the first directory in the path.
    #    Nb: If you change the path after the slave has been initialized
    #    you probably need to call "auto_reset" in the slave in order that it
    #    gets the right auto_index() array values.

    proc ::safe::InterpSetConfig {slave access_path staticsok\
	    nestedok deletehook} {

	# determine and store the access path if empty
	if {[string match "" $access_path]} {
	    set access_path [uplevel #0 set auto_path];
	    # Make sure that tcl_library is in auto_path
	    # and at the first position (needed by setAccessPath)
	    set where [lsearch -exact $access_path [info library]];
	    if {$where == -1} {
		# not found, add it.
		set access_path [concat [list [info library]] $access_path];
		Log $slave "tcl_library was not in auto_path,\
			added it to slave's access_path" NOTICE;
	    } elseif {$where != 0} {
		# not first, move it first
		set access_path [concat [list [info library]]\
			[lreplace $access_path $where $where]];
		Log $slave "tcl_libray was not in first in auto_path,\
			moved it to front of slave's access_path" NOTICE;
	    
	    }

	    # Add 1st level sub dirs (will searched by auto loading from tcl
	    # code in the slave using glob and thus fail, so we add them
	    # here so by default it works the same).
	    set access_path [AddSubDirs $access_path];
	}

	Log $slave "Setting accessPath=($access_path) staticsok=$staticsok\
		nestedok=$nestedok deletehook=($deletehook)" NOTICE;

	# clear old autopath if it existed
	set nname [PathNumberName $slave];
	if {[Exists $nname]} {
	    set n [Set $nname];
	    for {set i 0} {$i<$n} {incr i} {
		Unset [PathToken $i $slave];
	    }
	}

	# build new one
	set slave_auto_path {}
	set i 0;
	foreach dir $access_path {
	    Set [PathToken $i $slave] $dir;
	    lappend slave_auto_path "\$[PathToken $i]";
	    incr i;
	}
	Set $nname $i;
	Set [PathListName $slave] $access_path;
	Set [VirtualPathListName $slave] $slave_auto_path;

	Set [StaticsOkName $slave] $staticsok
	Set [NestedOkName $slave] $nestedok
	Set [DeleteHookName $slave] $deletehook

	SyncAccessPath $slave;
    }

    #
    #
    # FindInAccessPath:
    #    Search for a real directory and returns its virtual Id
    #    (including the "$")
proc ::safe::interpFindInAccessPath {slave path} {
	set access_path [GetAccessPath $slave];
	set where [lsearch -exact $access_path $path];
	if {$where == -1} {
	    return -code error "$path not found in access path $access_path";
	}
	return "\$[PathToken $where]";
    }

    #
    # addToAccessPath:
    #    add (if needed) a real directory to access path
    #    and return its virtual token (including the "$").
proc ::safe::interpAddToAccessPath {slave path} {
	# first check if the directory is already in there
	if {![catch {interpFindInAccessPath $slave $path} res]} {
	    return $res;
	}
	# new one, add it:
	set nname [PathNumberName $slave];
	set n [Set $nname];
	Set [PathToken $n $slave] $path;

	set token "\$[PathToken $n]";

	Lappend [VirtualPathListName $slave] $token;
	Lappend [PathListName $slave] $path;
	Set $nname [expr $n+1];

	SyncAccessPath $slave;

	return $token;
    }

    # This procedure applies the initializations to an already existing
    # interpreter. It is useful when you want to install the safe base
    # aliases into a preexisting safe interpreter.
    proc ::safe::InterpInit {
	slave 
	access_path
	staticsok
	nestedok
	deletehook
    } {

	# Configure will generate an access_path when access_path is
	# empty.
	InterpSetConfig $slave $access_path $staticsok $nestedok $deletehook;

	# These aliases let the slave load files to define new commands

	# NB we need to add [namespace current], aliases are always
	# absolute paths.
	::interp alias $slave source {} [namespace current]::AliasSource $slave
	::interp alias $slave load {} [namespace current]::AliasLoad $slave

	# This alias lets the slave have access to a subset of the 'file'
	# command functionality.

	AliasSubset $slave file file dir.* join root.* ext.* tail \
		path.* split

	# This alias interposes on the 'exit' command and cleanly terminates
	# the slave.

	::interp alias $slave exit {} [namespace current]::interpDelete $slave

	# The allowed slave variables already have been set
	# by Tcl_MakeSafe(3)


	# Source init.tcl into the slave, to get auto_load and other
	# procedures defined:

	# We don't try to use the -rsrc on the mac because it would get
	# confusing if you would want to customize init.tcl
	# for a given set of safe slaves, on all the platforms
	# you just need to give a specific access_path and
	# the mac should be no exception. As there is no
	# obvious full "safe ressources" design nor implementation
	# for the mac, safe interps there will just don't
	# have that ability. (A specific app can still reenable
	# that using custom aliases if they want to).
	# It would also make the security analysis and the Safe Tcl security
	# model platform dependant and thus more error prone.

	if {[catch {::interp eval $slave\
		{source [file join $tcl_library init.tcl]}}\
		msg]} {
	    Log $slave "can't source init.tcl ($msg)";
	    error "can't source init.tcl into slave $slave ($msg)"
	}

	return $slave
    }


    # Add (only if needed, avoid duplicates) 1 level of
    # sub directories to an existing path list.
    # Also removes non directories from the returned list.
    proc AddSubDirs {pathList} {
	set res {}
	foreach dir $pathList {
	    if {[file isdirectory $dir]} {
		# check that we don't have it yet as a children
		# of a previous dir
		if {[lsearch -exact $res $dir]<0} {
		    lappend res $dir;
		}
		foreach sub [glob -nocomplain -- [file join $dir *]] {
		    if {    ([file isdirectory $sub])
		         && ([lsearch -exact $res $sub]<0) } {
			# new sub dir, add it !
	                lappend res $sub;
	            }
		}
	    }
	}
	return $res;
    }

    # This procedure deletes a safe slave managed by Safe Tcl and
    # cleans up associated state:

proc ::safe::interpDelete {slave} {

        Log $slave "About to delete" NOTICE;

	# If the slave has a cleanup hook registered, call it.
	# check the existance because we might be called to delete an interp
	# which has not been registered with us at all
	set hookname [DeleteHookName $slave];
	if {[Exists $hookname]} {
	    set hook [Set $hookname];
	    if {![::tcl::Lempty $hook]} {
		# remove the hook now, otherwise if the hook
		# calls us somehow, we'll loop
		Unset $hookname;
		if {[catch {eval $hook $slave} err]} {
		    Log $slave "Delete hook error ($err)";
		}
	    }
	}

	# Discard the global array of state associated with the slave, and
	# delete the interpreter.

	set statename [InterpStateName $slave];
	if {[Exists $statename]} {
	    Unset $statename;
	}

	# if we have been called twice, the interp might have been deleted
	# already
	if {[::interp exists $slave]} {
	    ::interp delete $slave;
	    Log $slave "Deleted" NOTICE;
	}

	return
    }

    # Set (or get) the loging mecanism 

proc ::safe::setLogCmd {args} {
    variable Log;
    if {[llength $args] == 0} {
	return $Log;
    } else {
	if {[llength $args] == 1} {
	    set Log [lindex $args 0];
	} else {
	    set Log $args
	}
    }
}

    # internal variable
    variable Log {}

    # ------------------- END OF PUBLIC METHODS ------------


    #
    # sets the slave auto_path to the master recorded value.
    # also sets tcl_library to the first token of the virtual path.
    #
    proc SyncAccessPath {slave} {
	set slave_auto_path [Set [VirtualPathListName $slave]];
	::interp eval $slave [list set auto_path $slave_auto_path];
	Log $slave \
		"auto_path in $slave has been set to $slave_auto_path"\
		NOTICE;
	::interp eval $slave [list set tcl_library [lindex $slave_auto_path 0]];
    }

    # base name for storing all the slave states
    # the array variable name for slave foo is thus "Sfoo"
    # and for sub slave {foo bar} "Sfoo bar" (spaces are handled
    # ok everywhere (or should))
    # We add the S prefix to avoid that a slave interp called "Log"
    # would smash our "Log" variable.
    proc InterpStateName {slave} {
	return "S$slave";
    }

    # Check that the given slave is "one of us"
    proc IsInterp {slave} {
	expr {    ([Exists [InterpStateName $slave]]) 
	       && ([::interp exists $slave])}
    }

    # returns the virtual token for directory number N
    # if the slave argument is given, 
    # it will return the corresponding master global variable name
    proc PathToken {n {slave ""}} {
	if {[string compare "" $slave]} {
	    return "[InterpStateName $slave](access_path,$n)";
	} else {
	    # We need to have a ":" in the token string so
	    # [file join] on the mac won't turn it into a relative
	    # path.
	    return "p(:$n:)";
	}
    }
    # returns the variable name of the complete path list
    proc PathListName {slave} {
	return "[InterpStateName $slave](access_path)";
    }
    # returns the variable name of the complete path list
    proc VirtualPathListName {slave} {
	return "[InterpStateName $slave](access_path_slave)";
    }
    # returns the variable name of the number of items
    proc PathNumberName {slave} {
	return "[InterpStateName $slave](access_path,n)";
    }
    # returns the staticsok flag var name
    proc StaticsOkName {slave} {
	return "[InterpStateName $slave](staticsok)";
    }
    # returns the nestedok flag var name
    proc NestedOkName {slave} {
	return "[InterpStateName $slave](nestedok)";
    }
    # Run some code at the namespace toplevel
    proc Toplevel {args} {
	namespace eval [namespace current] $args;
    }
    # set/get values
    proc Set {args} {
	eval Toplevel set $args;
    }
    # lappend on toplevel vars
    proc Lappend {args} {
	eval Toplevel lappend $args;
    }
    # unset a var/token (currently just an global level eval)
    proc Unset {args} {
	eval Toplevel unset $args;
    }
    # test existance 
    proc Exists {varname} {
	Toplevel info exists $varname;
    }
    # short cut for access path getting
    proc GetAccessPath {slave} {
	Set [PathListName $slave]
    }
    # short cut for statics ok flag getting
    proc StaticsOk {slave} {
	Set [StaticsOkName $slave]
    }
    # short cut for getting the multiples interps sub loading ok flag
    proc NestedOk {slave} {
	Set [NestedOkName $slave]
    }
    # interp deletion storing hook name
    proc DeleteHookName {slave} {
	return [InterpStateName $slave](cleanupHook)
    }

    #
    # translate virtual path into real path
    #
    proc TranslatePath {slave path} {
	# somehow strip the namespaces 'functionality' out (the danger
	# is that we would strip valid macintosh "../" queries... :
	if {[regexp {(::)|(\.\.)} $path]} {
	    error "invalid characters in path $path";
	}
	set n [expr [Set [PathNumberName $slave]]-1];
	for {} {$n>=0} {incr n -1} {
	    # fill the token virtual names with their real value
	    set [PathToken $n] [Set [PathToken $n $slave]];
	}
	# replaces the token by their value
	subst -nobackslashes -nocommands $path;
    }


    # Log eventually log an error
    # to enable error logging, set Log to {puts stderr} for instance
    proc Log {slave msg {type ERROR}} {
	variable Log;
	if {[info exists Log] && [llength $Log]} {
	    eval $Log [list "$type for slave $slave : $msg"];
	}
    }

    
    # file name control (limit access to files/ressources that should be
    # a valid tcl source file)
    proc CheckFileName {slave file} {
	# limit what can be sourced to .tcl
	# and forbid files with more than 1 dot and
	# longer than 14 chars
	set ftail [file tail $file];
	if {[string length $ftail]>14} {
	    error "$ftail: filename too long";
	}
	if {[regexp {\..*\.} $ftail]} {
	    error "$ftail: more than one dot is forbidden";
	}
	if {[string compare $ftail "tclIndex"] && \
		[string compare [string tolower [file extension $ftail]]\
		".tcl"]} {
	    error "$ftail: must be a *.tcl or tclIndex";
	}

	if {![file exists $file]} {
	    # don't tell the file path
	    error "no such file or directory";
	}

	if {![file readable $file]} {
	    # don't tell the file path
	    error "not readable";
	}

    }


    # AliasSource is the target of the "source" alias in safe interpreters.

    proc AliasSource {slave args} {

	set argc [llength $args];
	# Allow only "source filename"
	# (and not mac specific -rsrc for instance - see comment in ::init
	# for current rationale)
	if {$argc != 1} {
	    set msg "wrong # args: should be \"source fileName\""
	    Log $slave "$msg ($args)";
	    return -code error $msg;
	}
	set file [lindex $args 0]
	
	# get the real path from the virtual one.
	if {[catch {set file [TranslatePath $slave $file]} msg]} {
	    Log $slave $msg;
	    return -code error "permission denied"
	}
	
	# check that the path is in the access path of that slave
	if {[catch {FileInAccessPath $slave $file} msg]} {
	    Log $slave $msg;
	    return -code error "permission denied"
	}

	# do the checks on the filename :
	if {[catch {CheckFileName $slave $file} msg]} {
	    Log $slave "$file:$msg";
	    return -code error $msg;
	}

	# passed all the tests , lets source it:
	if {[catch {::interp invokehidden $slave source $file} msg]} {
	    Log $slave $msg;
	    return -code error "script error";
	}
	return $msg
    }

    # AliasLoad is the target of the "load" alias in safe interpreters.

    proc AliasLoad {slave file args} {

	set argc [llength $args];
	if {$argc > 2} {
	    set msg "load error: too many arguments";
	    Log $slave "$msg ($argc) {$file $args}";
	    return -code error $msg;
	}

	# package name (can be empty if file is not).
	set package [lindex $args 0];

	# Determine where to load. load use a relative interp path
	# and {} means self, so we can directly and safely use passed arg.
	set target [lindex $args 1];
	if {[string length $target]} {
	    # we will try to load into a sub sub interp
	    # check that we want to authorize that.
	    if {![NestedOk $slave]} {
		Log $slave "loading to a sub interp (nestedok)\
			disabled (trying to load $package to $target)";
		return -code error "permission denied (nested load)";
	    }
	    
	}

	# Determine what kind of load is requested
	if {[string length $file] == 0} {
	    # static package loading
	    if {[string length $package] == 0} {
		set msg "load error: empty filename and no package name";
		Log $slave $msg;
		return -code error $msg;
	    }
	    if {![StaticsOk $slave]} {
		Log $slave "static packages loading disabled\
			(trying to load $package to $target)";
		return -code error "permission denied (static package)";
	    }
	} else {
	    # file loading

	    # get the real path from the virtual one.
	    if {[catch {set file [TranslatePath $slave $file]} msg]} {
		Log $slave $msg;
		return -code error "permission denied"
	    }

	    # check the translated path
	    if {[catch {FileInAccessPath $slave $file} msg]} {
		Log $slave $msg;
		return -code error "permission denied (path)"
	    }
	}

	if {[catch {::interp invokehidden\
		$slave load $file $package $target} msg]} {
	    Log $slave $msg;
	    return -code error $msg
	}

	return $msg
    }

    # FileInAccessPath raises an error if the file is not found in
    # the list of directories contained in the (master side recorded) slave's
    # access path.

    # the security here relies on "file dirname" answering the proper
    # result.... needs checking ?
    proc FileInAccessPath {slave file} {

	set access_path [GetAccessPath $slave];

	if {[file isdirectory $file]} {
	    error "\"$file\": is a directory"
	}
	set parent [file dirname $file]
	if {[lsearch -exact $access_path $parent] == -1} {
	    error "\"$file\": not in access_path";
	}
    }

    # This procedure enables access from a safe interpreter to only a subset of
    # the subcommands of a command:

    proc Subset {slave command okpat args} {
	set subcommand [lindex $args 0]
	if {[regexp $okpat $subcommand]} {
	    return [eval {$command $subcommand} [lrange $args 1 end]]
	}
	set msg "not allowed to invoke subcommand $subcommand of $command";
	Log $slave $msg;
	error $msg;
    }

    # This procedure installs an alias in a slave that invokes "safesubset"
    # in the master to execute allowed subcommands. It precomputes the pattern
    # of allowed subcommands; you can use wildcards in the pattern if you wish
    # to allow subcommand abbreviation.
    #
    # Syntax is: AliasSubset slave alias target subcommand1 subcommand2...

    proc AliasSubset {slave alias target args} {
	set pat ^(; set sep ""
	foreach sub $args {
	    append pat $sep$sub
	    set sep |
	}
	append pat )\$
	::interp alias $slave $alias {}\
		[namespace current]::Subset $slave $target $pat
    }

}
