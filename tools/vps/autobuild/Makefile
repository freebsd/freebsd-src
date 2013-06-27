#
# Klaus P. Ohrhallinger <k@7he.at>, 2012-2013
# 
# $Id: Makefile 128 2013-04-09 18:07:32Z klaus $
#

FREEBSD_VERSION=91
ORIGSRCURL=ftp://ftp.at.freebsd.org/pub/FreeBSD/releases/amd64/9.1-RELEASE/src.txz
SVNURL='svn+ssh://klaus@svn.7he.at/svn/vps/trunk'

BUILDER32_HOST=builder32.7he.at
BUILDER32_USER=root

SOURCES=usr/src/sys usr/src/sbin/mount usr/src/include

DIFF_EXCLUDE=".svn\nrsync_vps.amd64\nrsync_vps.i386\nautobuild\n\*nodiff\n"

#
#
#

# debug
COPTFLAGS= -pipe
CFLAGS= -fno-strict-aliasing -pipe

# prod
#COPTFLAGS= -pipe -O2
#CFLAGS= -fno-strict-aliasing -pipe -O2


all:	fetch extract diff patch build summary
#all:	build summary

build:	build_amd64_dbg build_amd64_prod build_i386_dbg build_i386_prod
#build:	build_amd64_dbg build_i386_dbg
#build:	build_amd64_prod build_i386_prod
#build:	build_amd64_dbg
#build:	build_i386_dbg

clean:
	rm -rf work

fetch: fetch_orig fetch_vps

extract: extract_pre extract_orig extract_vps

fetch_orig:
	if [ ! -s "src.txz" ] \
	; then \
		fetch -4 ${ORIGSRCURL} \
	; else \
		echo "source already downloaded" \
	; fi

fetch_vps:
	if [ ! -d "trunk" ] \
	; then \
		svn co ${SVNURL} \
	; else \
		echo "svn already checked out" \
	; fi

extract_pre:
	mkdir work

