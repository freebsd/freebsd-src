# Microsoft Visual C++ generated build script - Do not modify
# $FreeBSD$

PROJ = FBSDBOOT
DEBUG = 0
PROGTYPE = 6
CALLER = 
ARGS = 
DLLS = 
D_RCDEFINES = -d_DEBUG
R_RCDEFINES = -dNDEBUG
ORIGIN = MSVC
ORIGIN_VER = 1.00
PROJPATH = C:\SRC\FBSDBOOT\
USEMFC = 0
CC = cl
CPP = cl
CXX = cl
CCREATEPCHFLAG = 
CPPCREATEPCHFLAG = 
CUSEPCHFLAG = 
CPPUSEPCHFLAG = 
FIRSTC = FBSDBOOT.C  
FIRSTCPP =             
RC = rc
CFLAGS_D_DEXE = /nologo /Gs /G3 /Zp1 /W3 /Zi /AL /Oi /D "_DEBUG" /D "i386" /D "_DOS" /D "__i386__" /Fc /Fd"FBSDBOOT.PDB"
CFLAGS_R_DEXE = /nologo /Gs /G3 /Zp1 /W3 /AL /Ox /D "NDEBUG" /D "i386" /D "_DOS" /D "__i386__" /D "DO_BAD144" 
LFLAGS_D_DEXE = /NOLOGO /NOI /STACK:6000 /ONERROR:NOEXE /CO /MAP /LINE 
LFLAGS_R_DEXE = /NOLOGO /NOI /STACK:5120 /ONERROR:NOEXE 
LIBS_D_DEXE = oldnames llibce 
LIBS_R_DEXE = oldnames llibce 
RCFLAGS = /nologo
RESFLAGS = /nologo
RUNFLAGS = 
OBJS_EXT = 
LIBS_EXT = 
!if "$(DEBUG)" == "1"
CFLAGS = $(CFLAGS_D_DEXE)
LFLAGS = $(LFLAGS_D_DEXE)
LIBS = $(LIBS_D_DEXE)
MAPFILE = nul
RCDEFINES = $(D_RCDEFINES)
!else
CFLAGS = $(CFLAGS_R_DEXE)
LFLAGS = $(LFLAGS_R_DEXE)
LIBS = $(LIBS_R_DEXE)
MAPFILE = nul
RCDEFINES = $(R_RCDEFINES)
!endif
!if [if exist MSVC.BND del MSVC.BND]
!endif
SBRS = FBSDBOOT.SBR \
		PROTMOD.SBR \
		BOOT.SBR \
		DISK.SBR \
		SYS.SBR \
		DOSBOOT.SBR


FBSDBOOT_DEP = c:\src\fbsdboot\reboot.h \
	c:\src\fbsdboot\boot.h \
	c:\src\fbsdboot\param.h \
	c:\src\fbsdboot\sysparam.h \
	c:\src\fbsdboot\syslimit.h \
	c:\src\fbsdboot\quota.h \
	c:\src\fbsdboot\cdefs.h \
	c:\src\fbsdboot\fs.h \
	c:\src\fbsdboot\inode.h \
	c:\src\fbsdboot\dinode.h \
	c:\src\fbsdboot\bootinfo.h \
	c:\src\fbsdboot\dosboot.h \
	c:\src\fbsdboot\protmod.h


PROTMOD_DEP = c:\src\fbsdboot\boot.h \
	c:\src\fbsdboot\param.h \
	c:\src\fbsdboot\sysparam.h \
	c:\src\fbsdboot\syslimit.h \
	c:\src\fbsdboot\quota.h \
	c:\src\fbsdboot\cdefs.h \
	c:\src\fbsdboot\fs.h \
	c:\src\fbsdboot\inode.h \
	c:\src\fbsdboot\dinode.h \
	c:\src\fbsdboot\bootinfo.h \
	c:\src\fbsdboot\protmod.h


