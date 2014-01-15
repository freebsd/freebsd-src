#
# $Id: elftoolchain.os.mk 2068 2011-10-26 15:49:07Z jkoshy $
#

# OS specific build instructions

.if !defined(OS_HOST)

# Determine the host operating system flavor.
OS_HOST != uname -s

# Bring in OS-specific Makefiles, if they exist
.if exists(${TOP}/mk/os.${OS_HOST}.mk)
.include "${TOP}/mk/os.${OS_HOST}.mk"
.endif

# Bring in per-subproject OS-specific Makefiles, if they exist
.if exists(${.CURDIR}/os.${OS_HOST}.mk)
.include "${.CURDIR}/os.${OS_HOST}.mk"
.endif

.endif
