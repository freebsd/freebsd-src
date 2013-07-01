# Microsoft Developer Studio Project File - Name="aprutil" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=aprutil - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "aprutil.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "aprutil.mak" CFG="aprutil - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "aprutil - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "aprutil - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "aprutil - x64 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "aprutil - x64 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "aprutil - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "LibR"
# PROP BASE Intermediate_Dir "LibR"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "LibR"
# PROP Intermediate_Dir "LibR"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FD /c
# ADD CPP /nologo /MD /W3 /O2 /Oy- /Zi /I "./include" /I "../apr/include" /I "./include/private" /I "../apr-iconv/include" /I "./dbm/sdbm" /I "./xml/expat/lib" /D "NDEBUG" /D "APR_DECLARE_STATIC" /D "APU_DECLARE_STATIC" /D "API_DECLARE_STATIC" /D "APU_USE_SDBM" /D "HAVE_SQL_H" /D "XML_STATIC" /D "WIN32" /D "_WINDOWS" /Fo"$(INTDIR)\" /Fd"$(OUTDIR)\aprutil-1" /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"LibR\aprutil-1.lib"

!ELSEIF  "$(CFG)" == "aprutil - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "LibD"
# PROP BASE Intermediate_Dir "LibD"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "LibD"
# PROP Intermediate_Dir "LibD"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /EHsc /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FD /c
# ADD CPP /nologo /MDd /W3 /EHsc /Zi /Od /I "./include" /I "../apr/include" /I "./include/private" /I "../apr-iconv/include" /I "./dbm/sdbm" /I "./xml/expat/lib" /D "_DEBUG" /D "APR_DECLARE_STATIC" /D "APU_DECLARE_STATIC" /D "API_DECLARE_STATIC" /D "APU_USE_SDBM" /D "HAVE_SQL_H" /D "XML_STATIC" /D "WIN32" /D "_WINDOWS" /Fo"$(INTDIR)\" /Fd"$(OUTDIR)\aprutil-1" /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"LibD\aprutil-1.lib"

!ELSEIF  "$(CFG)" == "aprutil - x64 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "x64\LibR"
# PROP BASE Intermediate_Dir "x64\LibR"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "x64\LibR"
# PROP Intermediate_Dir "x64\LibR"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FD /c
# ADD CPP /nologo /MD /W3 /Zi /O2 /Oy- /I "./include" /I "../apr/include" /I "./include/private" /I "../apr-iconv/include" /I "./dbm/sdbm" /I "./xml/expat/lib" /D "NDEBUG" /D "APR_DECLARE_STATIC" /D "APU_DECLARE_STATIC" /D "API_DECLARE_STATIC" /D "APU_USE_SDBM" /D "HAVE_SQL_H" /D "XML_STATIC" /D "WIN32" /D "_WINDOWS" /Fo"$(INTDIR)\" /Fd"$(OUTDIR)\aprutil-1" /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"x64\LibR\aprutil-1.lib"

!ELSEIF  "$(CFG)" == "aprutil - x64 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "x64\LibD"
# PROP BASE Intermediate_Dir "x64\LibD"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "x64\LibD"
# PROP Intermediate_Dir "x64\LibD"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FD /EHsc /c
# ADD CPP /nologo /MDd /W3 /Zi /Od /I "./include" /I "../apr/include" /I "./include/private" /I "../apr-iconv/include" /I "./dbm/sdbm" /I "./xml/expat/lib" /D "_DEBUG" /D "APR_DECLARE_STATIC" /D "APU_DECLARE_STATIC" /D "API_DECLARE_STATIC" /D "APU_USE_SDBM" /D "HAVE_SQL_H" /D "XML_STATIC" /D "WIN32" /D "_WINDOWS" /Fo"$(INTDIR)\" /Fd"$(OUTDIR)\aprutil-1" /FD /EHsc /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"x64\LibD\aprutil-1.lib"

!ENDIF 

# Begin Target

# Name "aprutil - Win32 Release"
# Name "aprutil - Win32 Debug"
# Name "aprutil - x64 Release"
# Name "aprutil - x64 Debug"
# Begin Group "Source Files"

# PROP Default_Filter ""
# Begin Group "buckets"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\buckets\apr_brigade.c
# End Source File
# Begin Source File

SOURCE=.\buckets\apr_buckets.c
# End Source File
# Begin Source File

SOURCE=.\buckets\apr_buckets_alloc.c
# End Source File
# Begin Source File

SOURCE=.\buckets\apr_buckets_eos.c
# End Source File
# Begin Source File

SOURCE=.\buckets\apr_buckets_file.c
# End Source File
# Begin Source File

