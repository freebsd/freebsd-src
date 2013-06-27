# Microsoft Developer Studio Project File - Name="apr_ldap" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=apr_ldap - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "apr_ldap.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "apr_ldap.mak" CFG="apr_ldap - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "apr_ldap - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "apr_ldap - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "apr_ldap - x64 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "apr_ldap - x64 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "apr_ldap - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FD /c
# ADD CPP /nologo /MD /W3 /Zi /O2 /Oy- /I "../include" /I "../../apr/include" /I "../include/private" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /Fo"$(INTDIR)\" /Fd"$(INTDIR)\apr_ldap_src" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o /win32 "NUL"
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o /win32 "NUL"
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /fo"Release/apr_ldap-1.res" /d DLL_NAME="apr_ldap" /d "NDEBUG" /d "APU_VERSION_ONLY" /I "../include" /I "../../apr/include"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib advapi32.lib ws2_32.lib mswsock.lib wldap32.lib ole32.lib /nologo /base:"0x6EEB0000" /subsystem:windows /dll /incremental:no /debug /opt:ref
# ADD LINK32 kernel32.lib advapi32.lib ws2_32.lib mswsock.lib wldap32.lib ole32.lib /nologo /base:"0x6EEB0000" /subsystem:windows /dll /incremental:no /debug /out:"Release\apr_ldap-1.dll" /pdb:"Release\apr_ldap-1.pdb" /implib:"Release\apr_ldap-1.lib" /MACHINE:X86 /opt:ref
# Begin Special Build Tool
TargetPath=Release\apr_ldap-1.dll
SOURCE="$(InputPath)"
PostBuild_Desc=Embed .manifest
PostBuild_Cmds=if exist $(TargetPath).manifest mt.exe -manifest $(TargetPath).manifest -outputresource:$(TargetPath);2
# End Special Build Tool

!ELSEIF  "$(CFG)" == "apr_ldap - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FD /EHsc /c
# ADD CPP /nologo /MDd /W3 /Zi /Od /I "../include" /I "../../apr/include" /I "../include/private" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /Fo"$(INTDIR)\" /Fd"$(INTDIR)\apr_ldap_src" /FD /EHsc /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o /win32 "NUL"
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o /win32 "NUL"
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /fo"Debug/apr_ldap-1.res" /d DLL_NAME="apr_ldap" /d "_DEBUG" /d "APU_VERSION_ONLY" /I "../include" /I "../../apr/include"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib advapi32.lib ws2_32.lib mswsock.lib wldap32.lib ole32.lib /nologo /base:"0x6EEB0000" /subsystem:windows /dll /incremental:no /debug
# ADD LINK32 kernel32.lib advapi32.lib ws2_32.lib mswsock.lib wldap32.lib ole32.lib /nologo /base:"0x6EEB0000" /subsystem:windows /dll /incremental:no /debug /out:"Debug\apr_ldap-1.dll" /pdb:"Debug\apr_ldap-1.pdb" /implib:"Debug\apr_ldap-1.lib" /MACHINE:X86
# Begin Special Build Tool
TargetPath=Debug\apr_ldap-1.dll
SOURCE="$(InputPath)"
PostBuild_Desc=Embed .manifest
PostBuild_Cmds=if exist $(TargetPath).manifest mt.exe -manifest $(TargetPath).manifest -outputresource:$(TargetPath);2
# End Special Build Tool

