#!/usr/local/bin/tclsh
#
# This Tcl script tries to build a GCC-source in a BSD-structured tree from 
# the GNU-structured one.  This code isn't for people with faint hearts.
#
#    "Med dens egne Vaaben skal jeg tugte Hoben,
#    som med Skrig og Raaben bryder Byens Fred.
#    Se engang til Taaben,
#    som med Munden aaben glor paa mig med Maaben."
#
# Fra Carl Nielsens "Maskerade".
#
# To change something in gcc for FreeBSD use this plan:
#	1. modify the GNU-gcc distribution as needed.
#	2. modify this script as needed.
#	3. run this script
#	4. try the result, goto 1 until you got it right.
#	5. send a patch to the GNU people !
# 
# Written Jan/Feb 1994 by Poul-Henning Kamp <phk@login.dkuug.dk>
# For FreeBSD 1.0 and 1.1 versus GCC 2.5.8
# Also seems to work for GCC.2.4.5 
#
# In case GNU changes the setup of their tree (again), this code will loose
# badly.  I have parameterized as much as possible, but a lot of Makefiles
# get made inline in the code.  Good luck.
#############################################################################
# Tweakable things, set for GCC 2.5.8, below they can be enabled for 2.4.5

# TWEAK:  Which GCC version ?
set ver 2.5.8

# TWEAK:  Where is the GNU-tree ?
set gnu /phk/tmp/gcc-2.5.8

# TWEAK:  Where is the BSD-tree ?
set bsd .

# TWEAK:  Whats the names in the GNU Makefile of the programs.
set pgm {cc1 cc1plus cc1obj cccp xgcc g++}

# TWEAK:  Where should the programs be installed
set pgm_install(cc1)	/usr/libexec/cc1
set pgm_install(cc1plus) /usr/libexec/cc1plus
set pgm_install(cc1obj)	/usr/libexec/cc1obj
set pgm_install(cccp)	/usr/libexec/cpp
set pgm_install(xgcc)	/usr/bin/gcc
set pgm_install(g++)	/usr/bin/g++

# TWEAK:  What links to make to each program
set pgm_link(xgcc)	/usr/bin/cc
set pgm_link(g++)	/usr/bin/c++
set pgm_link(cccp)	{ /usr/libexec/gccp }

# TWEAK:  What is the man.1 file named in the GNU-dist
set pgm_man1(xgcc)	gcc.1
set pgm_man1(g++)	g++.1
set pgm_man1(cccp)	cccp.1

# TWEAK:  What names should the man-page be installed under 
set pgm_dst1(xgcc)	{gcc.1 cc.1}
set pgm_dst1(g++)	{g++.1 c++.1}
set pgm_dst1(cccp)	{cpp.1}

# TWEAK:  What files must be explicitly installed ?
set config {i386/gstabs.h i386/perform.h i386/gas.h i386/i386.h i386/bsd.h
	i386/freebsd.h i386/unix.h cp-input.c bc-opcode.h bc-arity.h
	config.status md insn-attr.h insn-flags.h insn-codes.h c-parse.h}


############################################################################
# For gcc-2.4.5 we simply overwrite some of the tweaks...

if {$ver == "2.4.5"} {
    set gnu /usr/tmp/gcc-2.4.5
    set bsd cc245
    set pgm {cc1 cc1plus cc1obj cccp xgcc}
    set config {i386/gstabs.h i386/perform.h i386/gas.h i386/i386.h i386/bsd.h
	i386/386bsd.h i386/unix.h cp-input.c config.status md insn-attr.h
	insn-flags.h insn-codes.h c-parse.h}
}

############################################################################

# Try to make the GNU-Makefile produce a file for us.
proc makefile {name} {
    global gnu
    puts "------> Trying to make $name <-----"
    flush stdout
    catch {exec sh -c "cd $gnu ; make $name" >&@ stdout}
}

# Locate a source file, possibly making it on the fly.
proc find_source {name} {
    global gnu
    set n1 $name
    regsub {\.o$} $n1 {.c} n1
    if {[file exists $gnu/$n1]} {return $n1}
    makefile $n1
    if {[file exists $gnu/$n1]} {return $n1}
    makefile $name
    if {[file exists $gnu/$n1]} {return $n1}
    puts stderr "
find_source cannot locate or make a \"$n1\" for \"$name\"
"
    exit 1
}

