# $Id: Makefile 3690 2019-02-23 22:51:13Z jkoshy $

TOP=	../../../..

YAML_FILES=	check_elf \
		getclass \
		ehdr \
		ehdr-malformed-1 \
		fsize \
		newehdr newscn newscn2 \
		phdr \
		rdwr rdwr1 rdwr2 \
		shdr \
		u1 \
		versioning \
		xlate xscn-1 xscn-2 xscn-3 \
		zerosection

# Generate ELF binary files from their YAML desciptions.
.for f in ${YAML_FILES}
. for e in msb lsb
.  for c in 32 64
_YAML_ELF+=	${f}.${e}${c}
${f}.${e}${c}: ${f}.yaml
	_E=`echo ${e} | tr '[a-z]' '[A-Z]'`; _C=`echo ${c} | tr '[a-z]' '[A-Z]'`; \
	cat ${.CURDIR}/${f}.yaml | sed -e "s/ELFDATANONE/ELFDATA2$${_E}/g" \
		-e "s/ELFCLASSNONE/ELFCLASS$${_C}/g" | \
		${TS_ROOT}/bin/elfc -o ${.TARGET}
.  endfor
. endfor
.endfor

CLEANFILES+=	${_YAML_ELF}

all:	${_YAML_ELF}

.include "${TOP}/mk/elftoolchain.tet.mk"
