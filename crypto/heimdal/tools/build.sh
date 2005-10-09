#!/bin/sh
#
# Build many combinations of kth-krb/heimdal/openssl
#
# $Id: build.sh,v 1.8 2003/04/17 12:55:02 lha Exp $

opt_n= #:
make_f= #-j

heimdal_versions="0.5.2 0.6pre4"
krb4_versions="1.2.2"
openssl_versions="0.9.6i 0.9.7a 0.9.7b"

make_check_version=".*heimdal-0.6.*"

# 0.5 dont eat 0.9.7
dont_build="openssl-0.9.7.*heimdal-0.5.*"
# 1.2 dont eat 0.9.7
dont_build="openssl-0.9.7.*krb4-1.2.* ${dont_build}"
#yacc problems
dont_build="openssl-0.9.6.*heimdal-0.5.*osf4.* ${dont_build}"
#local openssl 09.7 and broken kuser/Makefile.am
dont_build="openssl-0.9.6.*heimdal-0.5.*freebsd4.8.* ${dont_build}" 
failed=

# Allow override
for a in $HOME . /etc ; do 
    [ -f $a/.heimdal-build ] && . $a/.heimdal-build
done

targetdir=${targetdir:-/scratch/heimdal-test}
logfile="${targetdir}/buildlog"

distdirs="${distdirs} /afs/su.se/home/l/h/lha/Public/openssl"
distdirs="${distdirs} /afs/pdc.kth.se/public/ftp/pub/heimdal/src"
distdirs="${distdirs} /afs/pdc.kth.se/public/ftp/pub/heimdal/src/snapshots"
distdirs="${distdirs} /afs/pdc.kth.se/public/ftp/pub/krb/src"


logprint () {
    d=`date '+%Y-%m-%d %H:%M:%S'`
    echo "${d}: $*"
    echo "${d}: --- $*" >> ${logfile}
}

logerror () {
    echo "$*"
    exit 1
}

find_unzip_prog () {
    unzip_prog=
    oldIFS="$IFS"
    IFS=:
    set -- $PATH
    IFS="$oldIFS"
    for a in $* ; do
	if [ -x $a/gzip ] ; then
	    unzip_prog="$a/gzip -dc"
	    break
	elif [ -x $a/gunzip ] ; then
	    unzip_prog="$a/gunzip -c"
	    break
	fi
    done
    [ "$unzip_prog" = "" ] && logerror failed to find unzip program
}

find_canon_name () {
    canon_name=
    for a in ${distdirs} ; do
	if [ -f $a/config.guess ] ; then
	    canon_name=`$a/config.guess`
	fi
	if [ "${canon_name}" != "" ] ; then
	    break
	fi
    done
    [ "${canon_name}" = "" ] && logerror "cant find config.guess"
}

do_check_p () {
    eval check_var=\$"$1"
    for a in ${check_var} ; do
	expr "$2${canon_name}" : "${a}" > /dev/null 2>&1 && return 1
    done
    return 0
}

unpack_tar () {
    for a in ${distdirs} ; do
	if [ -f $a/$1 ] ; then
	    ${opt_n} ${unzip_prog} ${a}/$1 | ${opt_n} tar xf -
	    return 0
	fi
    done
    logerror "did not find $1"
}