# Define a macro in a makefile, to contain MANY files.
proc write_names {fd mac lst {suf ""}} {
	set lst [lsort $lst]	
	set a "$mac =\t"
	for {set i 0} {$i < [llength $lst]} {} {
	    set j [lindex $lst $i]$suf
	    if {[string length $a]+[string length $j]+1 < 68} {
		lappend a $j
		incr i
		continue;
	    }
	    puts $fd "$a \\"
	    set a "\t"
	}
	puts $fd "$a"
}

############################################################################
#
#    "Som en Samson skal jeg samle
#    Kraften i mit Styrkebaelte,
#    jeg skal gaa til deres Telte,
#    deres Stoetter skal jeg vaelte,
#    deres Tag skal sammen ramle,
#    om i Moerket skal de famle,
#    deres Knokkelrad skal skramle,
#    deres Rygmarv skal jeg smaelte,
#    deres Hjerne skal jeg aelte !"
#
# Fra Carl Nielsens "Maskerade".

puts "2bsd from $gnu to $bsd"

puts "Remove old directories if any, and create new ones."

# In case the BSD dir isn't there:
catch {exec mkdir $bsd}

# Throw the BSD/gcc_int dir away, and make a fresh.
catch {exec sh -c "rm -rf $bsd/gcc_int ; mkdir $bsd/gcc_int $bsd/gcc_int/i386"}

# Throw the BSD/libgcc dir away, and make a fresh.
catch {exec sh -c "rm -rf $bsd/libgcc ; mkdir $bsd/libgcc"}

# Throw the BSD/libobjc dir away, and make a fresh.
catch {exec sh -c "rm -rf $bsd/libobjc ; mkdir $bsd/libobjc"}

# Throw the BSD/$pgm dirs away too, and make some fresh.
foreach i $pgm {catch {exec sh -c "rm -rf $bsd/$i ; mkdir $bsd/$i"}}

puts "Check that $gnu directory is configured."

# configure makes this file, which we can use to check
if {![file exists $gnu/config.status]} {
    puts stderr "
Please go to the $gnu directory and execute these two commands:
	make distclean
	configure
Then restart this program.
"
    exit 1
}

