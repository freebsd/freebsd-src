# safeinit.tcl --
#
# This code runs in a master to manage a safe slave with Safe Tcl.
# See the safe.n man page for details.
#
# Copyright (c) 1996-1997 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# SCCS: @(#) safeinit.tcl 1.38 97/06/20 12:57:39

# This procedure creates a safe slave, initializes it with the
# safe base and installs the aliases for the security policy mechanism.

proc tcl_safeCreateInterp {slave} {
    global auto_path

    # Create the slave.
    interp create -safe $slave

    # Set its auto_path
    interp eval $slave [list set auto_path $auto_path]

    # And initialize it.
    return [tcl_safeInitInterp $slave]
}

# This procedure applies the initializations to an already existing
# interpreter. It is useful when you want to enable an interpreter
# created with "interp create -safe" to use security policies.

proc tcl_safeInitInterp {slave} {
    upvar #0 tclSafe$slave state
    global tcl_library tk_library auto_path tcl_platform

    # These aliases let the slave load files to define new commands

    interp alias $slave source {} tclSafeAliasSource $slave
    interp alias $slave load {} tclSafeAliasLoad $slave

    # This alias lets the slave have access to a subset of the 'file'
    # command functionality.
    tclAliasSubset $slave file file dir.* join root.* ext.* tail \
	path.* split

    # This alias interposes on the 'exit' command and cleanly terminates
    # the slave.
    interp alias $slave exit {} tcl_safeDeleteInterp $slave

    # Source init.tcl into the slave, to get auto_load and other
    # procedures defined:

    if {$tcl_platform(platform) == "macintosh"} {
	if {[catch {interp eval $slave [list source -rsrc Init]}]} {
	    if {[catch {interp eval $slave \
			[list source [file join $tcl_library init.tcl]]}]} {
		error "can't source init.tcl into slave $slave"
	    }
	}
    } else {
	if {[catch {interp eval $slave \
			[list source [file join $tcl_library init.tcl]]}]} {
	    error "can't source init.tcl into slave $slave"
	}
    }

    # Loading packages into slaves is handled by their master.
    # This is overloaded to deal with regular packages and security policies

    interp alias $slave tclPkgUnknown {} tclSafeAliasPkgUnknown $slave
    interp eval $slave {package unknown tclPkgUnknown}

    # We need a helper procedure to define a $dir variable and then
    # do a source of the pkgIndex.tcl file
    interp eval $slave \
	[list proc tclPkgSource {dir args} {
		if {[llength $args] == 2} {
		    source [lindex $args 0] [lindex $args 1]
		} else {
		    source [lindex $args 0]
		}
	      }]

    # Let the slave inherit a few variables
    foreach varName \
	{tcl_library tcl_version tcl_patchLevel \
	 tcl_platform(platform) auto_path} {
	upvar #0 $varName var
	interp eval $slave [list set $varName $var]
    }

    # Other variables are predefined with set values
    foreach {varName value} {
	    auto_noexec 1
	    errorCode {}
	    errorInfo {}
	    env() {}
	    argv0 {}
	    argv {}
	    argc 0
	    tcl_interactive 0
	    } {
	interp eval $slave [list set $varName $value]
    }

    # If auto_path is not set in the slave, set it to empty so it has
    # a value and exists. Otherwise auto_loading and package require
    # will complain.

    interp eval $slave {
	if {![info exists auto_path]} {
	    set auto_path {}
	}
    }

    # If we have Tk, make the slave have the same library as us:

    if {[info exists tk_library]} {
        interp eval $slave [list set tk_library $tk_library]
    }

    # Stub out auto-exec mechanism in slave
    interp eval $slave [list proc auto_execok {name} {return {}}]

    return $slave
}

# This procedure deletes a safe slave managed by Safe Tcl and
# cleans up associated state:

proc tcl_safeDeleteInterp {slave args} {
    upvar #0 tclSafe$slave state

    # If the slave has a policy loaded, clean it up now.
    if {[info exists state(policyLoaded)]} {
	set policy $state(policyLoaded)
	set proc ${policy}_PolicyCleanup
	if {[string compare [info proc $proc] $proc] == 0} {
	    $proc $slave
	}
    }

    # Discard the global array of state associated with the slave, and
    # delete the interpreter.
    catch {unset state}
    catch {interp delete $slave}

    return
}

# This procedure computes the global security policy search path.