extract_orig:
	mkdir work/orig
	tar xJpf src.txz -C work/orig ${SOURCES}
	mv work/orig/usr/src/* work/orig/
	rm -r work/orig/usr

extract_vps:
	cp -a trunk work
	rm -rf work/trunk/web

diff:
	svn info trunk | grep Revision | awk '{print $$2}' > ./work/svnversion
	[ -e "vps.diff" ] && rm vps.diff || true
	printf ${DIFF_EXCLUDE} > ./work/diffexclude
	# diff: 0 -> equal; 1 -> different; 2 -> trouble
	(cd work; diff -Naupr -X ./diffexclude -I '$$Id.*$$' -I '$$FreeBSD.*$$' -w orig trunk \
		> ../vps-fbsd${FREEBSD_VERSION}-r`cat ./svnversion`.diff) || true
	ls -lh vps-fbsd${FREEBSD_VERSION}-r`cat ./work/svnversion`.diff
	ln -s vps-fbsd${FREEBSD_VERSION}-r`cat ./work/svnversion`.diff vps.diff
	rm ./work/diffexclude

patch:
	[ -d "work/build" ] && rm -rf work/build || true
	cp -a work/orig work/build
	(cd work/build/; patch -p1 < ../../vps.diff)
	cp -v work/trunk/tools/vps/rsync/rsync_vps.amd64 \
		work/build/tools/vps/rsync/rsync_vps.amd64
	cp -v work/trunk/tools/vps/rsync/rsync_vps.i386 \
		work/build/tools/vps/rsync/rsync_vps.i386

summary:
	ls -lh vps*.diff
	ls -lh work/build/vps*.tbz

build_amd64_dbg:
	printf "COPTFLAGS= -pipe\nCFLAGS= -fno-strict-aliasing -pipe\n" > \
		/tmp/make-dbg.conf
	(cd work/build/sys/amd64/conf && \
		config VPS_DBG)
	(cd work/build/sys/amd64/compile/VPS_DBG && \
		make cleandepend __MAKE_CONF=/tmp/make-dbg.conf && \
		make depend __MAKE_CONF=/tmp/make-dbg.conf && \
		make kernel __MAKE_CONF=/tmp/make-dbg.conf && \
		make modules __MAKE_CONF=/tmp/make-dbg.conf)
	make do-package \
		COMPILEDIR=work/build/sys/amd64/compile/VPS_DBG \
		ARCH2=amd64 \
		TYPE2=dbg \
		MAKE_CONF=/tmp/make-dbg.conf
	
build_amd64_prod:
	printf "COPTFLAGS= -pipe -O2\nCFLAGS= -fno-strict-aliasing -pipe -O2 -Wuninitialized\n" > \
		./work/make-prod.conf
	(cd work/build/sys/amd64/conf && \
		config VPS_PROD)
	(cd work/build/sys/amd64/compile/VPS_PROD && \
		make cleandepend __MAKE_CONF=./work/make-prod.conf && \
		make depend __MAKE_CONF=./work/make-prod.conf && \
		make kernel __MAKE_CONF=./work/make-prod.conf && \
		make modules __MAKE_CONF=./work/make-prod.conf)
	make do-package \
		COMPILEDIR=work/build/sys/amd64/compile/VPS_PROD \
		ARCH2=amd64 \
		TYPE2=prod \
		MAKE_CONF=/tmp/make-prod.conf

build_i386_dbg:
	#[ ! -d "${BUILDER32}/tmp/work" ]
	cp Makefile ./work/Makefile
	rm -rf ./work/pkg-files
	cp -a pkg-files ./work/pkg-files
	rsync -av --delete -e "ssh -4 -l ${BUILDER32_USER}" work ${BUILDER32_HOST}:/tmp/${BUILDER32_USER}/
	ssh -4 -l ${BUILDER32_USER} ${BUILDER32_HOST} "cd /tmp/${BUILDER32_USER} && make -f work/Makefile __build_i386_dbg"
	rsync -av --delete -e "ssh -4 -l ${BUILDER32_USER}" ${BUILDER32_HOST}:/tmp/${BUILDER32_USER}/work ./


__build_i386_dbg:
	printf "COPTFLAGS= -pipe\nCFLAGS= -fno-strict-aliasing -pipe\n" > \
		/tmp/${BUILDER32_USER}/make-dbg.conf
	(cd work/build/sys/i386/conf && \
		config VPS_DBG)
	(cd work/build/sys/i386/compile/VPS_DBG && \
		make cleandepend __MAKE_CONF=/tmp/make-dbg.conf && \
		make depend __MAKE_CONF=/tmp/make-dbg.conf && \
		make kernel __MAKE_CONF=/tmp/make-dbg.conf && \
		make modules __MAKE_CONF=/tmp/make-dbg.conf )
	rm -rf ./pkg-files
	mv work/pkg-files ./pkg-files
	make -f work/Makefile do-package \
		COMPILEDIR=work/build/sys/i386/compile/VPS_DBG \
		ARCH2=i386 \
		TYPE2=dbg \
		MAKE_CONF=/tmp/${BUILDER32_USER}/make-dbg.conf
	rm -rf ./pkg-files
	
build_i386_prod:
	#[ ! -d "${BUILDER32}/tmp/work" ]
	cp Makefile ./work/Makefile
	rm -rf ./work/pkg-files
	cp -a pkg-files ./work/pkg-files
	rsync -av --delete -e "ssh -4 -l ${BUILDER32_USER}" work ${BUILDER32_HOST}:/tmp/${BUILDER32_USER}/
	ssh -4 -l ${BUILDER32_USER} ${BUILDER32_HOST} "cd /tmp/${BUILDER32_USER} && make -f work/Makefile __build_i386_prod"
	rsync -av --delete -e "ssh -4 -l ${BUILDER32_USER}" ${BUILDER32_HOST}:/tmp/${BUILDER32_USER}/work ./


__build_i386_prod:
	printf "COPTFLAGS= -pipe -O2\nCFLAGS= -fno-strict-aliasing -pipe -O2 -Wuninitialized\n" > \
		/tmp/make-prod.conf
	(cd work/build/sys/i386/conf && \
		config VPS_PROD)
	(cd work/build/sys/i386/compile/VPS_PROD && \
		make cleandepend __MAKE_CONF=/tmp/make-prod.conf && \
		make depend __MAKE_CONF=/tmp/make-prod.conf && \
		make kernel __MAKE_CONF=/tmp/make-prod.conf && \
		make modules __MAKE_CONF=/tmp/make-prod.conf )
	rm -rf ./pkg-files
	mv work/pkg-files ./pkg-files
	make -f work/Makefile do-package \
		COMPILEDIR=work/build/sys/i386/compile/VPS_PROD \
		ARCH2=i386 \
		TYPE2=prod \
		MAKE_CONF=/tmp/make-prod.conf
	rm -rf ./pkg-files

do-package:
	mkdir "${COMPILEDIR}/dist"
	(cd ${COMPILEDIR} && make install DESTDIR=`pwd`/dist)
	mkdir -p ${COMPILEDIR}/dist/sbin
	mkdir -p ${COMPILEDIR}/dist/usr/sbin
	mkdir -p ${COMPILEDIR}/dist/usr/share/man/man4
	mkdir -p ${COMPILEDIR}/dist/usr/share/man/man5
	mkdir -p ${COMPILEDIR}/dist/usr/share/man/man8
	mkdir -p ${COMPILEDIR}/dist/usr/share/man/man9
	mkdir -p ${COMPILEDIR}/dist/etc/vps
	(DIR=`realpath ${COMPILEDIR}`; \
		cd ${COMPILEDIR}/../../../../usr.sbin/vpsctl && \
		make clean __MAKE_CONF=${MAKE_CONF} && \
		make __MAKE_CONF=${MAKE_CONF} && \
		make install DESTDIR=$${DIR}/dist __MAKE_CONF=${MAKE_CONF} && \
		make clean __MAKE_CONF=${MAKE_CONF} )
	(DIR=`realpath ${COMPILEDIR}`; \
		cd ${COMPILEDIR}/../../../../sbin/mount_vpsfs && \
		make clean __MAKE_CONF=${MAKE_CONF} && \
		make __MAKE_CONF=${MAKE_CONF} && \
		make install DESTDIR=$${DIR}/dist __MAKE_CONF=${MAKE_CONF} && \
		make clean __MAKE_CONF=${MAKE_CONF} )
	install -o root -g wheel -m 0555 \
		${COMPILEDIR}/../../../../tools/vps/rsync/rsync_vps.${ARCH2} \
		${COMPILEDIR}/dist/usr/sbin/rsync_vps
	# various other files that go into package
	install -o root -g wheel -m 0644 \
		pkg-files/dist/etc_vps_example.conf \
		${COMPILEDIR}/dist/etc/vps/vps_example.conf
	install -o root -g wheel -m 0755 \
		pkg-files/dist/vpssetup.sh \
		${COMPILEDIR}/dist/etc/rc.vpssetup.sh
	# all installed now
	echo "@cwd /" > ${COMPILEDIR}/pkg-plist
	echo "@comment ORIGIN:misc/vps" >> ${COMPILEDIR}/pkg-plist
	echo "@srcdir `realpath ${COMPILEDIR}/dist`" >> ${COMPILEDIR}/pkg-plist
	echo "@exec mkdir -p /etc/vps" >> ${COMPILEDIR}/pkg-plist
	echo "@exec mkdir -p /usr/vps" >> ${COMPILEDIR}/pkg-plist
	(cd ${COMPILEDIR}/dist && find . -type f >> ../pkg-plist)
	echo "@dirrm /etc/vps" >> ${COMPILEDIR}/pkg-plist
	echo "@dirrm /usr/vps" >> ${COMPILEDIR}/pkg-plist
	pkg_create -v \
		-D pkg-files/pkg-message \
		-c pkg-files/pkg-comment \
		-d pkg-files/pkg-descr \
		-i pkg-files/pkg_install_pre.sh \
		-I pkg-files/pkg_install_post.sh \
		-k pkg-files/pkg_remove_pre.sh \
		-K pkg-files/pkg_remove_post.sh \
		-f ${COMPILEDIR}/pkg-plist \
		work/build/vps-fbsd${FREEBSD_VERSION}-r`cat ./work/svnversion`-${ARCH2}-${TYPE2}.tbz

	# clean up
	rm -rf "${COMPILEDIR}/dist"
	rm -f ${COMPILEDIR}/pkg-plist

# EOF