if 1 {
# XXX
puts "Create $bsd/libgcc"

# Make a temporary Makefile in the GNU-tree
exec cp $gnu/Makefile $gnu/2bsd.t02

# Add targets to tell us what we want to know
set fi [open $gnu/2bsd.t02 a]
puts $fi "\nFo1:\n\t@echo \$(LIB1FUNCS)"
puts $fi "\nFo2:\n\t@echo \$(LIB2FUNCS)"
close $fi

# Try it out...
set t01 [exec sh -c "( cd $gnu ; make -f 2bsd.t02 Fo1)"]
set t02 [exec sh -c "( cd $gnu ; make -f 2bsd.t02 Fo2)"]

# Remove it again
exec rm -f $gnu/2bsd.t02

# Copy the sources
exec cp $gnu/libgcc1.c $gnu/libgcc2.c $bsd/libgcc

# Create the Makefile
set fo [open $bsd/libgcc/Makefile w]
puts $fo {
LIB =		gcc
NOPROFILE = 	1
SHLIB_MAJOR= 1
SHLIB_MINOR= 0

CFLAGS +=       -I$(.CURDIR)/../gcc_int -DFREEBSD_NATIVE
}

write_names $fo LIB1OBJS $t01 .o
write_names $fo LIB2OBJS $t02 .o
puts $fo {
OBJS= ${LIB1OBJS} ${LIB2OBJS}
LIB1SOBJS=${LIB1OBJS:.o=.so}
LIB2SOBJS=${LIB2OBJS:.o=.so}

${LIB1OBJS}: libgcc1.c
	${CC} -c ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${.CURDIR}/libgcc1.c
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

${LIB2OBJS}: libgcc2.c
	${CC} -c ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${.CURDIR}/libgcc2.c
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

.if !defined(NOPIC)
${LIB1SOBJS}: libgcc1.c
	${CC} -c -fpic ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${.CURDIR}/libgcc1.c

${LIB2SOBJS}: libgcc2.c
	${CC} -c -fpic ${CFLAGS} -DL${.PREFIX} -o ${.TARGET} ${.CURDIR}/libgcc2.c
.endif

.include <bsd.lib.mk>
}

close $fo

# Try to find out which .o files goes into which programs.
foreach  i $pgm {

    puts "Disecting $i."

    # Try to snatch the link-stmt from the GNU-Makefile
    set t01 [exec grep "  *-o  *$i  *" $gnu/Makefile]

    # Make a temporary Makefile in the GNU-tree
    exec cp $gnu/Makefile $gnu/2bsd.t02

    # Add a target to tell us what we want to know
    set fi [open $gnu/2bsd.t02 a]
    puts $fi "\nFoO:\n\t@echo $t01"
    close $fi

    # Try it out...
    set t02 [exec sh -c "( cd $gnu ; make -f 2bsd.t02 FoO)"]

    # Remove it again
    exec rm -f $gnu/2bsd.t02

    # Extract the .o files from it.
    set o ""
    foreach j $t02 {
	if {[regexp {\.o$} $j]} {
	    lappend o $j
	    # count the number of times each is used.
	    if {[catch {incr t04($j)} x]} {set t04($j) 1}
	}
    # Save that goes into this program
    set t05($i) $o
    }
}

# get a sorted list of all known objects
set l [lsort [array names t04]]

puts "Copy source files to $bsd/gcc_int"

# What goes into the libgcc_int ?
set lib ""
foreach i $l {
    # Used more than once, and we take it.
    if {$t04($i) > 1} { 
	set s [find_source $i]
	lappend lib $s
	# Copy the file
	exec cp $gnu/$s $bsd/gcc_int
    }
}

puts "Creating Makefile in $bsd/gcc_int"
set fo [open $bsd/gcc_int/Makefile w]

# Define SRCS to all the sourcefiles.
write_names $fo SRCS $lib

# Put the rest in there.
set lv [split $ver .]

# Shared-lib code-gen.  Will not work without much coding.
#puts $fo "SHLIB_MAJOR =	[format %d%02d [lindex $lv 0] [lindex $lv 1]]"
#puts $fo "SHLIB_MINOR =	[lindex $lv 2]"
# REQ's patch to /usr/share/mk/bsd.lib.mk XXX
#puts $fo "NOSTATIC =	1"

puts $fo {
LIB =		gcc_int
NOPROFILE =	1
CFLAGS +=	-I$(.CURDIR) -DFREEBSD_NATIVE

install:
	@true

.include	<bsd.lib.mk>

}
close $fo

# Now for each of the programs in turn
foreach i $pgm {
    puts "Copy source files to $bsd/$i"
    set o ""
    foreach j $t05($i) {
	if {$t04($j) == 1} { 
	    set s [find_source $j]
	    lappend o $s 
	    exec cp $gnu/$s $bsd/$i
        }
    }

    puts "Creating Makefile in $bsd/$i"

    set fo [open $bsd/$i/Makefile w]
    write_names $fo SRCS $o
    puts $fo "PROG =\t$i"
    puts $fo {
.if exists($(.CURDIR)/../gcc_int/obj)
DPADD +=         $(.CURDIR)/../gcc_int/obj/libgcc_int.a
LDADD +=         -L$(.CURDIR)/../gcc_int/obj -lgcc_int
.else
DPADD +=         $(.CURDIR)/../gcc_int/libgcc_int.a
LDADD +=         -L$(.CURDIR)/../gcc_int -lgcc_int
.endif

LDADD+=		-lgnumalloc -static
DPADD+=		${LIBGNUMALLOC}

CFLAGS +=       -I$(.CURDIR)/../gcc_int -DFREEBSD_NATIVE
}

    if {[catch {set pgm_install($i)} x]} {
	puts stderr "Needs to know where to install $i, pgm_install wont tell us."
	exit 1
    }
    puts $fo "\ninstall: $i\n\t\install \$(COPY) -o \${BINOWN} -g \${BINGRP} -m \${BINMODE} $i \$(DESTDIR)$pgm_install($i)"
    if {![catch {set pgm_link($i)} x]} {
	foreach j $x {
	    puts $fo "\t@rm -f \$(DESTDIR)$j"
	    puts $fo "\t@ln -s `basename $pgm_install($i)` \$(DESTDIR)$j"
	}
    }
    if {![catch {set pgm_man1($i)} x]} {
	exec cp $gnu/$x $bsd/$i/$i.1
	if {[catch {set pgm_dst1($i)} x]} {
	    puts stderr "Needs to know where to install manpage for $i, pgm_dst1 wont tell us."
	    exit 1
	}
	puts $fo "\t\install \$(COPY) -m \$(MANMODE) -o \$(MANOWN) -g \$(MANGRP) \$(.CURDIR)/$i.1 \$(DESTDIR)/usr/share/man/man1/[lindex $x 0]"
	foreach j [lrange $x 1 end] {
	    puts $fo "\trm -f \$(DESTDIR)/usr/share/man/man1/$j"
	    puts $fo "\tln -s [lindex $x 0] \$(DESTDIR)/usr/share/man/man1/$j"
	}
    }
    puts $fo ""
    puts $fo {.include <bsd.prog.mk>}

    close $fo
}

puts "Copy config files to $bsd/gcc_int"

# Get everything ending in .h and .def
exec sh -c "cp $gnu/*.h $gnu/*.def $bsd/gcc_int"

# Get anything still missing
foreach s $config {
    # Try to copy right away
    if {![catch {exec cp $gnu/$s $bsd/gcc_int/$s} x]} continue
    # Try to copy from below the config subdir 
    if {![catch {exec cp $gnu/config/$s $bsd/gcc_int/$s} x]} continue
    # Try to make the file then...
    makefile $s
    if {![catch {exec cp $gnu/$s $bsd/gcc_int/$s} x]} continue
    # Too BAD.
    puts stderr "I'm having troble finding the \"config\" file $s"
    exit 1
}
}