proc tclSafeComputePolicyPath {} {
    global auto_path tclSafeAutoPathComputed tclSafePolicyPath

    set recompute 0
    if {(![info exists tclSafePolicyPath]) ||
	    ("$tclSafePolicyPath" == "")} {
	set tclSafePolicyPath ""
	set tclSafeAutoPathComputed ""
	set recompute 1
    }
    if {"$tclSafeAutoPathComputed" != "$auto_path"} {
	set recompute 1
	set tclSafeAutoPathComputed $auto_path
    }
    if {$recompute == 1} {
	set tclSafePolicyPath ""
	foreach i $auto_path {
	    lappend tclSafePolicyPath [file join $i policies]
	}
    }
    return $tclSafePolicyPath
}

# ---------------------------------------------------------------------------
# ---------------------------------------------------------------------------

# tclSafeAliasSource is the target of the "source" alias in safe interpreters.

proc tclSafeAliasSource {slave args} {
    global auto_path errorCode errorInfo

    if {[llength $args] == 2} {
	if {[string compare "-rsrc" [lindex $args 0]] != 0} {
	    return -code error "incorrect arguments to source"
	}
	if {[catch {interp invokehidden $slave source -rsrc [lindex $args 1]} \
		 msg]} {
	    return -code error $msg
	}
    } else {
	set file [lindex $args 0]
	if {[catch {tclFileInPath $file $auto_path $slave} msg]} {
	    return -code error "permission denied"
	}
	set errorInfo ""
	if {[catch {interp invokehidden $slave source $file} msg]} {
	    return -code error $msg
	}
    }
    return $msg
}

# tclSafeAliasLoad is the target of the "load" alias in safe interpreters.

proc tclSafeAliasLoad {slave file args} {
    global auto_path

    if {[llength $args] == 2} {
	# Trying to load into another interpreter
	# Allow this for a child of the slave, or itself
	set other [lindex $args 1]
	foreach x $slave y $other {
	    if {[string length $x] == 0} {
		break
	    } elseif {[string compare $x $y] != 0} {
		return -code error "permission denied"
	    }
	}
	set slave $other
    }

    if {[string length $file] && \
		[catch {tclFileInPath $file $auto_path $slave} msg]} {
	return -code error "permission denied"
    }
    if {[catch {
	switch [llength $args] {
	    0 {
		interp invokehidden $slave load $file
	    }
	    1 -
	    2 {
		interp invokehidden $slave load $file [lindex $args 0]
	    }
	    default {
		error "too many arguments to load"
	    }
	}
    } msg]} {
	return -code error $msg
    }
    return $msg
}

# tclFileInPath raises an error if the file is not found in
# the list of directories contained in path.

proc tclFileInPath {file path slave} {
    set realcheckpath [tclSafeCheckAutoPath $path $slave]
    set pwd [pwd]
    if {[file isdirectory $file]} {
	error "$file: not found"
    }
    set parent [file dirname $file]
    if {[catch {cd $parent} msg]} {
	error "$file: not found"
    }
    set realfilepath [file split [pwd]]
    foreach dir $realcheckpath {
	set match 1
	foreach a [file split $dir] b $realfilepath {
	    if {[string length $a] == 0} {
		break
	    } elseif {[string compare $a $b] != 0} {
		set match 0
		break
	    }
	}
	if {$match} {
	    cd $pwd
	    return 1
	}
    }
    cd $pwd
    error "$file: not found"
}

# This procedure computes our expanded copy of the path, as needed.
# It returns the path after expanding out all aliases.

proc tclSafeCheckAutoPath {path slave} {
    global auto_path
    upvar #0 tclSafe$slave state

    if {![info exists state(expanded_auto_path)]} {
	# Compute for the first time:
	set state(cached_auto_path) $path
    } elseif {"$state(cached_auto_path)" != "$path"} {
	# The value of our path changed, so recompute:
	set state(cached_auto_path) $path
    } else {
	# No change: no need to recompute.
	return $state(expanded_auto_path)
    }

    set pwd [pwd]
    set state(expanded_auto_path) ""
    foreach dir $state(cached_auto_path) {
	if {![catch {cd $dir}]} {
	    lappend state(expanded_auto_path) [pwd]
	}
    }
    cd $pwd
    return $state(expanded_auto_path)
}