!ELSEIF  "$(CFG)" == "apr_ldap - x64 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "x64\Release"
# PROP BASE Intermediate_Dir "x64\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "x64\Release"
# PROP Intermediate_Dir "x64\Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FD /c
# ADD CPP /nologo /MD /W3 /Zi /O2 /Oy- /I "../include" /I "../../apr/include" /I "../include/private" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /Fo"$(INTDIR)\" /Fd"$(INTDIR)\apr_ldap_src" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o /win32 "NUL"
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o /win32 "NUL"
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /fo"x64/Release/apr_ldap-1.res" /d DLL_NAME="apr_ldap" /d "NDEBUG" /d "APU_VERSION_ONLY" /I "../include" /I "../../apr/include"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib advapi32.lib ws2_32.lib mswsock.lib wldap32.lib ole32.lib /nologo /base:"0x6EEB0000" /subsystem:windows /dll /incremental:no /debug /opt:ref
# ADD LINK32 kernel32.lib advapi32.lib ws2_32.lib mswsock.lib wldap32.lib ole32.lib /nologo /base:"0x6EEB0000" /subsystem:windows /dll /incremental:no /debug /out:"x64\Release\apr_ldap-1.dll" /pdb:"x64\Release\apr_ldap-1.pdb" /implib:"x64\Release\apr_ldap-1.lib" /MACHINE:X64 /opt:ref
# Begin Special Build Tool
TargetPath=x64\Release\apr_ldap-1.dll
SOURCE="$(InputPath)"
PostBuild_Desc=Embed .manifest
PostBuild_Cmds=if exist $(TargetPath).manifest mt.exe -manifest $(TargetPath).manifest -outputresource:$(TargetPath);2
# End Special Build Tool

!ELSEIF  "$(CFG)" == "apr_ldap - x64 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "x64\Debug"
# PROP BASE Intermediate_Dir "x64\Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "x64\Debug"
# PROP Intermediate_Dir "x64\Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FD /EHsc /c
# ADD CPP /nologo /MDd /W3 /Zi /Od /I "../include" /I "../../apr/include" /I "../include/private" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /Fo"$(INTDIR)\" /Fd"$(INTDIR)\apr_ldap_src" /FD /EHsc /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o /win32 "NUL"
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o /win32 "NUL"
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /fo"x64/Debug/apr_ldap-1.res" /d DLL_NAME="apr_ldap" /d "_DEBUG" /d "APU_VERSION_ONLY" /I "../include" /I "../../apr/include"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib advapi32.lib ws2_32.lib mswsock.lib wldap32.lib ole32.lib /nologo /base:"0x6EEB0000" /subsystem:windows /dll /incremental:no /debug
# ADD LINK32 kernel32.lib advapi32.lib ws2_32.lib mswsock.lib wldap32.lib ole32.lib /nologo /base:"0x6EEB0000" /subsystem:windows /dll /incremental:no /debug /out:"x64\Debug\apr_ldap-1.dll" /pdb:"x64\Debug\apr_ldap-1.pdb" /implib:"x64\Debug\apr_ldap-1.lib" /MACHINE:X64
# Begin Special Build Tool
TargetPath=x64\Debug\apr_ldap-1.dll
SOURCE="$(InputPath)"
PostBuild_Desc=Embed .manifest
PostBuild_Cmds=if exist $(TargetPath).manifest mt.exe -manifest $(TargetPath).manifest -outputresource:$(TargetPath);2
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "apr_ldap - Win32 Release"
# Name "apr_ldap - Win32 Debug"
# Name "apr_ldap - x64 Release"
# Name "apr_ldap - x64 Debug"
# Begin Group "Source Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\apr_ldap_init.c
# End Source File
# Begin Source File

SOURCE=.\apr_ldap_option.c
# End Source File 
# Begin Source File

SOURCE=.\apr_ldap_rebind.c
# End Source File
# End Group
# Begin Group "Public Header Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\include\apr_ldap.h
# End Source File
# Begin Source File

SOURCE=..\include\apr_ldap_init.h
# End Source File
# Begin Source File

SOURCE=..\include\apr_ldap_option.h
# End Source File
# Begin Source File

SOURCE=..\include\apr_ldap_rebind.h
# End Source File
# Begin Source File

SOURCE=..\include\apr_ldap_url.h
# End Source File
# End Group
# Begin Group "Internal Header Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\include\private\apu_config.h
# End Source File
# Begin Source File

SOURCE=..\include\private\apu_internal.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\libaprutil.rc
# End Source File
# End Target
# End Project
