# Microsoft Developer Studio Generated NMAKE File, Format Version 4.10
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

!IF "$(CFG)" == ""
CFG=roken - Win32 Debug
!MESSAGE No configuration specified.  Defaulting to roken - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "roken - Win32 Release" && "$(CFG)" != "roken - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "roken.mak" CFG="roken - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "roken - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "roken - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 
################################################################################
# Begin Project
# PROP Target_Last_Scanned "roken - Win32 Debug"
RSC=rc.exe
MTL=mktyplib.exe
CPP=cl.exe

!IF  "$(CFG)" == "roken - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
OUTDIR=.\Release
INTDIR=.\Release

ALL : ".\Release\roken.dll"

CLEAN : 
	-@erase ".\Release\gettimeofday.obj"
	-@erase ".\Release\roken.dll"
	-@erase ".\Release\roken.exp"
	-@erase ".\Release\roken.lib"
	-@erase ".\Release\strcasecmp.obj"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\krb" /I "..\des" /I "..\..\include" /I "..\..\include\win32" /I "." /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "HAVE_CONFIG_H" /YX /c
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\krb" /I "..\des" /I "..\..\include" /I\
 "..\..\include\win32" /I "." /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "HAVE_CONFIG_H" /Fp"$(INTDIR)/roken.pch" /YX /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\Release/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
MTL_PROJ=/nologo /D "NDEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/roken.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib /nologo /subsystem:windows /dll /incremental:no\
 /pdb:"$(OUTDIR)/roken.pdb" /machine:I386 /def:".\roken.def"\
 /out:"$(OUTDIR)/roken.dll" /implib:"$(OUTDIR)/roken.lib" 
DEF_FILE= \
	".\roken.def"
LINK32_OBJS= \
	".\Release\gettimeofday.obj" \
	".\Release\strcasecmp.obj"

".\Release\roken.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "roken - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
OUTDIR=.\Debug
INTDIR=.\Debug

ALL : ".\Debug\roken.dll"

CLEAN : 
	-@erase ".\Debug\gettimeofday.obj"
	-@erase ".\Debug\roken.dll"
	-@erase ".\Debug\roken.exp"
	-@erase ".\Debug\roken.ilk"
	-@erase ".\Debug\roken.lib"
	-@erase ".\Debug\roken.pdb"
	-@erase ".\Debug\strcasecmp.obj"
	-@erase ".\Debug\vc40.idb"
	-@erase ".\Debug\vc40.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\krb" /I "..\des" /I "..\..\include" /I "..\..\include\win32" /I "." /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "HAVE_CONFIG_H" /YX /c
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /Zi /Od /I "..\krb" /I "..\des" /I\
 "..\..\include" /I "..\..\include\win32" /I "." /D "_DEBUG" /D "WIN32" /D\
 "_WINDOWS" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)/roken.pch" /YX /Fo"$(INTDIR)/"\
 /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
MTL_PROJ=/nologo /D "_DEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/roken.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib /nologo /subsystem:windows /dll /incremental:yes\
 /pdb:"$(OUTDIR)/roken.pdb" /debug /machine:I386 /def:".\roken.def"\
 /out:"$(OUTDIR)/roken.dll" /implib:"$(OUTDIR)/roken.lib" 
DEF_FILE= \
	".\roken.def"
LINK32_OBJS= \
	".\Debug\gettimeofday.obj" \
	".\Debug\strcasecmp.obj"

".\Debug\roken.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.c{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

################################################################################
# Begin Target

# Name "roken - Win32 Release"
# Name "roken - Win32 Debug"

!IF  "$(CFG)" == "roken - Win32 Release"

!ELSEIF  "$(CFG)" == "roken - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=\TEMP\jimpa3\lib\krb\gettimeofday.c
DEP_CPP_GETTI=\
	"..\..\include\protos.h"\
	"..\..\include\sys/bitypes.h"\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	"..\des\des.h"\
	"..\krb\krb.h"\
	"..\krb\krb_locl.h"\
	"..\krb\prot.h"\
	"..\krb\resolve.h"\
	".\roken.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

!IF  "$(CFG)" == "roken - Win32 Release"


".\Release\gettimeofday.obj" : $(SOURCE) $(DEP_CPP_GETTI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "roken - Win32 Debug"


".\Debug\gettimeofday.obj" : $(SOURCE) $(DEP_CPP_GETTI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\roken.def

!IF  "$(CFG)" == "roken - Win32 Release"

!ELSEIF  "$(CFG)" == "roken - Win32 Debug"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\strcasecmp.c
DEP_CPP_STRCA=\
	"..\..\include\sys/cdefs.h"\
	"..\..\include\win32\config.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

!IF  "$(CFG)" == "roken - Win32 Release"


".\Release\strcasecmp.obj" : $(SOURCE) $(DEP_CPP_STRCA) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "roken - Win32 Debug"


".\Debug\strcasecmp.obj" : $(SOURCE) $(DEP_CPP_STRCA) "$(INTDIR)"


!ENDIF 

# End Source File
# End Target
# End Project
################################################################################
