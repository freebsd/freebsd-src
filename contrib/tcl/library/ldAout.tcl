# ldAout.tcl --
#
#	This "tclldAout" procedure in this script acts as a replacement
#	for the "ld" command when linking an object file that will be
#	loaded dynamically into Tcl or Tk using pseudo-static linking.
#
# Parameters:
#	The arguments to the script are the command line options for
#	an "ld" command.
#
# Results:
#	The "ld" command is parsed, and the "-o" option determines the
#	module name.  ".a" and ".o" options are accumulated.
#	The input archives and object files are examined with the "nm"
#	command to determine whether the modules initialization
#	entry and safe initialization entry are present.  A trivial
#	C function that locates the entries is composed, compiled, and
#	its .o file placed before all others in the command; then
#	"ld" is executed to bind the objects together.
#
# SCCS: @(#) ldAout.tcl 1.10 96/05/18 16:40:42
#
# Copyright (c) 1995, by General Electric Company. All rights reserved.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# This work was supported in part by the ARPA Manufacturing Automation
# and Design Engineering (MADE) Initiative through ARPA contract
# F33615-94-C-4400.

proc tclLdAout {{cc {}} {shlib_suffix {}} {shlib_cflags none}} {
  global env
  global argv

  if {$cc==""} {
    set cc $env(CC)
  }

  # if only two parameters are supplied there is assumed that the
  # only shlib_suffix is missing. This parameter is anyway available
  # as "info sharedlibextension" too, so there is no need to transfer
  # 3 parameters to the function tclLdAout. For compatibility, this
  # function now accepts both 2 and 3 parameters.

  if {$shlib_suffix==""} {
    set shlib_suffix $env(SHLIB_SUFFIX)
    set shlib_cflags $env(SHLIB_CFLAGS)
  } else {
    if {$shlib_cflags=="none"} {
      set shlib_cflags $shlib_suffix
      set shlib_suffix [info sharedlibextension]
    }
  }

  # seenDotO is nonzero if a .o or .a file has been seen

  set seenDotO 0

  # minusO is nonzero if the last command line argument was "-o".

  set minusO 0

  # head has command line arguments up to but not including the first
  # .o or .a file. tail has the rest of the arguments.

  set head {}
  set tail {}

  # nmCommand is the "nm" command that lists global symbols from the
  # object files.

  set nmCommand {|nm -g}

  # entryProtos is the table of _Init and _SafeInit prototypes found in the
  # module.

  set entryProtos {}

  # entryPoints is the table of _Init and _SafeInit entries found in the
  # module.

  set entryPoints {}

  # libraries is the list of -L and -l flags to the linker.

  set libraries {}
  set libdirs {}

  # Process command line arguments

  foreach a $argv {
    if {!$minusO && [regexp {\.[ao]$} $a]} {
      set seenDotO 1
      lappend nmCommand $a
    }
    if {$minusO} {
      set outputFile $a
      set minusO 0
    } elseif {![string compare $a -o]} {
      set minusO 1
    }
    if [regexp {^-[lL]} $a] {
	lappend libraries $a
	if [regexp {^-L} $a] {
	    lappend libdirs [string range $a 2 end]
	}
    } elseif {$seenDotO} {
	lappend tail $a
    } else {
	lappend head $a
    }
  }
  lappend libdirs /lib /usr/lib
  lappend libraries -lm -lc

  # MIPS -- If there are corresponding G0 libraries, replace the
  # ordinary ones with the G0 ones.

  set libs {}
  foreach lib $libraries {
      if [regexp {^-l} $lib] {
	  set lname [string range $lib 2 end]
	  foreach dir $libdirs {
	      if [file exists [file join $dir lib${lname}_G0.a]] {
		  set lname ${lname}_G0
		  break
	      }
	  }
	  lappend libs -l$lname
      } else {
	  lappend libs $lib
      }
  }
  set libraries $libs

  # Extract the module name from the "-o" option

  if {![info exists outputFile]} {
    error "-o option must be supplied to link a Tcl load module"
  }
  set m [file tail $outputFile]
  set l [expr [string length $m] - [string length $shlib_suffix]]
  if [string compare [string range $m $l end] $shlib_suffix] {
    error "Output file does not appear to have a $shlib_suffix suffix"
  }
  set modName [string tolower [string range $m 0 [expr $l-1]]]
  if [regexp {^lib} $modName] {
    set modName [string range $modName 3 end]
  }
  if [regexp {[0-9\.]*(_g0)?$} $modName match] {
    set modName [string range $modName 0 [expr [string length $modName]-[string length $match]-1]]
  }
  set modName "[string toupper [string index $modName 0]][string range $modName 1 end]"
  
  # Catalog initialization entry points found in the module

  set f [open $nmCommand r]
  while {[gets $f l] >= 0} {
    if [regexp {T[ 	]*_?([A-Z][a-z0-9_]*_(Safe)?Init(__FP10Tcl_Interp)?)$} $l trash symbol] {
      if {![regexp {_?([A-Z][a-z0-9_]*_(Safe)?Init)} $symbol trash s]} {
	set s $symbol
      }
      append entryProtos {extern int } $symbol { (); } \n
      append entryPoints {  } \{ { "} $s {", } $symbol { } \} , \n
    }
  }
  close $f

  if {$entryPoints==""} {
    error "No entry point found in objects"
  }

  # Compose a C function that resolves the initialization entry points and
  # embeds the required libraries in the object code.

  set C {#include <string.h>}
  append C \n
  append C {char TclLoadLibraries_} $modName { [] =} \n
  append C {  "@LIBS: } $libraries {";} \n
  append C $entryProtos
  append C {static struct } \{ \n
  append C {  char * name;} \n
  append C {  int (*value)();} \n
  append C \} {dictionary [] = } \{ \n
  append C $entryPoints
  append C {  0, 0 } \n \} \; \n
  append C {typedef struct Tcl_Interp Tcl_Interp;} \n
  append C {typedef int Tcl_PackageInitProc (Tcl_Interp *);} \n
  append C {Tcl_PackageInitProc *} \n
  append C TclLoadDictionary_ $modName { (symbol)} \n
  append C {    char * symbol;} \n
  append C {{
    int i;
    for (i = 0; dictionary [i] . name != 0; ++i) {
      if (!strcmp (symbol, dictionary [i] . name)) {
	return dictionary [i].value;
      }
    }
    return 0;
}} \n

  # Write the C module and compile it

  set cFile tcl$modName.c
  set f [open $cFile w]
  puts -nonewline $f $C
  close $f
  set ccCommand "$cc -c $shlib_cflags $cFile"
  puts stderr $ccCommand
  eval exec $ccCommand

  # Now compose and execute the ld command that packages the module

  set ldCommand ld
  foreach item $head {
    lappend ldCommand $item
  }
  lappend ldCommand tcl$modName.o
  foreach item $tail {
    lappend ldCommand $item
  }
  puts stderr $ldCommand
  eval exec $ldCommand

  # Clean up working files

  exec /bin/rm $cFile [file rootname $cFile].o
}