SOURCE=.\buckets\apr_buckets_flush.c
# End Source File
# Begin Source File

SOURCE=.\buckets\apr_buckets_heap.c
# End Source File
# Begin Source File

SOURCE=.\buckets\apr_buckets_mmap.c
# End Source File
# Begin Source File

SOURCE=.\buckets\apr_buckets_pipe.c
# End Source File
# Begin Source File

SOURCE=.\buckets\apr_buckets_pool.c
# End Source File
# Begin Source File

SOURCE=.\buckets\apr_buckets_refcount.c
# End Source File
# Begin Source File

SOURCE=.\buckets\apr_buckets_simple.c
# End Source File
# Begin Source File

SOURCE=.\buckets\apr_buckets_socket.c
# End Source File
# End Group
# Begin Group "crypto"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\crypto\apr_crypto.c
# End Source File
# Begin Source File

SOURCE=.\crypto\apr_md4.c
# End Source File
# Begin Source File

SOURCE=.\crypto\apr_md5.c
# End Source File
# Begin Source File

SOURCE=.\crypto\apr_sha1.c
# End Source File
# Begin Source File

SOURCE=.\crypto\getuuid.c
# End Source File
# Begin Source File

SOURCE=.\crypto\uuid.c
# End Source File
# End Group
# Begin Group "dbd"
# PROP Default_Filter ""
# Begin Source File

SOURCE=.\dbd\apr_dbd.c
# End Source File
# Begin Source File

SOURCE=.\dbd\apr_dbd_freetds.c
# End Source File
# Begin Source File

SOURCE=.\dbd\apr_dbd_mysql.c
# End Source File
# Begin Source File

SOURCE=.\dbd\apr_dbd_odbc.c
# End Source File
# Begin Source File

SOURCE=.\dbd\apr_dbd_oracle.c
# End Source File
# Begin Source File

SOURCE=.\dbd\apr_dbd_pgsql.c
# End Source File
# Begin Source File

SOURCE=.\dbd\apr_dbd_sqlite2.c
# End Source File
# Begin Source File

SOURCE=.\dbd\apr_dbd_sqlite3.c
# End Source File
# End Group
# Begin Group "dbm"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\dbm\apr_dbm.c
# End Source File
# Begin Source File

SOURCE=.\dbm\apr_dbm_berkeleydb.c
# End Source File
# Begin Source File

SOURCE=.\dbm\apr_dbm_gdbm.c
# End Source File
# Begin Source File

SOURCE=.\dbm\apr_dbm_sdbm.c
# End Source File
# End Group
# Begin Group "encoding"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\encoding\apr_base64.c
# End Source File
# End Group
# Begin Group "hooks"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\hooks\apr_hooks.c
# End Source File
# End Group
# Begin Group "ldap"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\ldap\apr_ldap_init.c
# End Source File
# Begin Source File

SOURCE=.\ldap\apr_ldap_option.c
# End Source File
# Begin Source File

SOURCE=.\ldap\apr_ldap_rebind.c
# End Source File
# Begin Source File

SOURCE=.\ldap\apr_ldap_stub.c
# End Source File
# Begin Source File

SOURCE=.\ldap\apr_ldap_url.c
# End Source File
# End Group
# Begin Group "memcache"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\memcache\apr_memcache.c
# End Source File
# End Group
# Begin Group "misc"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\misc\apr_date.c
# End Source File
# Begin Source File

SOURCE=.\misc\apu_dso.c
# End Source File
# Begin Source File

SOURCE=.\misc\apr_queue.c
# End Source File
# Begin Source File

SOURCE=.\misc\apr_reslist.c
# End Source File
# Begin Source File

SOURCE=.\misc\apr_rmm.c
# End Source File
# Begin Source File

SOURCE=.\misc\apr_thread_pool.c
# End Source File
# Begin Source File

SOURCE=.\misc\apu_version.c
# End Source File
# End Group
# Begin Group "sdbm"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\dbm\sdbm\sdbm.c
# End Source File
# Begin Source File

SOURCE=.\dbm\sdbm\sdbm_hash.c
# End Source File
# Begin Source File

SOURCE=.\dbm\sdbm\sdbm_lock.c
# End Source File
# Begin Source File

SOURCE=.\dbm\sdbm\sdbm_pair.c
# End Source File
# Begin Source File

SOURCE=.\dbm\sdbm\sdbm_pair.h
# End Source File
# Begin Source File

SOURCE=.\dbm\sdbm\sdbm_private.h
# End Source File
# Begin Source File

