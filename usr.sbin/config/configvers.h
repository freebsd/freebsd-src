/*
 * 6 digits of version.  The most significant are branch indicators
 * (eg: RELENG_2_2 = 22, -current presently = 70 etc).  The least
 * significant digits are incremented as needed.
 *
 * DO NOT CASUALLY BUMP THIS NUMBER!  The rules are not the same as shared
 * libs or param.h/osreldate.
 *
 * It is the version number of the protocol between config(8) and the
 * sys/conf/ Makefiles (the kernel build system).
 *
 * It is now also used to trap certain problems that the syntax parser cannot
 * detect.
 *
 * Unfortunately, there is no version number for user supplied config files.
 *
 * Once, config(8) used to silently report errors and continue anyway.  This
 * was a huge problem for 'make buildkernel' which was run with the installed
 * /usr/sbin/config, not a cross built one.  We started bumping the version
 * number as a way to trap cases where the previous installworld was not
 * compatable with the new buildkernel.  The buildtools phase and much more
 * comprehensive error code returns solved this original problem.
 *
 * Most end-users will use buildkenel and the build tools from buildworld.
 * The people that are inconvenienced by gratuitous bumps are developers
 * who run config by hand. 
 *
 * $FreeBSD$
 */
#define	CONFIGVERS	700000

/*
 * Examples of when there should NOT be a bump:
 * - Adding a new keyword
 * - Changing the syntax of a keyword such that old syntax will break config.
 * - Changing the syntax of a keyword such that new syntax will break old
 *   config binaries.
 *
 * Examples of when there should be a bump:
 * - When files generated in sys/$mach/compile/NAME are changed and the
 *   Makefile.$mach rules might not handle it correctly.
 * - When there are incompatable changes to the way sys/conf/files.* or the
 *   other associated files are parsed such that they will be interpreted
 *   incorrectly rather than fail outright.
 */
