Tcl

by John Ousterhout (and many others at Sun Microsystems and elsewhere)
john.ousterhout@eng.sun.com

SCCS: @(#) README 1.29 96/04/19 11:42:58

1. Introduction
---------------

This directory and its descendants contain the sources and documentation
for Tcl, an embeddable scripting language.  The information here corresponds
to release 7.5.  The most important new feature in this release is support
for the PC and Mac platforms.  In addition, there are major new facilities
for dynamic loading, package and version management, multiple interpreters,
safe execution of untrusted scripts, and a new I/O system that supports
nonblocking I/O and sockets.  This release also contains many bug fixes.
Tcl 7.5 should be backwards compatible with Tcl 7.4 scripts (there are two
small incompatibilities described below, but they are relatively insignificant
and shouldn't affect most existing Tcl code and extensions).

2. Documentation
----------------

The best way to get started with Tcl is to read one of the introductory
books on Tcl:

    Tcl and the Tk Toolkit, by John Ousterhout,
    Addison-Wesley, 1994, ISBN 0-201-63337-X

    Practical Programming in Tcl and Tk, by Brent Welch,
    Prentice-Hall, 1995, ISBN 0-13-182007-9

    Exploring Expect, by Don Libes,
    O'Reilly and Associates, 1995, ISBN 1-56592-090-2

The "doc" subdirectory in this release contains a complete set of reference
manual entries for Tcl.  Files with extension ".1" are for programs (for
example, tclsh.1); files with extension ".3" are for C library procedures;
and files with extension ".n" describe Tcl commands.  The file "doc/Tcl.n"
gives a quick summary of the Tcl language syntax.  To print any of the man
pages, cd to the "doc" directory and invoke your favorite variant of
troff using the normal -man macros, for example

		ditroff -man Tcl.n

to print Tcl.n.  If Tcl has been installed correctly and your "man"
program supports it, you should be able to access the Tcl manual entries
using the normal "man" mechanisms, such as

		man Tcl

There is also an official home for Tcl and Tk on the Web:
	http://www.sunlabs.com/research/tcl
These Web pages include release updates, reports on bug fixes and porting
issues, HTML versions of the manual pages, and pointers to many other
Tcl/Tk Web pages at other sites.  Check them out!

3. Compiling and installing Tcl
-------------------------------

This release contains everything you should need to compile and run
Tcl under UNIX, Macintoshes, and PCs (either Windows NT, Windows 95,
or Win 3.1 with Win32s).

Before trying to compile Tcl you should do the following things:

    (a) Check for a binary release.  Pre-compiled binary releases are
        available now for PCs and Macintoshes, and they may be available
	in the future for some flavors of UNIX.  Binary releases are much
	easier to install than source releases.  To find out whether a
	binary release is available for your platform, check the home page
	for the Sun Tcl/Tk project (http://www.sunlabs.com/research/tcl)
	and also check in the FTP directory from which you retrieved the
	base distribution.

    (b) Check for patches.  Look in the FTP directory from which you
        retrieved the base distribution and see if there are files with
	names like tcl7.5p1.patch, tcl7.5p2.patch, etc.  These files may
	also have .gz or .Z extensions to indicate compression.  If you find
	any patch files, apply them to the source directory in order
	from "p1" up.  To apply an uncompressed patch file such as
	tcl7.5p1.patch, invoke a shell command like the following from
	the directory containing this file:
	    patch -p < tcl7.5p1.patch
	If the patch file has a .gz extension, invoke a command like the
	following:
	    gunzip -c tcl7.5p1.patch.gz | patch -p
	If the patch file has a .Z extension, it was compressed with
	compress.  To apply it, invoke a command like the following:
	    zcat tcl7.5p1.patch.Z | patch -p
	If you're applying a patch to a release that has already been
	compiled, then before applying the patch you should cd to the
	"unix" subdirectory and type "make distclean" to restore the
	directory to a pristine state.

Once you've done this, change to the "unix" subdirectory if you're
compiling under UNIX, "win" if you're compiling under Windows, or
"mac" if you're compiling on a Macintosh.  Then follow the instructions
in the README file in that directory for compiling Tcl, installing it,
and running the test suite.

4. Summary of changes in Tcl 7.5
--------------------------------

The most important change for Tcl 7.5 is that Tcl now runs on Macintosh
and PC platforms as well as UNIX.  The PC port runs under Windows 3.1
(with Win32s), Windows 95, and Windows NT.  This required a lot of
reorganization of the sources but it didn't require any changes to
Tcl's externally visible interfaces.

In addition to the ports, Tcl 7.5 also has many other new features.
The following feature changes have occurred since Tcl 7.4:

    1. Dynamic loading.  There is a new "load" command for loading binary
    extensions into Tcl on the fly.  This works now on most of the major
    UNIX platforms as well as PCs and Macintoshes.  Three new "info"
    commands, "info loaded", "info sharedlibextension", and
    "info nameofexecutable", were also added as part of the dynamic loading
    implementation.  You can also create Tcl and Tk themselves as shared
    libraries with the --enable-shared switch to the configure script.

    2. Packages and versions.  There is a new "package" command for
    package and version management.  See the manual entries for "package"
    and "pkg_mkIndex" for details on how to use it.  There are also
    C APIs to the package mechanism.  See PkgRequire.3.
    
    3. Multiple interpreters and Safe-Tcl.  There is a new "interp" command
    that allows you to create multiple interpreters within a single application
    and set up communication between them with "aliases".  The mechanism also
    supports "safe" interpreters, which provide a generalized version of the
    security mechanisms in Borenstein and Rose's Safe-Tcl.  There are still
    a few missing security features, such as resource control.  You can use
    "load" to add extensions (including Tk) into slave interpreters.

    4. The event loop from Tk has been moved to Tcl.  Tcl now has commands
    "after", "fileevent", "update", and "vwait" (which replaces tkwait).
    The "tkerror" command has been renamed to "bgerror".  "Tkerror" is
    still supported for backwards compatibility, but you should switch ASAP
    to using "bgerror" instead.  Many C procedures that used to be in Tk
    have been moved to Tcl and renamed, such as Tcl_DoOneEvent, Tcl_DoWhenIdle,
    Tcl_CreateFileHandler, and Tcl_CreateTimerHandler.

    5. Tcl has a whole new I/O system.  All of the Tcl commands like
    "open" and "puts" should continue to operate as before, but there
    is a totally new implementation that doesn't use the C stdio library:
	- The new I/O system is more portable, and it can be extended
	  with new kinds of I/O channels;  see CrtChannel.3 for details.
	- Nonblocking I/O is supported on all platforms and there is a
	  new command "fconfigure" to enable it and other channel options;
	  see fconfigure.n for details.  There is also a new "fblocked"
	  command.
	- The I/O system automatically translates between different
	  end-of-line representations (such as CR on Macs and CRLF on
	  PC's) to the newline form used in UNIX and in all Tcl scripts;
	  the "fconfigure" command can be used to control this feature.
	- There is a set of C APIs for manipulating Tcl_Channel's, which
	  are analogous to UNIX FILE's.  The C procedures have roughly the
	  same functionality as the stdio procedures.  See OpenFileChnl.3,
	  CrtCloseHdlr.3, and CrtChnlHdlr.3 for details.
	- There is a new structure Tcl_File that provides platform-
	  independent access to file handles such as UNIX fd's.  See
	  GetFile.3 for details.
	- There are new procedures Tcl_GetErrno and Tcl_SetErrno for
	  accessing the "errno" variable in a safe and portable fashion.
	  See SetErrno.3.

    6. There are new commands "file split", "file join", and "file pathtype",
    which make it possible to handle file names in a way that will work on
    all platforms.  See the manual entries file.n and filename.n for
    details.

    7. There is a new "socket" command for network communication via
    TCP sockets.  It works for both the client and server sides.  There
    is also C-level support for sockets;  see OpenTcp.3.

    8. There is a new "clock" command, which contains the functionality
    of the TclX clock-handling commands.

    9. The "foreach" command has been generalized significantly to support
    multiple lists and multiple variables iterating over each list.

    10. There is a new "notifier" mechanism, which was added as part of
    the ports.  This allows the basic mechanisms for reporting events
    to be implemented in different ways on different platforms.  It
    may also be useful for other purposes, such as merging the Tk and
    Xt event loops so that Tk and Xt widgets can coexist in a single
    application.  See the manual entry Notifier.3 for more information.

    11. There is an "AssocData" mechanism that allows extensions to store
    their own data in an interpreter and get called back when the interpreter
    is deleted.  This is visible at C level via the procedures Tcl_SetAssocData
    and Tcl_GetAssocData.

    12. When manual pages are installed, additional links are created for
    each of the procedures described in the manual page, so that it's
    easier to invoke the "man" command.

    13. There is a new variable "tcl_platform" with platform information.
    This is an associative array with elements like "os" and "machine"
    that contain various pieces of information about the platform.

    14. There is a new procedure Tcl_CreateExitHandler that you can use to
    make sure a C procedure is called before the Tcl application exits.

    15. There is a new procedure Tcl_UpdateLinkedVar to force the Tcl-level
    variable to be updated after you've changed the corresponding C-level
    variable.

    16. The procedures Tk_Preserve, Tk_Release, and Tk_EventuallyFree
    have been moved from Tk to Tcl and given names like Tcl_Preserve.

Three incompatibilities were introduced by the changes.  All of these
are at C-level, and only the first one should have much impact.  Existing
scripts for Tcl 7.4 should run unchanged  under Tcl 7.5.

    1. The procedure Tcl_EnterFile no longer exists.  However, a new
    procedure Tcl_MakeFileChannel provides similar functionality.
    Tcl_GetOpenFile still exists but only works under UNIX.
    Tcl_CreatePipeline also remains, but it too works only under UNIX
    now;  use Tcl_OpenCommandChannel for better portability.

    2. Tcl doesn't export any global C variables anymore, because this doesn't
    work with Windows DLLs.  The C variables tcl_AsyncReady and
    tcl_FileCloseProc have been replaced with procedures Tcl_AsyncReady()
    and Tcl_SetFileCloseProc().  The C variable tcl_RcFileName has been
    replaced with a Tcl variable tcl_rcFileName (use Tcl_SetVar to set the
    Tcl variable, instead of assigning to the old C variable).
    
    3. Files are no longer shared between interpreters by default:  if a
    file is opened in one interpreter, it cannot normally be used in other
    interpreters.  However, the new procedure Tcl_ShareHandle allows files
    to be shared between interpreters if requested explicitly.

For a complete list of all changes in this release, see the file "changes"
in this directory.

5. Tcl newsgroup
-----------------

There is a network news group "comp.lang.tcl" intended for the exchange
of information about Tcl, Tk, and related applications.  Feel free to use
the newsgroup both for general information questions and for bug reports.
We read the newsgroup and will attempt to fix bugs and problems reported
to it.

When using comp.lang.tcl, please be sure that your e-mail return address
is correctly set in your postings.  This allows people to respond directly
to you, rather than the entire newsgroup, for answers that are not of
general interest.  A bad e-mail return address may prevent you from
getting answers to your questions.  You may have to reconfigure your news
reading software to ensure that it is supplying valid e-mail addresses.

6. Tcl contributed archive
--------------------------

Many people have created exciting packages and applications based on Tcl
and/or Tk and made them freely available to the Tcl community.  An archive
of these contributions is kept on the machine ftp.neosoft.com.  You
can access the archive using anonymous FTP;  the Tcl contributed archive is
in the directory "/pub/tcl".  The archive also contains several FAQ
("frequently asked questions") documents that provide solutions to problems
that are commonly encountered by TCL newcomers.

7. Support and bug fixes
------------------------

We're very interested in receiving bug reports and suggestions for
improvements.  We prefer that you send this information to the
comp.lang.tcl newsgroup rather than to any of us at Sun.  We'll see
anything on comp.lang.tcl, and in addition someone else who reads 
omp.lang.tcl may be able to offer a solution.  The normal turn-around
time for bugs is 2-4 weeks.  Enhancements may take longer and may not
happen at all unless there is widespread support for them (we're
trying to slow the rate at which Tcl turns into a kitchen sink).  It's
very difficult to make incompatible changes to Tcl at this point, due
to the size of the installed base.

When reporting bugs, please provide a short tclsh script that we can
use to reproduce the bug.  Make sure that the script runs with a
bare-bones tclsh and doesn't depend on any extensions or other
programs, particularly those that exist only at your site.  Also,
please include three additional pieces of information with the
script:
    (a) how do we use the script to make the problem happen (e.g.
	what things do we click on, in what order)?
    (b) what happens when you do these things (presumably this is
        undesirable)?
    (c) what did you expect to happen instead?

The Tcl community is too large for us to provide much individual
support for users.  If you need help we suggest that you post questions
to comp.lang.tcl.  We read the newsgroup and will attempt to answer
esoteric questions for which no-one else is likely to know the answer.
In addition, Tcl support and training are available commercially from
NeoSoft (info@neosoft.com), Computerized Processes Unlimited
(gwl@cpu.com), and Data Kinetics (education@dkl.com).

8. Tcl version numbers
----------------------

Each Tcl release is identified by two numbers separated by a dot, e.g.
6.7 or 7.0.  If a new release contains changes that are likely to break
existing C code or Tcl scripts then the major release number increments
and the minor number resets to zero: 6.0, 7.0, etc.  If a new release
contains only bug fixes and compatible changes, then the minor number
increments without changing the major number, e.g. 7.1, 7.2, etc.  If
you have C code or Tcl scripts that work with release X.Y, then they
should also work with any release X.Z as long as Z > Y.

Alpha and beta releases have an additional suffix of the form a2 or b1.
For example, Tcl 7.0b1 is the first beta release of Tcl version 7.0,
Tcl 7.0b2 is the second beta release, and so on.  A beta release is an
initial version of a new release, used to fix bugs and bad features before
declaring the release stable.  An alpha release is like a beta release,
except it's likely to need even more work before it's "ready for prime
time".  New releases are normally preceded by one or more alpha and beta
releases.  We hope that lots of people will try out the alpha and beta
releases and report problems.  We'll make new alpha/beta releases to fix
the problems, until eventually there is a beta release that appears to
be stable.  Once this occurs we'll make the final release.

We can't promise to maintain compatibility among alpha and beta releases.
For example, release 7.1b2 may not be backward compatible with 7.1b1, even
though the final 7.1 release will be backward compatible with 7.0.  This
allows us to change new features as we find problems during beta testing.
We'll try to minimize incompatibilities between beta releases, but if
a major problem turns up then we'll fix it even if it introduces an
incompatibility.  Once the official release is made then there won't
be any more incompatibilities until the next release with a new major
version number.