SOURCE=.\dbm\sdbm\sdbm_tune.h
# End Source File
# End Group
# Begin Group "strmatch"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\strmatch\apr_strmatch.c
# End Source File
# End Group
# Begin Group "uri"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\uri\apr_uri.c
# End Source File
# End Group
# Begin Group "xlate"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\xlate\xlate.c
# End Source File
# End Group
# Begin Group "xml"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\xml\apr_xml.c
# End Source File
# End Group
# End Group
# Begin Group "Generated Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\include\apr_ldap.h.in
# End Source File
# Begin Source File

SOURCE=.\include\apr_ldap.hnw
# End Source File
# Begin Source File

SOURCE=.\include\apr_ldap.hw

!IF  "$(CFG)" == "aprutil - Win32 Release"

# Begin Custom Build - Creating apr_ldap.h from apr_ldap.hw
InputPath=.\include\apr_ldap.hw

".\include\apr_ldap.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\include\apr_ldap.hw > .\include\apr_ldap.h

# End Custom Build

!ELSEIF  "$(CFG)" == "aprutil - Win32 Debug"

# Begin Custom Build - Creating apr_ldap.h from apr_ldap.hw
InputPath=.\include\apr_ldap.hw

".\include\apr_ldap.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\include\apr_ldap.hw > .\include\apr_ldap.h

# End Custom Build

!ELSEIF  "$(CFG)" == "aprutil - x64 Release"

# Begin Custom Build - Creating apr_ldap.h from apr_ldap.hw
InputPath=.\include\apr_ldap.hw

".\include\apr_ldap.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\include\apr_ldap.hw > .\include\apr_ldap.h

# End Custom Build

!ELSEIF  "$(CFG)" == "aprutil - x64 Debug"

# Begin Custom Build - Creating apr_ldap.h from apr_ldap.hw
InputPath=.\include\apr_ldap.hw

".\include\apr_ldap.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\include\apr_ldap.hw > .\include\apr_ldap.h

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\include\apu.h.in
# End Source File
# Begin Source File

SOURCE=.\include\apu.hnw
# End Source File
# Begin Source File

SOURCE=.\include\apu.hw

!IF  "$(CFG)" == "aprutil - Win32 Release"

# Begin Custom Build - Creating apu.h from apu.hw
InputPath=.\include\apu.hw

".\include\apu.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\include\apu.hw > .\include\apu.h

# End Custom Build

!ELSEIF  "$(CFG)" == "aprutil - Win32 Debug"

# Begin Custom Build - Creating apu.h from apu.hw
InputPath=.\include\apu.hw

".\include\apu.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\include\apu.hw > .\include\apu.h

# End Custom Build

!ELSEIF  "$(CFG)" == "aprutil - x64 Release"

# Begin Custom Build - Creating apu.h from apu.hw
InputPath=.\include\apu.hw

".\include\apu.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\include\apu.hw > .\include\apu.h

# End Custom Build

!ELSEIF  "$(CFG)" == "aprutil - x64 Debug"

# Begin Custom Build - Creating apu.h from apu.hw
InputPath=.\include\apu.hw

".\include\apu.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\include\apu.hw > .\include\apu.h

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\include\private\apu_config.h.in
# End Source File
# Begin Source File

SOURCE=.\include\private\apu_config.hw

!IF  "$(CFG)" == "aprutil - Win32 Release"

# Begin Custom Build - Creating apu_config.h from apu_config.hw
InputPath=.\include\private\apu_config.hw

".\include\private\apu_config.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\include\private\apu_config.hw > .\include\private\apu_config.h

# End Custom Build

!ELSEIF  "$(CFG)" == "aprutil - Win32 Debug"

# Begin Custom Build - Creating apu_config.h from apu_config.hw
InputPath=.\include\private\apu_config.hw

".\include\private\apu_config.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\include\private\apu_config.hw > .\include\private\apu_config.h

# End Custom Build

!ELSEIF  "$(CFG)" == "aprutil - x64 Release"

# Begin Custom Build - Creating apu_config.h from apu_config.hw
InputPath=.\include\private\apu_config.hw

".\include\private\apu_config.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\include\private\apu_config.hw > .\include\private\apu_config.h

# End Custom Build

!ELSEIF  "$(CFG)" == "aprutil - x64 Debug"

# Begin Custom Build - Creating apu_config.h from apu_config.hw
InputPath=.\include\private\apu_config.hw

".\include\private\apu_config.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\include\private\apu_config.hw > .\include\private\apu_config.h

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\include\private\apu_select_dbm.h.in
# End Source File
# Begin Source File

SOURCE=.\include\private\apu_select_dbm.hw

!IF  "$(CFG)" == "aprutil - Win32 Release"

# Begin Custom Build - Creating apu_select_dbm.h from apu_select_dbm.hw
InputPath=.\include\private\apu_select_dbm.hw

