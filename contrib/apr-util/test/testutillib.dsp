# Microsoft Developer Studio Project File - Name="testutillib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=testutillib - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "testutillib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "testutillib.mak" CFG="testutillib - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "testutillib - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "testutillib - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE "testutillib - Win32 Release9x" (based on "Win32 (x86) External Target")
!MESSAGE "testutillib - Win32 Debug9x" (based on "Win32 (x86) External Target")
!MESSAGE "testutillib - x64 Release" (based on "Win32 (x86) External Target")
!MESSAGE "testutillib - x64 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "testutillib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ""
# PROP BASE Intermediate_Dir ""
# PROP BASE Cmd_Line "NMAKE /f Makefile.win INTDIR=LibR OUTDIR=LibR MODEL=static all check"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "LibR\testall.exe"
# PROP BASE Bsc_Name ""
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ""
# PROP Intermediate_Dir ""
# PROP Cmd_Line "NMAKE /f Makefile.win INTDIR=LibR OUTDIR=LibR MODEL=static all check"
# PROP Rebuild_Opt "/a"
# PROP Target_File "LibR\testall.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "testutillib - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ""
# PROP BASE Intermediate_Dir ""
# PROP BASE Cmd_Line "NMAKE /f Makefile.win INTDIR=LibD OUTDIR=LibD MODEL=static _DEBUG=1 all check"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "LibD\testall.exe"
# PROP BASE Bsc_Name ""
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ""
# PROP Intermediate_Dir ""
# PROP Cmd_Line "NMAKE /f Makefile.win INTDIR=LibD OUTDIR=LibD MODEL=static _DEBUG=1 all check"
# PROP Rebuild_Opt "/a"
# PROP Target_File "LibD\testall.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "testutillib - Win32 Release9x"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ""
# PROP BASE Intermediate_Dir ""
# PROP BASE Cmd_Line "NMAKE /f Makefile.win INTDIR=9x\LibR OUTDIR=9x\LibR MODEL=static all check"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "9x\LibR\testall.exe"
# PROP BASE Bsc_Name ""
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ""
# PROP Intermediate_Dir ""
# PROP Cmd_Line "NMAKE /f Makefile.win INTDIR=9x\LibR OUTDIR=9x\LibR MODEL=static all check"
# PROP Rebuild_Opt "/a"
# PROP Target_File "9x\LibR\testall.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "testutillib - Win32 Debug9x"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ""
# PROP BASE Intermediate_Dir ""
# PROP BASE Cmd_Line "NMAKE /f Makefile.win INTDIR=9x\LibD OUTDIR=9x\LibD MODEL=static _DEBUG=1 all check"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "9x\LibD\testall.exe"
# PROP BASE Bsc_Name ""
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ""
# PROP Intermediate_Dir ""
# PROP Cmd_Line "NMAKE /f Makefile.win INTDIR=9x\LibD OUTDIR=9x\LibD MODEL=static _DEBUG=1 all check"
# PROP Rebuild_Opt "/a"
# PROP Target_File "9x\LibD\testall.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "testutillib - x64 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ""
# PROP BASE Intermediate_Dir ""
# PROP BASE Cmd_Line "NMAKE /f Makefile.win INTDIR=x64\LibR OUTDIR=x64\LibR MODEL=static all check"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "x64\LibR\testall.exe"
# PROP BASE Bsc_Name ""
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ""
# PROP Intermediate_Dir ""
# PROP Cmd_Line "NMAKE /f Makefile.win INTDIR=x64\LibR OUTDIR=x64\LibR MODEL=static all check"
# PROP Rebuild_Opt "/a"
# PROP Target_File "x64\LibR\testall.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "testutillib - x64 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ""
# PROP BASE Intermediate_Dir ""
# PROP BASE Cmd_Line "NMAKE /f Makefile.win INTDIR=x64\LibD OUTDIR=x64\LibD MODEL=static _DEBUG=1 all check"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "x64\LibD\testall.exe"
# PROP BASE Bsc_Name ""
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ""
# PROP Intermediate_Dir ""
# PROP Cmd_Line "NMAKE /f Makefile.win INTDIR=x64\LibD OUTDIR=x64\LibD MODEL=static _DEBUG=1 all check"
# PROP Rebuild_Opt "/a"
# PROP Target_File "x64\LibD\testall.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "testutillib - Win32 Release"
# Name "testutillib - Win32 Debug"
# Name "testutillib - Win32 Release9x"
# Name "testutillib - Win32 Debug9x"
# Name "testutillib - x64 Release"
# Name "testutillib - x64 Debug"
# Begin Group "testall Source Files"

# PROP Default_Filter ".c"
# Begin Source File

SOURCE=.\abts.c
# End Source File
# Begin Source File

SOURCE=.\abts.h
# End Source File
# Begin Source File

SOURCE=.\abts_tests.h
# End Source File
# Begin Source File

SOURCE=.\testbuckets.c
# End Source File
# Begin Source File

SOURCE=.\testdate.c
# End Source File
# Begin Source File

SOURCE=.\testdbd.c
# End Source File
# Begin Source File

SOURCE=.\testdbm.c
# End Source File
# Begin Source File

SOURCE=.\testldap.c
# End Source File
# Begin Source File

SOURCE=.\testmd4.c
# End Source File
# Begin Source File

SOURCE=.\testmd5.c
# End Source File
# Begin Source File

SOURCE=.\testmemcache.c
# End Source File
# Begin Source File

SOURCE=.\testpass.c
# End Source File
# Begin Source File

SOURCE=.\testqueue.c
# End Source File
# Begin Source File

SOURCE=.\testreslist.c
# End Source File
# Begin Source File

SOURCE=.\testrmm.c
# End Source File
# Begin Source File

SOURCE=.\teststrmatch.c
# End Source File
# Begin Source File

SOURCE=.\testuri.c
# End Source File
# Begin Source File

SOURCE=.\testutil.c
# End Source File
# Begin Source File

SOURCE=.\testuuid.c
# End Source File
# Begin Source File

SOURCE=.\testxlate.c
# End Source File
# Begin Source File

SOURCE=.\testxml.c
# End Source File
# End Group
# Begin Group "Other Source Files"

# PROP Default_Filter ".c"
# Begin Source File

SOURCE=.\dbd.c
# End Source File
# End Group
# Begin Source File

SOURCE=.\Makefile.win
# End Source File
# End Target
# End Project
