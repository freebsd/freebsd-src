#	@(#)NEWS-OS.4.x	8.6	(Berkeley)	3/12/1998
define(`confBEFORE', `limits.h')
define(`confMAPDEF', `-DNDBM')
define(`confLIBS', `-lmld')
define(`confMBINDIR', `/usr/lib')
define(`confSBINDIR', `/usr/etc')
define(`confUBINDIR', `/usr/ucb')
define(`confEBINDIR', `/usr/lib')
define(`confSTDIR', `/usr/lib')
define(`confHFDIR', `/usr/lib')
PUSHDIVERT(3)
limits.h:
	touch limits.h
POPDIVERT
