# Microsoft Developer Studio Generated NMAKE File, Based on KClient.dsp
!IF "$(CFG)" == ""
CFG=kclient - Win32 Release
!MESSAGE No configuration specified. Defaulting to kclient - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "kclient - Win32 Release" && "$(CFG)" !=\
 "kclient - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "KClient.mak" CFG="kclient - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "kclient - Win32 Release" (based on\
 "Win32 (x86) Dynamic-Link Library")
!MESSAGE "kclient - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "kclient - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\kclnt32.dll"

!ELSE 

ALL : "krb - Win32 Release" "$(OUTDIR)\kclnt32.dll"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"krb - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN : 
!ENDIF 
	-@erase "$(INTDIR)\KClient.obj"
	-@erase "$(INTDIR)\passwd_dialog.res"
	-@erase "$(INTDIR)\passwd_dlg.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\kclnt32.dll"
	-@erase "$(OUTDIR)\kclnt32.exp"
	-@erase "$(OUTDIR)\kclnt32.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "." /I "..\krb" /I "..\..\include" /I\
 "..\..\include\win32" /I "..\des" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "HAVE_CONFIG_H" /Fp"$(INTDIR)\KClient.pch" /YX /Fo"$(INTDIR)\\"\
 /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=.\Release/
CPP_SBRS=.

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

MTL=midl.exe
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC=rc.exe
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\passwd_dialog.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\KClient.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=..\krb\Release\krb.lib ..\des\Release\des.lib wsock32.lib\
 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib\
 shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /base:"0x1320000"\
 /subsystem:windows /dll /incremental:no /pdb:"$(OUTDIR)\kclnt32.pdb"\
 /machine:I386 /def:".\KClient.def" /out:"$(OUTDIR)\kclnt32.dll"\
 /implib:"$(OUTDIR)\kclnt32.lib" 
DEF_FILE= \
	".\KClient.def"
LINK32_OBJS= \
	"$(INTDIR)\KClient.obj" \
	"$(INTDIR)\passwd_dialog.res" \
	"$(INTDIR)\passwd_dlg.obj" \
	"..\krb\Release\krb.lib"

"$(OUTDIR)\kclnt32.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "kclient - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\kclnt32.dll"

!ELSE 

ALL : "krb - Win32 Debug" "$(OUTDIR)\kclnt32.dll"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"krb - Win32 DebugCLEAN" 
!ELSE 
CLEAN : 
!ENDIF 
	-@erase "$(INTDIR)\KClient.obj"
	-@erase "$(INTDIR)\passwd_dialog.res"
	-@erase "$(INTDIR)\passwd_dlg.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\vc50.pdb"
	-@erase "$(OUTDIR)\kclnt32.dll"
	-@erase "$(OUTDIR)\kclnt32.exp"
	-@erase "$(OUTDIR)\kclnt32.ilk"
	-@erase "$(OUTDIR)\kclnt32.lib"
	-@erase "$(OUTDIR)\kclnt32.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MDd /W3 /Gm /GX /Zi /Od /I "." /I "..\krb" /I "..\..\include"\
 /I "..\..\include\win32" /I "..\des" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D\
 "HAVE_CONFIG_H" /Fp"$(INTDIR)\KClient.pch" /YX /Fo"$(INTDIR)\\"\
 /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.

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

MTL=midl.exe
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC=rc.exe
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\passwd_dialog.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\KClient.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=..\krb\Debug\krb.lib ..\des\Debug\des.lib wsock32.lib kernel32.lib\
 user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib\
 ole32.lib oleaut32.lib uuid.lib /nologo /base:"0x1320000" /subsystem:windows\
 /dll /incremental:yes /pdb:"$(OUTDIR)\kclnt32.pdb" /debug /machine:I386\
 /def:".\KClient.def" /out:"$(OUTDIR)\kclnt32.dll"\
 /implib:"$(OUTDIR)\kclnt32.lib" 
DEF_FILE= \
	".\KClient.def"
LINK32_OBJS= \
	"$(INTDIR)\KClient.obj" \
	"$(INTDIR)\passwd_dialog.res" \
	"$(INTDIR)\passwd_dlg.obj" \
	"..\krb\Debug\krb.lib"

"$(OUTDIR)\kclnt32.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(CFG)" == "kclient - Win32 Release" || "$(CFG)" ==\
 "kclient - Win32 Debug"
SOURCE=.\KClient.c
DEP_CPP_KCLIE=\
	"..\..\include\win32\config.h"\
	"..\..\include\win32\ktypes.h"\
	"..\des\des.h"\
	"..\krb\krb-protos.h"\
	"..\krb\krb.h"\
	".\KClient.h"\
	".\passwd_dlg.h"\


"$(INTDIR)\KClient.obj" : $(SOURCE) $(DEP_CPP_KCLIE) "$(INTDIR)"


SOURCE=.\passwd_dialog.rc

"$(INTDIR)\passwd_dialog.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)


SOURCE=.\passwd_dlg.c
DEP_CPP_PASSW=\
	"..\..\include\win32\config.h"\
	".\passwd_dlg.h"\
	

"$(INTDIR)\passwd_dlg.obj" : $(SOURCE) $(DEP_CPP_PASSW) "$(INTDIR)"


!IF  "$(CFG)" == "kclient - Win32 Release"
	
"krb - Win32 Release" : 
   cd "\tmp\wirus-krb\krb4-pre-0.9.9\lib\krb"
   $(MAKE) /$(MAKEFLAGS) /F ".\krb.mak" CFG="krb - Win32 Release" 
   cd "..\kclient"

"krb - Win32 ReleaseCLEAN" : 
   cd "\tmp\wirus-krb\krb4-pre-0.9.9\lib\krb"
   $(MAKE) /$(MAKEFLAGS) CLEAN /F ".\krb.mak" CFG="krb - Win32 Release"\
 RECURSE=1 
   cd "..\kclient"

!ELSEIF  "$(CFG)" == "kclient - Win32 Debug"

"krb - Win32 Debug" : 
   cd "\tmp\wirus-krb\krb4-pre-0.9.9\lib\krb"
   $(MAKE) /$(MAKEFLAGS) /F ".\krb.mak" CFG="krb - Win32 Debug" 
   cd "..\kclient"

"krb - Win32 DebugCLEAN" : 
   cd "\tmp\wirus-krb\krb4-pre-0.9.9\lib\krb"
   $(MAKE) /$(MAKEFLAGS) CLEAN /F ".\krb.mak" CFG="krb - Win32 Debug" RECURSE=1\

   cd "..\kclient"

!ENDIF 


!ENDIF 

