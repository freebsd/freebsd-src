divert(-1)
#
# Copyright (c) 1999-2001, 2006 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
#  Definitions for Makefile construction for sendmail
#
#	$Id: defines.m4,v 8.48 2012/01/21 00:07:08 ashish Exp $
#
# temporary hack: if confREQUIRE_LIBSM is set then also set confREQUIRE_SM_OS_H
ifdef(`confREQUIRE_LIBSM',`
ifdef(`confREQUIRE_SM_OS_H',`', `define(`confREQUIRE_SM_OS_H', `1')')')
#
divert(0)dnl

# C compiler
CC=	confCC
CCOPTS= ifdef(`confCCOPTS', `confCCOPTS', ` ') ifdef(`confMT', ifdef(`confMTCCOPTS', `confMTCCOPTS', `'), `')

# Linker for executables
CCLINK = ifdef(`confCCLINK', `confCCLINK', `confCC')
# Linker for libraries
LD=	ifdef(`confLD', `confLD', `confCC')
LDOPTS=	ifdef(`confLDOPTS', `confLDOPTS') ifdef(`confMT', ifdef(`confMTLDOPTS', `confMTLDOPTS', `'), `')
LDOPTS_SO= ${LDOPTS} ifdef(`confLDOPTS_SO', `confLDOPTS_SO', `-shared')

# Shell
SHELL=	confSHELL

# use O=-O (usual) or O=-g (debugging)
O=	ifdef(`confOPTIMIZE', `confOPTIMIZE', `-O')

# Object archiver
AR=     ifdef(`confAR', `confAR', `ar')
AROPTS=	ifdef(`confAROPTS', `confAROPTS', `crv')

# Remove command
RM=     ifdef(`confRM', `confRM', `rm')
RMOPTS=	ifdef(`confRMOPTS', `confRMOPTS', `-f')

# Link command
LN=     ifdef(`confLN', `confLN', `ln')
LNOPTS=	ifdef(`confLNOPTS', `confLNOPTS', `-f -s')

# Ranlib (or echo)
RANLIB= ifdef(`confRANLIB', `confRANLIB', `ranlib')
RANLIBOPTS=	ifdef(`confRANLIBOPTS', `confRANLIBOPTS', `')

# Object stripper
STRIP=	ifdef(`confSTRIP', `confSTRIP', `strip')
STRIPOPTS=	ifdef(`confSTRIPOPTS', `confSTRIPOPTS', `')

# environment definitions (e.g., -D_AIX3)
ENVDEF= ifdef(`confENVDEF', `confENVDEF') ifdef(`conf_'bldCURRENT_PRD`_ENVDEF', `conf_'bldCURRENT_PRD`_ENVDEF')

# location of the source directory
SRCDIR=	ifdef(`confSRCDIR', `confSRCDIR', `_SRC_PATH_')

# inc`'lude directories
INCDIRS= confINCDIRS

# library directories
LIBDIRS=confLIBDIRS

# Additional libs needed
LIBADD= ifdef(`conf_'bldCURRENT_PRD`_LIBS', `conf_'bldCURRENT_PRD`_LIBS')

# libraries required on your system
LIBS= ${LIBADD} ifdef(`confLIBS', `confLIBS') ifdef(`conf_'bldCURRENT_PRD`_LIB_POST', `conf_'bldCURRENT_PRD`_LIB_POST')

# location of sendmail binary (usually /usr/sbin or /usr/lib)
BINDIR=	ifdef(`confMBINDIR', `confMBINDIR', `/usr/sbin')

# location of "user" binaries (usually /usr/bin or /usr/ucb)
UBINDIR=ifdef(`confUBINDIR', `confUBINDIR', `/usr/bin')

# location of "root" binaries (usually /usr/sbin or /usr/etc)
SBINDIR=ifdef(`confSBINDIR', `confSBINDIR', `/usr/sbin')

# location of "root" binaries (usually /usr/sbin or /usr/etc)
MBINDIR=ifdef(`confMBINDIR', `confMBINDIR', `/usr/sbin')

# location of "libexec" binaries (usually /usr/libexec or /usr/etc)
EBINDIR=ifdef(`confEBINDIR', `confEBINDIR', `/usr/libexec')

# where to install inc`'lude files (usually /usr/inc`'lude)
INCLUDEDIR=ifdef(`confINCLUDEDIR', `confINCLUDEDIR', `/usr/inc`'lude')

# where to install library files (usually /usr/lib)
LIBDIR=ifdef(`confLIBDIR', `confLIBDIR', `/usr/lib')

# additional .c files needed
SRCADD= ifdef(`confSRCADD', `confSRCADD')

ifdef(`conf_'bldCURRENT_PRD`_SRCADD', `bldLIST_PUSH_ITEM(`bldSOURCES', `conf_'bldCURRENT_PRD`_SRCADD')')

# additional .o files needed
OBJADD=	ifdef(`confOBJADD', `confOBJADD')
bldCURRENT_PRODUCT`OBJADD'= ifdef(`conf_'bldCURRENT_PRD`_OBJADD', `conf_'bldCURRENT_PRD`_OBJADD') ifdef(`confLIBADD', `bldADD_EXTENSIONS(`a', confLIBADD)', `')

# copy files
CP= ifdef(`confCOPY', `confCOPY', `cp')

# In some places windows wants nmake where unix would just want make
NMAKE=ifdef(`confNMAKE', `confNMAKE', `${MAKE}')

###################  end of user configuration flags  ######################

BUILDBIN=confBUILDBIN
COPTS=	-I. ${INCDIRS} ${ENVDEF} ${CCOPTS}
CFLAGS=	$O ${COPTS} ifdef(`confMT', ifdef(`confMTCFLAGS', `confMTCFLAGS -DXP_MT', `-DXP_MT'), `')


BEFORE=	confBEFORE ifdef(`confREQUIRE_SM_OS_H',`sm_os.h')

LINKS=ifdef(`bldLINK_SOURCES', `bldLINK_SOURCES', `')

bldCURRENT_PRODUCT`SRCS'= bldSOURCES ${SRCADD}
bldCURRENT_PRODUCT`OBJS'= bldSUBST_EXTENSIONS(`o', bldSOURCES) ifdef(`bldLINK_SOURCES', `bldSUBST_EXTENSIONS(`o', bldLINK_SOURCES)') ${OBJADD} ${bldCURRENT_PRODUCT`OBJADD'}
bldCURRENT_PRODUCT`SMDEPLIBS'= ifdef(`bldSMDEPLIBS', `bldSMDEPLIBS', `')
bldCURRENT_PRODUCT`TARGET_LINKS'= ifdef(`bldTARGET_LINKS', `bldTARGET_LINKS', `')

bldPUSH_ALL_SRCS(bldCURRENT_PRODUCT`SRCS')dnl

ifdef(`bldBIN_TYPE', , `define(`bldBIN_TYPE', `U')')dnl
ifdef(`bldINSTALL_DIR', , `define(`bldINSTALL_DIR', `U')')dnl

NROFF=	ifdef(`confNROFF', `confNROFF', `groff -Tascii')
MANDOC=	ifdef(`confMANDOC', `confMANDOC', `-man')

INSTALL=ifdef(`confINSTALL', `confINSTALL', `install')

# User binary ownership/permissions
UBINOWN=ifdef(`confUBINOWN', `confUBINOWN', `bin')
UBINGRP=ifdef(`confUBINGRP', `confUBINGRP', `bin')
UBINMODE=ifdef(`confUBINMODE', `confUBINMODE', `555')

# Setuid binary ownership/permissions
SBINOWN=ifdef(`confSBINOWN', `confSBINOWN', `root')
SBINGRP=ifdef(`confSBINGRP', `confSBINGRP', `bin')
SBINMODE=ifdef(`confSBINMODE', `confSBINMODE', `4555')

# Setgid binary ownership/permissions
GBINOWN=ifdef(`confGBINOWN', `confGBINOWN', `root')
GBINGRP=ifdef(`confGBINGRP', `confGBINGRP', `smmsp')
GBINMODE=ifdef(`confGBINMODE', `confGBINMODE', `2555')

# owner of MSP queue
MSPQOWN=ifdef(`confMSPQOWN', `confMSPQOWN', `smmsp')

# MTA binary ownership/permissions
MBINOWN=ifdef(`confMBINOWN', `confMBINOWN', `root')
MBINGRP=ifdef(`confMBINGRP', `confMBINGRP', `bin')
MBINMODE=ifdef(`confMBINMODE', `confMBINMODE', `550')

# Library ownership/permissions
LIBOWN=ifdef(`confLIBOWN', `confLIBOWN', `root')
LIBGRP=ifdef(`confLIBGRP', `confLIBGRP', `bin')
LIBMODE=ifdef(`confLIBMODE', `confLIBMODE', `0444')

# Include file ownership/permissions
INCOWN=ifdef(`confINCOWN', `confINCOWN', `root')
INCGRP=ifdef(`confINCGRP', `confINCGRP', `bin')
INCMODE=ifdef(`confINCMODE', `confINCMODE', `0444')