build () {
    real_ver=$1
    prog=$2
    ver=$3
    confprog=$4
    checks=$5
    pv=${prog}-${ver}
    mkdir tmp || logerror "failed to build tmpdir"
    cd tmp || logerror "failed to change dir to tmpdir"
    do_check_p dont_build ${real_ver} || \
	{ cd .. ; rmdir tmp ; logprint "not building $1" && return 0 ; }
    cd .. || logerror "failed to change back from tmpdir"
    rmdir tmp || logerror "failed to remove tmpdir"
    logprint "preparing for ${pv}"
    ${opt_n} rm -rf ${targetdir}/${prog}-${ver}
    ${opt_n} rm -rf ${prog}-${ver}
    unpack_tar ${pv}.tar.gz
    ${opt_n} cd ${pv} || logerror directory ${pv} not there
    logprint "configure ${prog} ${ver} (${confprog})"
    ${opt_n} ./${confprog} \
	--prefix=${targetdir}/${pv} >> ${logfile} 2>&1 || \
	    { logprint failed to configure ${pv} ; return 1 ; }
    logprint "make ${prog} ${ver}"
    ${opt_n} make ${make_f} >> ${logfile} 2>&1 || \
	{ logprint failed to make ${pv} ; return 1 ; }
    ${opt_n} make install >> ${logfile} 2>&1 || \
	{ logprint failed to install ${pv} ; return 1 ; }
    do_check_p make_check_version ${real_ver} || \
    	{ ${opt_n} make check >> ${logfile} 2>&1 || return 1 ; }
    ${opt_n} cd ..
    [ "${checks}" != "" ] && ${opt_n} ${checks} >> ${logfile} 2>&1
    return 0
}

find_canon_name

logprint using host `hostname`
logprint `uname -a`
logprint canonical name ${canon_name}

logprint clearing logfile
> ${logfile}

find_unzip_prog

logprint using target dir ${targetdir}
mkdir -p ${targetdir}/src
cd ${targetdir}/src || exit 1
rm -rf heimdal* openssl* krb4*

logprint === building openssl versions
for vo in ${openssl_versions} ; do
    build openssl-${vo} openssl $vo config
done

wssl="--with-openssl=${targetdir}/openssl"
wssli="--with-openssl-include=${targetdir}/openssl"	#this is a hack for broken heimdal 0.5.x autoconf test
wossl="--without-openssl"
wk4c="--with-krb4-config=${targetdir}/krb4"
bk4c="/bin/krb4-config"
wok4="--without-krb4"

logprint === building heimdal w/o krb4 versions
for vo in ${openssl_versions} ; do
    for vh in ${heimdal_versions} ; do
	v="openssl-${vo}-heimdal-${vh}"
	build "${v}" \
	    heimdal ${vh} \
	    "configure ${wok4} ${wssl}-${vo} ${wssli}-${vo}/include" \
	    "${targetdir}/heimdal-${vh}/bin/krb5-config --libs | grep lcrypto" \ || \
	    { failed="${failed} ${v}" ; logprint ${v} failed ; }
    done
done

logprint === building krb4
for vo in ${openssl_versions} ; do
    for vk in ${krb4_versions} ; do
	v="openssl-${vo}-krb4-${vk}"
	build "${v}" \
	    krb4 ${vk} \
	    "configure ${wssl}-${vo}" \
	    "${targetdir}/krb4-${vk}/bin/krb4-config --libs | grep lcrypto"|| \
	    { failed="${failed} ${v}" ; logprint ${v} failed ; }
    done
done

logprint === building heimdal with krb4 versions
for vo in ${openssl_versions} ; do
    for vk in ${krb4_versions} ; do
	for vh in ${heimdal_versions} ; do
	    v="openssl-${vo}-krb4-${vk}-heimdal-${vh}"
	    build "${v}" \
		heimdal ${vh} \
		"configure ${wk4c}-${vk}${bk4c} ${wssl}-${vo} ${wssli}-${vo}/include" \
		"${targetdir}/heimdal-${vh}/bin/krb5-config --libs | grep lcrypto && ${targetdir}/heimdal-${vh}/bin/krb5-config --libs | grep krb4" \
		 || \
	    { failed="${failed} ${v}" ; logprint ${v} failed ; }
	done
    done
done

logprint === building heimdal without krb4 and openssl versions
for vh in ${heimdal_versions} ; do
    v="des-heimdal-${vh}"
    build "${v}" \
	heimdal ${vh} \
	"configure ${wok4} ${wossl}" || \
	{ failed="${failed} ${v}" ; logprint ${v} failed ; }
done

logprint all done
[ "${failed}" != "" ] && logprint "failed: ${failed}"
exit 0
