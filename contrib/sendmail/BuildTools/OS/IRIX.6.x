#	@(#)IRIX.6.x	8.11	(Berkeley)	4/24/98
PUSHDIVERT(1)
# Select what ABI we are using -- see abi(5) for details
#    -32	~ IRIX 5.3				(default: -mips2)
#                 - long and pointer are 32 bit
#    -n32        New to IRIX 6.2			(default: -mips3)
#                 - long and pointer are 32 bit
#    -64	~ IRIX 6.1				(default: -mips4)
#                 - long and pointer are 64 bit
# We force ABI here, so then it does not depend on CPU
#
# With IDO 6.2 (IRIX 6.2) you need subsystem compiler_dev.sw32.lib
#	for compilation with ABI=-n32 -- alternatively you can set ABI=-32
# ABI=-64 requires subsystem compiler_dev.sw64.lib, but this runs
#	only with IRIX64 (ie. 64 bit kernels)
#
# NOTE: Do not set `confABI' in a site configuration file!  The ABI MUST 
#	be given on the Build command line using the -E parameter, e.g.:
#
#		Build -E ABI=-n32
#
ABI=	confABI
POPDIVERT
define(`confMAPDEF', `-DNDBM -DNIS -DMAP_REGEX')
define(`confENVDEF', `-DIRIX6 ${ABI} ')
define(`confLDOPTS', `${ABI}')
define(`confMBINDIR', `/usr/lib')
define(`confSBINDIR', `/usr/etc')
define(`confUBINDIR', `/usr/bsd')
define(`confEBINDIR', `/usr/lib')
define(`confSBINGRP', `sys')
define(`confSTDIR', `/var')
define(`confHFDIR', `/etc')
define(`confINSTALL', `${BUILDBIN}/install.sh')
define(`confDEPEND_TYPE', `CC-M')
