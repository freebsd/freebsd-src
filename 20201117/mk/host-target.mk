# RCSid:
#	$Id: host-target.mk,v 1.13 2020/08/05 23:32:08 sjg Exp $

# Host platform information; may be overridden
.if !defined(_HOST_OSNAME)
_HOST_OSNAME !=	uname -s
.export _HOST_OSNAME
.endif
.if !defined(_HOST_OSREL)
_HOST_OSREL  !=	uname -r
.export _HOST_OSREL
.endif
.if !defined(_HOST_MACHINE)
_HOST_MACHINE != uname -m
.export _HOST_MACHINE
.endif
.if !defined(_HOST_ARCH)
# for NetBSD prefer $MACHINE (amd64 rather than x86_64)
.if ${_HOST_OSNAME:NDarwin:NNetBSD} == ""
_HOST_ARCH := ${_HOST_MACHINE}
.else
_HOST_ARCH != uname -p 2> /dev/null || uname -m
# uname -p may produce garbage on linux
.if ${_HOST_ARCH:[\#]} > 1 || ${_HOST_ARCH:Nunknown} == ""
_HOST_ARCH := ${_HOST_MACHINE}
.endif
.endif
.export _HOST_ARCH
.endif
.if !defined(HOST_MACHINE)
HOST_MACHINE := ${_HOST_MACHINE}
.export HOST_MACHINE
.endif

HOST_OSMAJOR := ${_HOST_OSREL:C/[^0-9].*//}
HOST_OSTYPE  :=	${_HOST_OSNAME:S,/,,g}-${_HOST_OSREL:C/\([^\)]*\)//}-${_HOST_ARCH}
HOST_OS      :=	${_HOST_OSNAME}
host_os      :=	${_HOST_OSNAME:tl}
HOST_TARGET  := ${host_os:S,/,,g}${HOST_OSMAJOR}-${_HOST_ARCH}
# sometimes we want HOST_TARGET32
MACHINE32.amd64 = i386
MACHINE32.x86_64 = i386
_HOST_ARCH32 := ${MACHINE32.${_HOST_ARCH}:U${_HOST_ARCH:S,64$,,}}
HOST_TARGET32 := ${host_os:S,/,,g}${HOST_OSMAJOR}-${_HOST_ARCH32}

# tr is insanely non-portable, accommodate the lowest common denominator
TR ?= tr
toLower = ${TR} 'ABCDEFGHIJKLMNOPQRSTUVWXYZ' 'abcdefghijklmnopqrstuvwxyz'
toUpper = ${TR} 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'