puts "Create $bsd/libobjc"

# Make a temporary Makefile in the GNU-tree
exec cp $gnu/objc/Makefile $gnu/objc/2bsd.t02

puts "Squeeze Makefile"

# Add targets to tell us what we want to know
set fi [open $gnu/objc/2bsd.t02 a]
puts $fi "\nFo1:\n\t@echo \$(OBJC_O)"
puts $fi "\nFo2:\n\t@echo \$(OBJC_H)"
close $fi

# Try it out...
set t01 [exec sh -c "( cd $gnu/objc ; make -f 2bsd.t02 Fo1)"]
set t02 [exec sh -c "( cd $gnu/objc ; make -f 2bsd.t02 Fo2)"]

# Remove it again
exec rm -f $gnu/objc/2bsd.t02

puts "Copy sources"
set l ""
foreach i $t01 {
    regsub {\.o} $i {} j
    if {[file exists $gnu/objc/$j.m]} {
	lappend l $j.m
	exec cp $gnu/objc/$j.m $bsd/libobjc
    } elseif {[file exists $gnu/objc/$j.c]} {
	lappend l $j.c
	exec cp $gnu/objc/$j.c $bsd/libobjc
    } else {
	puts stderr "Cannot locate source for objc/$i"
	exit 1
    }
}

puts "Copy includes"
exec sh -c "cp $gnu/objc/*.h $bsd/libobjc"

# This is a trick to make #include <objc/...> work here.
exec sh -c "ln -s . $bsd/libobjc/objc"

puts "Create Makefile"
set fo [open $bsd/libobjc/Makefile w]
puts $fo {

LIB =	objc
}
write_names $fo SRCS $l
write_names $fo HDRS $t02
puts $fo {
CFLAGS +=	-I${.CURDIR} -I${.CURDIR}/../gcc_int

.include <bsd.lib.mk>

.SUFFIXES: .m

.m.o:
	$(CC) -fgnu-runtime -c $(CFLAGS) $<
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

.m.po:
	$(CC) -p -fgnu-runtime -c $(CFLAGS) -o ${.TARGET} $<
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

afterinstall:
	rm -rf /usr/include/objc
	mkdir /usr/include/objc
	cd $(.CURDIR) ; install $(COPY) -o ${BINOWN} -g ${BINGRP} -m 444 $(HDRS) $(DESTDIR)/usr/include/objc
}

close $fo