proc tclSafeAliasPkgUnknown {slave package version {exact {}}} {
    tclSafeLoadPkg $slave $package $version $exact
}

proc tclSafeLoadPkg {slave package version exact} {
    if {[string length $version] == 0} {
	set version 1.0
    }
    tclSafeLoadPkgInternal $slave $package $version $exact 0
}

proc tclSafeLoadPkgInternal {slave package version exact round} {
    global auto_path
    upvar #0 tclSafe$slave state

    # Search the policy path again; it might have changed in the meantime.

    if {$round == 1} {
	tclSafeResearchPolicyPath

	if {[tclSafeLoadPolicy $slave $package $version]} {
	    return
	}
    }

    # Try to load as a policy.

    if [tclSafeLoadPolicy $slave $package $version] {
	return
    }

    # The package is not a security policy, so do the regular setup.

    # Here we run tclPkgUnknown in the master, but we hijack
    # the source command so the setup ends up happening in the slave.

    rename source source.orig
    proc source {args} "upvar dir dir
	interp eval [list $slave] tclPkgSource \[list \$dir\] \$args"

    if [catch {tclPkgUnknown $package $version $exact} err] {
	global errorInfo

	rename source {}
	rename source.orig source

	error "$err\n$errorInfo"
    }
    rename source {}
    rename source.orig source

    # If we are in the first round, check if the package
    # is now known in the slave:

    if {$round == 0} {
        set ifneeded \
		[interp eval $slave [list package ifneeded $package $version]]

	if {"$ifneeded" == ""} {
	    return [tclSafeLoadPkgInternal $slave $package $version $exact 1]
	}
    }
}

proc tclSafeResearchPolicyPath {} {
    global tclSafePolicyPath auto_index auto_path

    # If there was no change, do not search again.

    if {![info exists tclSafePolicyPath]} {
	set tclSafePolicyPath ""
    }
    set oldPolicyPath $tclSafePolicyPath
    set newPolicyPath [tclSafeComputePolicyPath]
    if {"$newPolicyPath" == "$oldPolicyPath"} {
	return
    }

    # Loop through the path from back to front so early directories
    # end up overriding later directories.  This code is like auto_load,
    # but only new-style tclIndex files (version 2) are supported.

    for {set i [expr [llength $newPolicyPath] - 1]} \
	    {$i >= 0} \
	    {incr i -1} {
	set dir [lindex $newPolicyPath $i]
        set file [file join $dir tclIndex]
	if {[file exists $file]} {
	    if {[catch {source $file} msg]} {
		puts stderr "error sourcing $file: $msg"
	    }
	}
	foreach file [lsort [glob -nocomplain [file join $dir *]]] {
	    if {[file isdir $file]} {
		set dir $file
		set file [file join $file tclIndex]
		if {[file exists $file]} {
		    if {[catch {source $file} msg]} {
			puts stderr "error sourcing $file: $msg"
		    }
		}
	    }
	}
    }
}

proc tclSafeLoadPolicy {slave package version} {
    upvar #0 tclSafe$slave state
    global auto_index

    set proc ${package}_PolicyInit

    if {[info command $proc] == "$proc" ||
	    [info exists auto_index($proc)]} {
	if [info exists state(policyLoaded)] {
	    error "security policy $state(policyLoaded) already loaded"
	}	
	$proc $slave $version
	interp eval $slave [list package provide $package $version]
	set state(policyLoaded) $package
	return 1
    } else {
	return 0
    }
}
# This procedure enables access from a safe interpreter to only a subset of
# the subcommands of a command:

proc tclSafeSubset {command okpat args} {
    set subcommand [lindex $args 0]
    if {[regexp $okpat $subcommand]} {
	return [eval {$command $subcommand} [lrange $args 1 end]]
    }
    error "not allowed to invoke subcommand $subcommand of $command"
}

# This procedure installs an alias in a slave that invokes "safesubset"
# in the master to execute allowed subcommands. It precomputes the pattern
# of allowed subcommands; you can use wildcards in the pattern if you wish
# to allow subcommand abbreviation.
#
# Syntax is: tclAliasSubset slave alias target subcommand1 subcommand2...

proc tclAliasSubset {slave alias target args} {
    set pat ^(; set sep ""
    foreach sub $args {
	append pat $sep$sub
	set sep |
    }
    append pat )\$
    interp alias $slave $alias {} tclSafeSubset $target $pat
}
