.Go 9 "MAKEFILE"
.PP
On most Operating Systems, and with most compilers, the "Makefile.mix"
file is used to control compilation and installation of \*E.
This section of the manual describes the overall structure of "Makefile.mix",
and the various configuration options in it.
.NH 2
Configuring the Makefile
.PP
Begin by copying "Makefile.mix" to "Makefile".
Never alter the original "Makefile.mix".
.PP
Most of the configuration options are controlled via a group of macros.
Makefile.mix begins with several pre-configured sets of macro definitions \-
one group for each of the most common supported systems.
As shipped, all of these macro definitions are commented out;
you must either uncomment out one of the groups, or
(for less common systems) construct an entirely new group.
.NH 2
Using the Makefile
.PP
After configuring the Makefile, you can run "make" to compile the programs.
There are also some other useful things that the Makefile can do...
.LD
.ta 1i 2.2i
	COMMAND	RESULT
	make	compile all programs
	make install	copy the programs to the BIN directory
	make clean	remove all object files
	make clobber	remove everything except source & documentation
	make uue	produce uuencoded compressed tar archives of the source
	make sh	produce shar archives of the source
.DE
.PP
Note that the last two will probably work only under UNIX.
.NH 2
What "make install" does
.PP
To install elvis, we should copy all of the executables into a directory
where users can find them;
copy the documentation into a directory where the on-line manual program
can find them;
and arrange for edit buffers to be preserved after a system crash.
The "make install" command tries to do this automatically,
but there are problems.
.PP
Practically all operating systems allow programs to be installed different
directories.
As shipped, Makefile.mix contains somebody's best guess as to where you'd
like them to go.
You should double check it, though.
The BIN macro controls where the programs will be installed.
.PP
On UNIX systems the "elvprsv" and "elvrec" programs need to be
installed as SUID-root programs.
Consequently, you must run "make install" as root;
then they will automatically be installed as SUID-root.
.PP
For text to be recovered after a crash, you need to arrange for the
"elvprsv" program to be run before the /tmp file is cleaned.
This means that the /etc/rc file (or whatever) needs to be edited.
If you have a SysV UNIX system which uses a /etc/rc2.d directory for storing
start-up commands, then you're lucky.
"make install" will detect that /etc/rc2.d exists and attempt to
automatically create a file called "/etc/rc2.d/S03elvis" which runs elvprsv.
However, for non-UNIX systems, or UNIX systems which don't have a /etc/rc2.d
directory, you'll need to do this by hand.
See the "Versions" section of the manual for hints about doing this on
your particular system.
.PP
Non-UNIX systems don't have a standard place where UNIX-style man-pages
go, so "make install" doesn't attempt to install documentation on those
systems.
.PP
On UNIX systems, there is no standard place either, but you can be
pretty sure that your system has a non-standard one.
There is a shell script called "instman.sh" which attempts to figure
out where the man-pages belong on your system, and then copies them there.
You might need to edit "instman.sh" to make it work, but try it as-is first.
"instman.sh" is automatically run by "make install".
.PP
Note: It is safe to run "make install" more than once.
.NH 2
Summary of Macros
.PP
The following describes the configuration macros.
With most versions of make, a blank macro can simply be left undefined.
.IP OBJ
This is the filename extension for unlinked object files
\- usually .o, but MS-DOS uses .obj.
.IP EXE
This is the filename extension for elvis executable file
\- usually nothing, but MS-DOS uses .exe,
and other operating systems may use something else.
.IP COM
This is the filename extension for the executables of elvis' support programs
\- usually the same as the EXE macro, but since the support programs are all
much smaller that elvis, MS-DOS can use the .com format.
.IP EXTRA
This is a space-delimited list of version-specific object files to be linked
into elvis.
Typically, this list will contain at least one object file which was written
specifically for a given operating system.
It may also contain "tinytcap$(OBJ)" or "tinyprnt$(OBJ)".
.IP EXTRA2
This is a space-delimited list of version-specific object files used in elvis
and a few of the support programs.
For UNIX-like systems, this is typically an empty list.
For non-UNIX systems, it will usually either be empty, or it will contain
the name of an object file which contains functions which emulate certain
UNIX system calls.
(Not all non-UNIX systems need any special emulation functions, because
all C libraries try to emulate UNIX.  You only need an EXTRA2 list if
the library doesn't emulate UNIX well enough.)
.IP LIBS
This is a list of library flags used while linking elvis.
UNIX systems need "-ltermcap" or something similar, unless the EXTRA
macro includes "tinytcap$(OBJ)".
Most other operating systems use "tinytcap$(OBJ)" and don't need anything else,
so they leave the LIBS list empty.
.IP BIN
This is the directory where executables should be installed by "make install".
.IP CC
This is the C compiler command, possibly with "memory model" flags.
.IP CFLAGS
This lists the compiler flags used to select compile-time options.
The "CFLAGS" section of this manual describes this in detail.
.IP LNK
This is the name of the linker.
If you want to use $(CC) as your linker, then you can leave LNK undefined.
.IP LFLAGS
This is a list of linker flags used to select link-time options.
It is almost always blank.
.IP SMALL
The flag for special small memory model compilation \- usually blank.
.IP OF
The link flag to control the output file's name \- usually -o<space>.
The Sun version of "make" strips off trailing whitespace, so a pair
of empty quotes has been added after the space, to protect it.
On non-Suns, this isn't necessary.
.IP RF
The flag used to denote "compile but don't link" \- usually -c
.IP PROGS
This is a space-delimited list of all programs.
This list always includes elvis, ctags, ref, elvrec, and elvprsv.
Also, everybody gets fmt except for BSD UNIX; it already has its own
version of fmt as standard equipment.
.IP
Most non-UNIX systems also include the vi, ex, and view aliases.
(UNIX doesn't need those aliases in the PROGS list because it creates
them via file links during installation.)
OS-9 doesn't include the ex alias, because there is already a command
by that name built into its standard shell.
.IP
Note: some MS-DOS configurations break this list into two smaller lists,
to compensate for MS-DOS's limitations on command line length.
.IP CHMEM
This is either blank, or a command to be run immediately after linking elvis.
Under Minix and Coherent, elvis needs to have extra space assigned for the
stack & heap after it has been linked,
so their commands to do that are placed here.
Most other operating systems generally either don't need to have their stacks
enlarged, or they enlarge it during linking.
.IP SORT
This should be defined to be -DSORT if you want your tags list to be sorted,
or blank if you want it unsorted.
The real vi requires a sorted tags file, so for the sake of compatibility
all of the UNIX configurations use -DSORT.
Elvis doesn't need a sorted tags file, though, so on non-UNIX systems you
can leave this macro blank.
.IP RM
This is the name of a program that deletes files unconditionally.
It is used during "make clean".
RM is defined as "rm -f" for UNIX systems, or "del" for most others.
.IP CP
This is the name of a program that copies files.
\- usually "cp" or "copy".
It is used during "make install".
.IP SYS
This is the type of system.
It is used to select an appropriate style of linking and installation
that are used by "make" and "make install", respectively.
The available types are:
.LD
.ta 2i 3i
	unx	UNIX and UNIX-like systems
	dos	MS-DOS
	ami	AmigaDos
	tos	Atari TOS
	os9	OS-9/68k
	vms	VAX/VMS
	xdos	cross-compiled on SCO for MS-DOS
.DE
.IP DUMMY
This is used as the "source" filename in the dependency list of
targets which are supposed to be unconditionally compiled.
It is usually nothing since most versions of "make" treat an
empty source file list as a special case,
but OS-9 needs it defined as "dummy" and further requires that
there be no actual file named dummy.
.IP CFG
The is the name of the compiler configuration file \- usually blank,
since most compilers don't need a configuration file.
Some MS-DOS compilers need it, though.
.NH 2
Structure of Makefile.mix
.PP
Makefile.mix begins with several sets of commented out configuration
macro definitions, as described above.
A comment saying "The rest of this Makefile contains no user-serviceable parts"
marks the end of this section.
Most people won't need to edit anything after that.
.PP
This is followed by macro definitions which are identical,
regardless of your operating system.
The OBJS macros list the object files that form the portable parts of
elvis, and are used together with the EXTRA and EXTRA2 configuration macros
during linking.
.PP
The SRC macros list all of the files mentioned in the "MANIFEST" file.
These are used to bundle the source code via "make uue" or "make sh".
.PP
This is followed by a target named "all" which depends on all of the
programs listed in the PROGS configuration macro.
This is followed by detailed instructions describing how each file is
compiled and linked.
The only exceptions are the "elvis" program, and the various forms of the
"alias" program.
.PP
Linking a big program like elvis is non-standard on some systems.
To support this, we just say that elvis depends on "linkelv.$(SYS)",
where "$(SYS)" is replaced by whatever you defined the SYS configuration
macro to be.
The various link styles are listed after that.
The only really tricky one is for DOS.
Since the list of files to be linked is too long to fit on a DOS command line,
a customized response file is created, and the name of the response file
is passed instead.
The exact format of the response file depends on the compiler you're using.
.PP
This is followed by system-dependent ways of linking the "alias" object
file to create multiple executables.
For most systems, we only really link it once to form the "ex" executable,
and then copy that executable to form the "vi", "view", and "input" executables.
OS-9, though, doesn't need an "ex" executable and it requires actual
linking for each alias.
.PP
Next comes installation, in all its system dependent forms.
This uses the now-familiar trick of saying that the "install" target
depends on a bogus file named "inst.$(SYS)" and then listing each
installation technique after that.
There should be no surprises here.
.PP
The rest of Makefile.mix contains a few handy pseudo-targets,
such as "make clean".
