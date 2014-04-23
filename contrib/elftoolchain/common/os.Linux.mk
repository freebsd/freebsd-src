#
# Build recipes for Linux based operating systems.
#
# $Id: os.Linux.mk 2064 2011-10-26 15:12:32Z jkoshy $

_NATIVE_ELF_FORMAT = native-elf-format

.BEGIN:	${_NATIVE_ELF_FORMAT}.h

${_NATIVE_ELF_FORMAT}.h:
	${.CURDIR}/${_NATIVE_ELF_FORMAT} > ${.TARGET} || rm ${.TARGET}

CLEANFILES += ${_NATIVE_ELF_FORMAT}.h
