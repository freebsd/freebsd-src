###
#	$Id: crunch_fs.mk,v 1.1 1996/05/21 01:12:21 julian Exp $
#
# This is included to make a floppy that includes a crunch file
#
# Variables that control this mk include file.
# TOP		specifies where the top of the FreeBSD source tree is.. (*)
# FS_DIRS	directories to make on the fs (*)
# STANDLINKS	added symlinks to /stand on the fs
# VERBATIM	a directory that contains tree to be copied to the fs
# FSSIZE	defaults to	1200
# FSLABEL	defaults to	fd1200
# FSINODE	defaults to	4300
# FS_DEVICES	devices to make on the fs (using MAKEDEV) (default = all)
# ZIP		decides if the installed cruch will also be gzip'd(def=true)
# (*) = Mandatory
###

# If we weren't told, default to nothing
.if ! defined( TOP )
# define TOP!
xxx
.endif

# mountpoint for filesystems.
MNT=			/mnt

# other floppy parameters.
FSSIZE?=		1200
FSLABEL?=		fd1200
FSINODE?=		4300
FS_DEVICES?= 		all
ZIP?=true

# Things which will get you into trouble if you change them
TREE=		tree
LABELDIR=	${OBJTOP}/sys/i386/boot/biosboot

clean:	
	rm -rf tree fs-image fs-image.size step[0-9]

.include <bsd.prog.mk>


#
# --==## Create a filesystem image ##==--
#

fs_image:	${TREE} step2 step3 step4 fs-image 

${TREE}: ${.CURDIR}/Makefile
	rm -rf ${TREE}
	mkdir -p ${TREE}
	cd ${TREE} && mkdir ${FS_DIRS}
	cd ${TREE} ; for i in ${STANDLINKS} ;\
	do ; \
		ln -s /stand $${i} ; \
	done

step2: ${.CURDIR}/${CRUNCHDIRS} ${.CURDIR}/Makefile
.if defined(CRUNCHDIRS)
	@cd ${.CURDIR} && $(MAKE) installCRUNCH DIR=${TREE}/stand ZIP=${ZIP}
.endif
	touch step2

step3:	step2 
.if defined (FS_DEVICES)
	( cd tree/dev && \
		cp ${TOP}/etc/etc.i386/MAKEDEV . && sh MAKEDEV ${FS_DEVICES} )
.endif
	touch step3

step4: step3
.if defined(VERBATIM)
	A=`pwd`;cd ${.CURDIR}/${VERBATIM}; \
	find . \! \(  -name CVS  -and -prune \) -print |cpio -pdmuv $$A/tree
.endif
	true || cp ${TOP}/etc/spwd.db tree/etc
	touch step4

fs-image: step4
	sh -e ${FS_BIN}/doFS.sh ${LABELDIR} ${MNT} ${FSSIZE} tree \
		10000 ${FSLABEL}


.if defined(CRUNCHDIRS)
installCRUNCH:
.if !defined(DIR)
	@echo "DIR undefined in installCRUNCH" && exit 1
.endif
.if !defined(ZIP)
	@echo "ZIP undefined in installCRUNCH" && exit 1
.endif
.for CRUNCHDIR in ${CRUNCHDIRS}
	if ${ZIP} ; then \
		gzip -9 < ${CRUNCHDIR}/crunch > ${DIR}/.crunch ; \
	else \
		ln -f ${CRUNCHDIR}/crunch ${DIR}/.crunch ; \
	fi
	chmod 555 ${DIR}/.crunch
	for i in `crunchgen -l ${.CURDIR}/${CRUNCHDIR}/crunch.conf` ; do \
		ln -f ${DIR}/.crunch ${DIR}/$$i ; \
	done
	rm -f ${DIR}/.crunch
.endfor
.endif

