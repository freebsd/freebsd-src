# Microsoft Developer Studio Generated NMAKE File, Based on roken.dsp
!IF "$(CFG)" == ""
CFG=roken - Win32 Release
!MESSAGE No configuration specified. Defaulting to roken - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "roken - Win32 Release" && "$(CFG)" != "roken - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "roken.mak" CFG="roken - Win32 Release"
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

CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "roken - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\roken.dll"

!ELSE 

ALL : "$(OUTDIR)\roken.dll"

!ENDIF 

CLEAN : 
	-@erase "$(INTDIR)\base64.obj"
	-@erase "$(INTDIR)\concat.obj"
	-@erase "$(INTDIR)\gettimeofday.obj"
	-@erase "$(INTDIR)\getuid.obj"
	-@erase "$(INTDIR)\resolve.obj"
	-@erase "$(INTDIR)\roken.res"
	-@erase "$(INTDIR)\snprintf.obj"
	-@erase "$(INTDIR)\strcasecmp.obj"
	-@erase "$(INTDIR)\strtok_r.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\roken.dll"
	-@erase "$(OUTDIR)\roken.exp"
	-@erase "$(OUTDIR)\roken.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /GX /O2 /I "..\krb" /I "..\des" /I "..\..\include" /I\
 "..\..\include\win32" /I "." /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "HAVE_CONFIG_H" /Fp"$(INTDIR)\roken.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\"\
 /FD /c 
CPP_OBJS=.\Release/
CPP_SBRS=.
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\roken.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\roken.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo\
 /base:"0x68e7780" /subsystem:windows /dll /incremental:no\
 /pdb:"$(OUTDIR)\roken.pdb" /machine:I386 /def:".\roken.def"\
 /out:"$(OUTDIR)\roken.dll" /implib:"$(OUTDIR)\roken.lib" 
DEF_FILE= \
	".\roken.def"
LINK32_OBJS= \
	"$(INTDIR)\base64.obj" \
	"$(INTDIR)\concat.obj" \
	"$(INTDIR)\gettimeofday.obj" \
	"$(INTDIR)\getuid.obj" \
	"$(INTDIR)\resolve.obj" \
	"$(INTDIR)\roken.res" \
	"$(INTDIR)\snprintf.obj" \
	"$(INTDIR)\strcasecmp.obj" \
	"$(INTDIR)\strtok_r.obj"

"$(OUTDIR)\roken.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "roken - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\roken.dll"

!ELSE 

ALL : "$(OUTDIR)\roken.dll"

!ENDIF 

CLEAN : 
	-@erase "$(INTDIR)\base64.obj"
	-@erase "$(INTDIR)\concat.obj"
	-@erase "$(INTDIR)\gettimeofday.obj"
	-@erase "$(INTDIR)\getuid.obj"
	-@erase "$(INTDIR)\resolve.obj"
	-@erase "$(INTDIR)\roken.res"
	-@erase "$(INTDIR)\snprintf.obj"
	-@erase "$(INTDIR)\strcasecmp.obj"
	-@erase "$(INTDIR)\strtok_r.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\vc50.pdb"
	-@erase "$(OUTDIR)\roken.dll"
	-@erase "$(OUTDIR)\roken.exp"
	-@erase "$(OUTDIR)\roken.ilk"
	-@erase "$(OUTDIR)\roken.lib"
	-@erase "$(OUTDIR)\roken.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MDd /Gm /GX /Zi /Od /I "..\krb" /I "..\des" /I\
 "..\..\include" /I "..\..\include\win32" /I "." /D "_DEBUG" /D "WIN32" /D\
 "_WINDOWS" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\roken.pch" /YX /Fo"$(INTDIR)\\"\
 /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\roken.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\roken.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo\
 /subsystem:windows /dll /incremental:yes /pdb:"$(OUTDIR)\roken.pdb" /debug\
 /machine:I386 /def:".\roken.def" /out:"$(OUTDIR)\roken.dll"\
 /implib:"$(OUTDIR)\roken.lib" 
LINK32_OBJS= \
	"$(INTDIR)\base64.obj" \
	"$(INTDIR)\concat.obj" \
	"$(INTDIR)\gettimeofday.obj" \
	"$(INTDIR)\getuid.obj" \
	"$(INTDIR)\resolve.obj" \
	"$(INTDIR)\roken.res" \
	"$(INTDIR)\snprintf.obj" \
	"$(INTDIR)\strcasecmp.obj" \
	"$(INTDIR)\strtok_r.obj"

"$(OUTDIR)\roken.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<


!IF "$(CFG)" == "roken - Win32 Release" || "$(CFG)" == "roken - Win32 Debug"
SOURCE=.\base64.c
DEP_CPP_BASE6=\
	"..\..\include\win32\config.h"\
	".\base64.h"\


"$(INTDIR)\base64.obj" : $(SOURCE) $(DEP_CPP_BASE6) "$(INTDIR)"


SOURCE=.\concat.c
DEP_CPP_CONCA=\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\roken.h"\
	".\err.h"\
	".\roken-common.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\concat.obj" : $(SOURCE) $(DEP_CPP_CONCA) "$(INTDIR)"


SOURCE=.\gettimeofday.c
DEP_CPP_GETTI=\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\roken.h"\
	".\err.h"\
	".\roken-common.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\gettimeofday.obj" : $(SOURCE) $(DEP_CPP_GETTI) "$(INTDIR)"


SOURCE=.\getuid.c
DEP_CPP_GETUI=\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\roken.h"\
	".\err.h"\
	".\roken-common.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\getuid.obj" : $(SOURCE) $(DEP_CPP_GETUI) "$(INTDIR)"


SOURCE=.\resolve.c
DEP_CPP_RESOL=\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\roken.h"\
	".\err.h"\
	".\resolve.h"\
	".\roken-common.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\resolve.obj" : $(SOURCE) $(DEP_CPP_RESOL) "$(INTDIR)"


SOURCE=.\snprintf.c
DEP_CPP_SNPRI=\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\roken.h"\
	".\err.h"\
	".\roken-common.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\snprintf.obj" : $(SOURCE) $(DEP_CPP_SNPRI) "$(INTDIR)"


SOURCE=.\strcasecmp.c
DEP_CPP_STRCA=\
	"..\..\include\win32\config.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\strcasecmp.obj" : $(SOURCE) $(DEP_CPP_STRCA) "$(INTDIR)"


SOURCE=.\strtok_r.c
DEP_CPP_STRTO=\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\roken.h"\
	".\err.h"\
	".\roken-common.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

"$(INTDIR)\strtok_r.obj" : $(SOURCE) $(DEP_CPP_STRTO) "$(INTDIR)"


SOURCE=.\roken.rc

"$(INTDIR)\roken.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)
	


!ENDIF 

