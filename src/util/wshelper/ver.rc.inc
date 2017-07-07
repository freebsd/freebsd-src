#ifdef RC_INVOKED

VS_VERSION_INFO VERSIONINFO
FILEVERSION    VER_FILEVERSION
PRODUCTVERSION VER_PRODUCTVERSION
FILEFLAGSMASK  VER_FILEFLAGSMASK
FILEFLAGS      VER_FILEFLAGS
FILEOS         VER_FILEOS
FILETYPE       VER_FILETYPE
FILESUBTYPE    VER_FILESUBTYPE
BEGIN
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x0409, 0x04B0
    END

    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904B0"   /* LANG_ENGLISH/SUBLANG_ENGLISH_US, Unicode CP */
        BEGIN
#if defined(VER_EXTRA_LABEL) && defined(VER_EXTRA_VALUE)
            VALUE VER_EXTRA_LABEL,   VER_EXTRA_VALUE
#endif
#ifdef VER_COMMENT
            VALUE "Comment",         VER_COMMENT
#endif
#ifdef VER_USERNAME
            VALUE "Built By",        VER_USERNAME
#endif
#ifdef VER_HOSTNAME
            VALUE "Build Host",      VER_HOSTNAME
#endif
#ifdef VER_DATE
            VALUE "Build Time",      VER_DATE
#endif
#ifdef VER_VENDOR
            VALUE "Modified by Vendor",  VER_VENDOR
#endif
            VALUE "CompanyName",     VER_COMPANYNAME_STR
            VALUE "FileDescription", VER_FILEDESCRIPTION_STR EXPORT_TAG
            VALUE "FileVersion",     VER_FILEVERSION_STR
            VALUE "InternalName",    VER_INTERNALNAME_STR
            VALUE "LegalCopyright",  VER_LEGALCOPYRIGHT_STR
#ifdef VER_LEGALTRADEMARK_STR
            VALUE "LegalTrademark",  VER_LEGALTRADEMARK_STR
#endif
            VALUE "OriginalFilename",VER_ORIGINALFILENAME_STR
            VALUE "ProductName",     VER_PRODUCTNAME_STR
            VALUE "ProductVersion",  VER_PRODUCTVERSION_STR
#ifdef VER_SPECIALBUILD
            VALUE "SpecialBuild",    VER_SPECIALBUILD
#endif
        END
    END
END

#endif