".\include\private\apu_select_dbm.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\include\private\apu_select_dbm.hw > .\include\private\apu_select_dbm.h

# End Custom Build

!ELSEIF  "$(CFG)" == "aprutil - Win32 Debug"

# Begin Custom Build - Creating apu_select_dbm.h from apu_select_dbm.hw
InputPath=.\include\private\apu_select_dbm.hw

".\include\private\apu_select_dbm.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\include\private\apu_select_dbm.hw > .\include\private\apu_select_dbm.h

# End Custom Build

!ELSEIF  "$(CFG)" == "aprutil - x64 Release"

# Begin Custom Build - Creating apu_select_dbm.h from apu_select_dbm.hw
InputPath=.\include\private\apu_select_dbm.hw

".\include\private\apu_select_dbm.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\include\private\apu_select_dbm.hw > .\include\private\apu_select_dbm.h

# End Custom Build

!ELSEIF  "$(CFG)" == "aprutil - x64 Debug"

# Begin Custom Build - Creating apu_select_dbm.h from apu_select_dbm.hw
InputPath=.\include\private\apu_select_dbm.hw

".\include\private\apu_select_dbm.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\include\private\apu_select_dbm.hw > .\include\private\apu_select_dbm.h

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\include\apu_want.h.in
# End Source File
# Begin Source File

SOURCE=.\include\apu_want.hnw
# End Source File
# Begin Source File

SOURCE=.\include\apu_want.hw

!IF  "$(CFG)" == "aprutil - Win32 Release"

# Begin Custom Build - Creating apu_want.h from apu_want.hw
InputPath=.\include\apu_want.hw

".\include\apu_want.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\include\apu_want.hw > .\include\apu_want.h

# End Custom Build

!ELSEIF  "$(CFG)" == "aprutil - Win32 Debug"

# Begin Custom Build - Creating apu_want.h from apu_want.hw
InputPath=.\include\apu_want.hw

".\include\apu_want.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\include\apu_want.hw > .\include\apu_want.h

# End Custom Build

!ELSEIF  "$(CFG)" == "aprutil - x64 Release"

# Begin Custom Build - Creating apu_want.h from apu_want.hw
InputPath=.\include\apu_want.hw

".\include\apu_want.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\include\apu_want.hw > .\include\apu_want.h

# End Custom Build

!ELSEIF  "$(CFG)" == "aprutil - x64 Debug"

# Begin Custom Build - Creating apu_want.h from apu_want.hw
InputPath=.\include\apu_want.hw

".\include\apu_want.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type .\include\apu_want.hw > .\include\apu_want.h

# End Custom Build

!ENDIF 

# End Source File
# End Group
# Begin Group "Public Header Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\include\apr_anylock.h
# End Source File
# Begin Source File

SOURCE=.\include\apr_base64.h
# End Source File
# Begin Source File

SOURCE=.\include\apr_buckets.h
# End Source File
# Begin Source File

SOURCE=.\include\apr_date.h
# End Source File
# Begin Source File

SOURCE=.\include\apr_dbm.h
# End Source File
# Begin Source File

SOURCE=.\include\apr_hooks.h
# End Source File
# Begin Source File

SOURCE=.\include\apr_ldap_url.h
# End Source File
# Begin Source File

SOURCE=.\include\apr_md4.h
# End Source File
# Begin Source File

SOURCE=.\include\apr_md5.h
# End Source File
# Begin Source File

SOURCE=.\include\apr_memcache.h
# End Source File
# Begin Source File

SOURCE=.\include\apr_optional.h
# End Source File
# Begin Source File

SOURCE=.\include\apr_optional_hooks.h
# End Source File
# Begin Source File

SOURCE=.\include\apr_queue.h
# End Source File
# Begin Source File

SOURCE=.\include\apr_reslist.h
# End Source File
# Begin Source File

SOURCE=.\include\apr_rmm.h
# End Source File
# Begin Source File

SOURCE=.\include\apr_sdbm.h
# End Source File
# Begin Source File

SOURCE=.\include\apr_sha1.h
# End Source File
# Begin Source File

SOURCE=.\include\apr_strmatch.h
# End Source File
# Begin Source File

SOURCE=.\include\apr_thread_pool.h
# End Source File
# Begin Source File

SOURCE=.\include\apr_uri.h
# End Source File
# Begin Source File

SOURCE=.\include\apr_uuid.h
# End Source File
# Begin Source File

SOURCE=.\include\apr_xlate.h
# End Source File
# Begin Source File

SOURCE=.\include\apr_xml.h
# End Source File
# Begin Source File

SOURCE=.\include\apu_version.h
# End Source File
# End Group
# End Target
# End Project