BOOT_DEP = c:\src\fbsdboot\bootinfo.h \
	c:\src\fbsdboot\protmod.h \
	c:\src\fbsdboot\param.h \
	c:\src\fbsdboot\sysparam.h \
	c:\src\fbsdboot\syslimit.h \
	c:\src\fbsdboot\boot.h \
	c:\src\fbsdboot\quota.h \
	c:\src\fbsdboot\cdefs.h \
	c:\src\fbsdboot\fs.h \
	c:\src\fbsdboot\inode.h \
	c:\src\fbsdboot\dinode.h \
	c:\src\fbsdboot\reboot.h \
	c:\src\fbsdboot\exec.h \
	c:\src\fbsdboot\mexec.h \
	c:\src\fbsdboot\imgact.h


DISK_DEP = c:\src\fbsdboot\boot.h \
	c:\src\fbsdboot\param.h \
	c:\src\fbsdboot\sysparam.h \
	c:\src\fbsdboot\syslimit.h \
	c:\src\fbsdboot\quota.h \
	c:\src\fbsdboot\cdefs.h \
	c:\src\fbsdboot\fs.h \
	c:\src\fbsdboot\inode.h \
	c:\src\fbsdboot\dinode.h \
	c:\src\fbsdboot\dkbad.h \
	c:\src\fbsdboot\disklabe.h


SYS_DEP = c:\src\fbsdboot\protmod.h \
	c:\src\fbsdboot\boot.h \
	c:\src\fbsdboot\param.h \
	c:\src\fbsdboot\sysparam.h \
	c:\src\fbsdboot\syslimit.h \
	c:\src\fbsdboot\quota.h \
	c:\src\fbsdboot\cdefs.h \
	c:\src\fbsdboot\fs.h \
	c:\src\fbsdboot\inode.h \
	c:\src\fbsdboot\dinode.h \
	c:\src\fbsdboot\dir.h \
	c:\src\fbsdboot\dirent.h


DOSBOOT_DEP = c:\src\fbsdboot\protmod.h \
	c:\src\fbsdboot\param.h \
	c:\src\fbsdboot\sysparam.h \
	c:\src\fbsdboot\syslimit.h \
	c:\src\fbsdboot\boot.h \
	c:\src\fbsdboot\quota.h \
	c:\src\fbsdboot\cdefs.h \
	c:\src\fbsdboot\fs.h \
	c:\src\fbsdboot\inode.h \
	c:\src\fbsdboot\dinode.h \
	c:\src\fbsdboot\bootinfo.h \
	c:\src\fbsdboot\reboot.h \
	c:\src\fbsdboot\exec.h \
	c:\src\fbsdboot\mexec.h \
	c:\src\fbsdboot\imgact.h


all:	$(PROJ).EXE

FBSDBOOT.OBJ:	FBSDBOOT.C $(FBSDBOOT_DEP)
	$(CC) $(CFLAGS) $(CCREATEPCHFLAG) /c FBSDBOOT.C

PROTMOD.OBJ:	PROTMOD.C $(PROTMOD_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c PROTMOD.C

BOOT.OBJ:	BOOT.C $(BOOT_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c BOOT.C

DISK.OBJ:	DISK.C $(DISK_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c DISK.C

SYS.OBJ:	SYS.C $(SYS_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c SYS.C

DOSBOOT.OBJ:	DOSBOOT.C $(DOSBOOT_DEP)
	$(CC) $(CFLAGS) $(CUSEPCHFLAG) /c DOSBOOT.C

$(PROJ).EXE::	FBSDBOOT.OBJ PROTMOD.OBJ BOOT.OBJ DISK.OBJ SYS.OBJ DOSBOOT.OBJ $(OBJS_EXT) $(DEFFILE)
	echo >NUL @<<$(PROJ).CRF
FBSDBOOT.OBJ +
PROTMOD.OBJ +
BOOT.OBJ +
DISK.OBJ +
SYS.OBJ +
DOSBOOT.OBJ +
$(OBJS_EXT)
$(PROJ).EXE
$(MAPFILE)
c:\msvc\lib\+
c:\msvc\mfc\lib\+
$(LIBS)
$(DEFFILE);
<<
	link $(LFLAGS) @$(PROJ).CRF

run: $(PROJ).EXE
	$(PROJ) $(RUNFLAGS)


$(PROJ).BSC: $(SBRS)
	bscmake @<<
/o$@ $(SBRS)
<<
