#!/bin/sh
# Fetches, builds and store the result of a heimdal build
# Version: $Id: heimdal-build.sh 21653 2007-07-18 20:15:59Z lha $

fetchmethod=wget	   #options are: wget, curl, ftp, afs
resultdir=
email=heimdal-build-log@it.su.se
baseurl=ftp://ftp.pdc.kth.se/pub/heimdal/src
afsdir=/afs/pdc.kth.se/public/ftp/pub/heimdal/src
keeptree=no
passhrase=
builddir=
noemail=
cputimelimit=3600
confflags=

# Add some bonus paths, to find sendmail and other tools
# on interesting platforms.
PATH="${PATH}:/usr/sbin:/usr/bin:/usr/libexec:/usr/lib"
PATH="${PATH}:/usr/local/bin:/usr/local/sbin"

# no more user configurabled part below (hopefully)

usage="[--current] [--svn SourceRepository] [--cvs-flags] [--result-directory dir] [--fetch-method wget|ftp|curl|cvs|fetch|afs] --keep-tree] [--autotools] [--passhrase string] [--no-email] [--build-dir dir] [--cputime] [--distcheck] [--test-environment env] [--configure-flags flags]"

date=`date +%Y%m%d`
if [ "$?" != 0 ]; then
    echo "have no sane date, punting"
    exit 1
fi

hostname=`hostname`
if [ "$?" != 0 ]; then
    echo "have no sane hostname, punting"
    exit 1
fi

version=`grep "^# Version: " "$0" | cut -f2- -d:`
if [ "X${version}" = X ]; then
    echo "Can not figure out what version I am"
    exit 1
fi

dir=
hversion=
cvsroot=
cvsflags=
cvsbranch=
branch=
autotools=no
distcheck=no

while true
do
	case $1 in
	--autotools)
		autotools=yes
		shift
		;;
	--build-dir)
		builddir="$2"
		shift 2
		;;
	--current)
		dir="snapshots/"
		hversion="heimdal-${date}"
		shift
		;;
	--release)
		hversion="heimdal-$2"
		shift 2
		;;
	--cputime)
		cputimelimit="$2"
		shift 2
		;;
	--ccache-dir)
		ccachedir="$2"
		shift 2
		;;
	--svn)
		hversion="heimdal-svn-${date}"
		svnroot=$2
		fetchmethod=svn
		shift 2
		;;
	--distcheck)
		distcheck=yes
		shift
		;;
	--result-directory)
		resultdir="$2"
		if [ ! -d "$resultdir" ]; then
		    echo "$resultdir doesn't exists"
		    exit 1
		fi
		resultdir="`pwd`/${resultdir}"
		shift 2
		;;
	--fetch-method)
		fetchmethod="$2"
		shift 2
		;;
	--keep-tree)
		keeptree=yes
		shift
		;;
	--passphrase)
		passhrase="$2"
		shift 2
		;;
	--prepend-path)
		prependpath="$2"
		shift 2
		;;
	--test-environment)
		testenvironment="$2"
		shift 2
		;;
	--no-email)
		noemail="yes"
		shift
		;;
	--configure-flags)
		confflags="${confflags} $2"
		shift 2
		;;
	--version)
		echo "Version: $version"
		exit 0
		;;
	-*)
		echo "unknown option: $1"
		break
		;;
	*)
		break
		;;
	esac
done
if test $# -gt 0; then
	echo $usage
	exit 1
fi

if [ "X${hversion}" = X ]; then
	echo "no version given"
	exit 0
fi

hfile="${hversion}.tar.gz"
url="${baseurl}/${dir}${hfile}"
afsfile="${afsdir}/${dir}${hfile}"
unpack=yes

# extra paths for the user
if [ "X${prependpath}" != X ]; then
	PATH="${prependpath}:${PATH}"
fi

# Limit cpu seconds this all can take
ulimit -t "$cputimelimit" > /dev/null 2>&1

if [ "X${builddir}" != X ]; then
	echo "Changing build dir to ${builddir}"
	cd "${builddir}"
fi

echo "Removing old source" 
rm -rf ${hversion}

echo "Fetching ${hversion} using $fetchmethod"
case "$fetchmethod" in
wget|ftp|fetch)
	${fetchmethod} $url > /dev/null
	res=$?
	;;
curl)
	${fetchmethod} -o ${hfile} ${url} > /dev/null
	res=$?
	;;
afs)
	cp ${afsfile} ${hfile}
	res=$?
	;;
svn)
	svn co $svnroot ${hversion}
	res=$?
	unpack=no
	autotools=yes
	;;
*)
	echo "unknown fetch method"
	;;
esac

if [ "X$res" != X0 ]; then
	echo "Failed to download the tar-ball"
	exit 1
fi

if [ X"$unpack" = Xyes ]; then
	echo Unpacking source
	(gzip -dc ${hfile} | tar xf -) || exit 1
fi

if [ X"$autotools" = Xyes ]; then
	echo "Autotooling"
	(cd ${hversion} && sh ./autogen.sh) || exit 1
fi

if [ X"$ccachedir" != X ]; then
	CCACHE_DIR="${ccachedir}"
	export CCACHE_DIR
fi

cd ${hversion} || exit 1

makecheckenv=
if [ X"${testenvironment}" != X ] ; then
    makecheckenv="${makecheckenv} TESTS_ENVIRONMENT=\"${testenvironment}\""
fi

mkdir socket_wrapper_dir
SOCKET_WRAPPER_DIR=`pwd`/socket_wrapper_dir
export SOCKET_WRAPPER_DIR

echo "Configuring and building ($hversion)"
echo "./configure --enable-socket-wrapper ${confflags}" > ab.txt
./configure --enable-socket-wrapper ${confflags} >> ab.txt 2>&1
if [ $? != 0 ] ; then
    echo Configure failed
    status=${status:-configure}
fi
echo make all >> ab.txt
make all >> ab.txt 2>&1
if [ $? != 0 ] ; then
    echo Make all failed
    status=${status:-make all}
fi
echo make check >> ab.txt
eval env $makecheckenv make check >> ab.txt 2>&1
if [ $? != 0 ] ; then
    echo Make check failed
    status=${status:-make check}
fi

if [ "$distcheck" = yes ] ; then
    echo make distcheck >> ab.txt
    if [ $? != 0 ] ; then
        echo Make check failed
        status=${status:-make distcheck}
    fi
fi

status=${status:-ok}

echo "done: ${status}"

if [ "X${resultdir}" != X ] ; then
	cp ab.txt "${resultdir}/ab-${hversion}-${hostname}-${date}.txt"
fi

if [ "X${noemail}" = X ] ; then
	cat > email-header <<EOF
From: ${USER:-unknown-user}@${hostname}
To: <heimdal-build-log@it.su.se>
Subject: heimdal-build-log SPAM COOKIE
X-heimdal-build: kaka-till-love

Script-version: ${version}
Heimdal-version: ${hversion}
Machine: `uname -a`
Status: $status
EOF

	if [ "X$passhrase" != X ] ; then
		cat >> email-header <<EOF
autobuild-passphrase: ${passhrase}
EOF
	fi
		cat >> email-header <<EOF
------L-O-G------------------------------------
EOF

	cat email-header ab.txt | sendmail "${email}"
fi

cd ..
if [ X"$keeptree" != Xyes ] ; then
    rm -rf ${hversion}
fi
rm -f ${hfile}

exit 0
