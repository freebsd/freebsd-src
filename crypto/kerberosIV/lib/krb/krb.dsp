# Microsoft Developer Studio Project File - Name="krb" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=krb - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "krb.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "krb.mak" CFG="krb - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "krb - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "krb - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "krb - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Release"
# PROP BASE Intermediate_Dir ".\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\Release"
# PROP Intermediate_Dir ".\Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "." /I "..\..\include" /I "..\..\include\win32" /I "..\des" /I "..\roken" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "HAVE_CONFIG_H" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 ..\roken\Release\roken.lib ..\des\Release\des.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /dll /machine:I386

!ELSEIF  "$(CFG)" == "krb - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Debug"
# PROP BASE Intermediate_Dir ".\Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\Debug"
# PROP Intermediate_Dir ".\Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /I "." /I "..\..\include" /I "..\..\include\win32" /I "..\des" /I "..\roken" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "HAVE_CONFIG_H" /YX /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386
# ADD LINK32 ..\roken\Debug\roken.lib ..\des\Debug\des.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /dll /debug /machine:I386

!ENDIF 

# Begin Target

# Name "krb - Win32 Release"
# Name "krb - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\cr_err_reply.c
# End Source File
# Begin Source File

SOURCE=.\create_auth_reply.c
# End Source File
# Begin Source File

SOURCE=.\create_ciph.c
# End Source File
# Begin Source File

SOURCE=.\create_ticket.c
# End Source File
# Begin Source File

SOURCE=.\debug_decl.c
# End Source File
# Begin Source File

SOURCE=.\decomp_ticket.c
# End Source File
# Begin Source File

SOURCE=.\dllmain.c
# End Source File
# Begin Source File

SOURCE=.\encrypt_ktext.c
# End Source File
# Begin Source File

SOURCE=.\extra.c
# End Source File
# Begin Source File

SOURCE=.\get_ad_tkt.c
# End Source File
# Begin Source File

SOURCE=.\get_cred.c
# End Source File
# Begin Source File

SOURCE=.\get_default_principal.c
# End Source File
# Begin Source File

SOURCE=.\get_host.c
# End Source File
# Begin Source File

SOURCE=.\get_in_tkt.c
# End Source File
# Begin Source File

SOURCE=.\get_krbrlm.c
# End Source File
# Begin Source File

SOURCE=.\get_svc_in_tkt.c
# End Source File
# Begin Source File

SOURCE=.\get_tf_fullname.c
# End Source File
# Begin Source File

SOURCE=.\get_tf_realm.c
# End Source File
# Begin Source File

SOURCE=.\getaddrs.c
# End Source File
# Begin Source File

SOURCE=.\getfile.c
# End Source File
# Begin Source File

SOURCE=.\getrealm.c
# End Source File
# Begin Source File

SOURCE=.\getst.c
# End Source File
# Begin Source File

SOURCE=.\k_gethostname.c
# End Source File
# Begin Source File

SOURCE=.\k_getport.c
# End Source File
# Begin Source File

SOURCE=.\k_getsockinst.c
# End Source File
# Begin Source File

SOURCE=.\k_localtime.c
# End Source File
# Begin Source File

SOURCE=.\kdc_reply.c
# End Source File
# Begin Source File

SOURCE=.\kntoln.c
# End Source File
# Begin Source File

SOURCE=.\krb.def
# End Source File
# Begin Source File

SOURCE=.\krb_check_auth.c
# End Source File
# Begin Source File

SOURCE=.\krb_equiv.c
# End Source File
# Begin Source File

SOURCE=.\krb_err_txt.c
# End Source File
# Begin Source File

SOURCE=.\krb_get_in_tkt.c
# End Source File
# Begin Source File

SOURCE=.\lifetime.c
# End Source File
# Begin Source File

SOURCE=.\logging.c
# End Source File
# Begin Source File

SOURCE=.\lsb_addr_comp.c
# End Source File
# Begin Source File

SOURCE=.\mk_auth.c
# End Source File
# Begin Source File

SOURCE=.\mk_err.c
# End Source File
# Begin Source File

SOURCE=.\mk_priv.c
# End Source File
# Begin Source File

SOURCE=.\mk_req.c
# End Source File
# Begin Source File

SOURCE=.\mk_safe.c
# End Source File
# Begin Source File

SOURCE=.\month_sname.c
# End Source File
# Begin Source File

SOURCE=.\name2name.c
# End Source File
# Begin Source File

SOURCE=.\netread.c
# End Source File
# Begin Source File

SOURCE=.\netwrite.c
# End Source File
# Begin Source File

SOURCE=.\one.c
# End Source File
# Begin Source File

SOURCE=.\parse_name.c
# End Source File
# Begin Source File

SOURCE=.\rd_err.c
# End Source File
# Begin Source File

SOURCE=.\rd_priv.c
# End Source File
# Begin Source File

SOURCE=.\rd_req.c
# End Source File
# Begin Source File

SOURCE=.\rd_safe.c
# End Source File
# Begin Source File

SOURCE=.\read_service_key.c
# End Source File
# Begin Source File

SOURCE=.\realm_parse.c
# End Source File
# Begin Source File

SOURCE=.\recvauth.c
# End Source File
# Begin Source File

SOURCE=.\rw.c
# End Source File
# Begin Source File

SOURCE=.\save_credentials.c
# End Source File
# Begin Source File

SOURCE=.\send_to_kdc.c
# End Source File
# Begin Source File

SOURCE=.\sendauth.c
# End Source File
# Begin Source File

SOURCE=.\stime.c
# End Source File
# Begin Source File

SOURCE=.\str2key.c
# End Source File
# Begin Source File

SOURCE=.\ticket_memory.c
# End Source File
# Begin Source File

SOURCE=.\time.c
# End Source File
# Begin Source File

SOURCE=.\tkt_string.c
# End Source File
# Begin Source File

SOURCE=.\unparse_name.c
# End Source File
# Begin Source File

SOURCE=.\util.c
# End Source File
# Begin Source File

SOURCE=.\verify_user.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=.\klog.h
# End Source File
# Begin Source File

SOURCE=".\krb-protos.h"
# End Source File
# Begin Source File

SOURCE=.\krb.h
# End Source File
# Begin Source File

SOURCE=.\krb_locl.h
# End Source File
# Begin Source File

SOURCE=.\krb_log.h
# End Source File
# Begin Source File

SOURCE=.\prot.h
# End Source File
# Begin Source File

SOURCE=.\ticket_memory.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\krb.rc
# End Source File
# End Group
# End Target
# End Project
