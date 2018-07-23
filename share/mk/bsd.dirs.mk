# $FreeBSD$
#
# Directory permissions management.

.if !target(__<bsd.dirs.mk>__)
__<bsd.dirs.mk>__:
# List of directory variable names to install.  Each variable name's value
# must be a full path.  If non-default permissions are desired, <DIR>_MODE,
# <DIR>_OWN, and <DIR>_GRP may be specified.
DIRS?=

.  for dir in ${DIRS:O:u}
.    if defined(${dir}) && !empty(${dir})
# Set default permissions for a directory
${dir}_MODE?=	0755
${dir}_OWN?=	root
${dir}_GRP?=	wheel
.      if defined(${dir}_FLAGS) && !empty(${dir}_FLAGS)
${dir}_FLAG=	-f ${${dir}_FLAGS}
.      endif

.      if defined(NO_ROOT)
.        if !defined(${dir}TAGS) || ! ${${dir}TAGS:Mpackage=*}
${dir}TAGS+=		package=${${dir}PACKAGE:Uruntime}
.        endif
${dir}TAG_ARGS=	-T ${${dir}TAGS:[*]:S/ /,/g}
.      endif

installdirs: installdirs-${dir}

installdirs-${dir}: ${DESTDIR}${${dir}}

${DESTDIR}${${dir}}:
	@${ECHO} installing DIRS ${dir}
	${INSTALL} ${${dir}TAG_ARGS} -d -m ${${dir}_MODE} -o ${${dir}_OWN} \
		-g ${${dir}_GRP} ${${dir}_FLAG} ${DESTDIR}${${dir}}
.    endif

realinstall: installdirs-${dir}
.  endfor

.endif
