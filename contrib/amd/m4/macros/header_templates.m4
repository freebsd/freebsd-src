dnl FILE: m4/macros/header_templates.m4
dnl defines descriptions for various am-utils specific macros

AH_TEMPLATE([HAVE_AMU_FS_AUTO],
[Define if have automount filesystem])

AH_TEMPLATE([HAVE_AMU_FS_DIRECT],
[Define if have direct automount filesystem])

AH_TEMPLATE([HAVE_AMU_FS_TOPLVL],
[Define if have "top-level" filesystem])

AH_TEMPLATE([HAVE_AMU_FS_ERROR],
[Define if have error filesystem])

AH_TEMPLATE([HAVE_AMU_FS_INHERIT],
[Define if have inheritance filesystem])

AH_TEMPLATE([HAVE_AMU_FS_PROGRAM],
[Define if have program filesystem])

AH_TEMPLATE([HAVE_AMU_FS_LINK],
[Define if have symbolic-link filesystem])

AH_TEMPLATE([HAVE_AMU_FS_LINKX],
[Define if have symlink with existence check filesystem])

AH_TEMPLATE([HAVE_AMU_FS_HOST],
[Define if have NFS host-tree filesystem])

AH_TEMPLATE([HAVE_AMU_FS_NFSL],
[Define if have nfsl (NFS with local link check) filesystem])

AH_TEMPLATE([HAVE_AMU_FS_NFSX],
[Define if have multi-NFS filesystem])

AH_TEMPLATE([HAVE_AMU_FS_UNION],
[Define if have union filesystem])

AH_TEMPLATE([HAVE_MAP_FILE],
[Define if have file maps (everyone should have it!)])

AH_TEMPLATE([HAVE_MAP_NIS],
[Define if have NIS maps])

AH_TEMPLATE([HAVE_MAP_NISPLUS],
[Define if have NIS+ maps])

AH_TEMPLATE([HAVE_MAP_DBM],
[Define if have DBM maps])

AH_TEMPLATE([HAVE_MAP_NDBM],
[Define if have NDBM maps])

AH_TEMPLATE([HAVE_MAP_HESIOD],
[Define if have HESIOD maps])

AH_TEMPLATE([HAVE_MAP_LDAP],
[Define if have LDAP maps])

AH_TEMPLATE([HAVE_MAP_PASSWD],
[Define if have PASSWD maps])

AH_TEMPLATE([HAVE_MAP_UNION],
[Define if have UNION maps])

AH_TEMPLATE([HAVE_FS_UFS],
[Define if have UFS filesystem])

AH_TEMPLATE([HAVE_FS_XFS],
[Define if have XFS filesystem (irix)])

AH_TEMPLATE([HAVE_FS_EFS],
[Define if have EFS filesystem (irix)])

AH_TEMPLATE([HAVE_FS_NFS],
[Define if have NFS filesystem])

AH_TEMPLATE([HAVE_FS_NFS3],
[Define if have NFS3 filesystem])

AH_TEMPLATE([HAVE_FS_PCFS],
[Define if have PCFS filesystem])

AH_TEMPLATE([HAVE_FS_LOFS],
[Define if have LOFS filesystem])

AH_TEMPLATE([HAVE_FS_HSFS],
[Define if have HSFS filesystem])

AH_TEMPLATE([HAVE_FS_CDFS],
[Define if have CDFS filesystem])

AH_TEMPLATE([HAVE_FS_TFS],
[Define if have TFS filesystem])

AH_TEMPLATE([HAVE_FS_TMPFS],
[Define if have TMPFS filesystem])

AH_TEMPLATE([HAVE_FS_MFS],
[Define if have MFS filesystem])

AH_TEMPLATE([HAVE_FS_CFS],
[Define if have CFS (crypto) filesystem])

AH_TEMPLATE([HAVE_FS_AUTOFS],
[Define if have AUTOFS filesystem])

AH_TEMPLATE([HAVE_FS_CACHEFS],
[Define if have CACHEFS filesystem])

AH_TEMPLATE([HAVE_FS_NULLFS],
[Define if have NULLFS (loopback on bsd44) filesystem])

AH_TEMPLATE([HAVE_FS_UNIONFS],
[Define if have UNIONFS filesystem])

AH_TEMPLATE([HAVE_FS_UMAPFS],
[Define if have UMAPFS (uid/gid mapping) filesystem])

AH_TEMPLATE([MOUNT_TYPE_UFS],
[Mount(2) type/name for UFS filesystem])

AH_TEMPLATE([MOUNT_TYPE_XFS],
[Mount(2) type/name for XFS filesystem (irix)])

AH_TEMPLATE([MOUNT_TYPE_EFS],
[Mount(2) type/name for EFS filesystem (irix)])

AH_TEMPLATE([MOUNT_TYPE_NFS],
[Mount(2) type/name for NFS filesystem])

AH_TEMPLATE([MOUNT_TYPE_NFS3],
[Mount(2) type/name for NFS3 filesystem])

AH_TEMPLATE([MOUNT_TYPE_PCFS],
[Mount(2) type/name for PCFS filesystem. XXX: conf/trap/trap_hpux.h may override this definition for HPUX 9.0])

AH_TEMPLATE([MOUNT_TYPE_LOFS],
[Mount(2) type/name for LOFS filesystem])

AH_TEMPLATE([MOUNT_TYPE_CDFS],
[Mount(2) type/name for CDFS filesystem])

AH_TEMPLATE([MOUNT_TYPE_TFS],
[Mount(2) type/name for TFS filesystem])

AH_TEMPLATE([MOUNT_TYPE_TMPFS],
[Mount(2) type/name for TMPFS filesystem])

AH_TEMPLATE([MOUNT_TYPE_MFS],
[Mount(2) type/name for MFS filesystem])

AH_TEMPLATE([MOUNT_TYPE_CFS],
[Mount(2) type/name for CFS (crypto) filesystem])

AH_TEMPLATE([MOUNT_TYPE_AUTOFS],
[Mount(2) type/name for AUTOFS filesystem])

AH_TEMPLATE([MOUNT_TYPE_CACHEFS],
[Mount(2) type/name for CACHEFS filesystem])

AH_TEMPLATE([MOUNT_TYPE_IGNORE],
[Mount(2) type/name for IGNORE filesystem (not real just ignore for df)])

AH_TEMPLATE([MOUNT_TYPE_NULLFS],
[Mount(2) type/name for NULLFS (loopback on bsd44) filesystem])

AH_TEMPLATE([MOUNT_TYPE_UNIONFS],
[Mount(2) type/name for UNIONFS filesystem])

AH_TEMPLATE([MOUNT_TYPE_UMAPFS],
[Mount(2) type/name for UMAPFS (uid/gid mapping) filesystem])

AH_TEMPLATE([MNTTAB_TYPE_UFS],
[Mount-table entry name for UFS filesystem])

AH_TEMPLATE([MNTTAB_TYPE_XFS],
[Mount-table entry name for XFS filesystem (irix)])

AH_TEMPLATE([MNTTAB_TYPE_EFS],
[Mount-table entry name for EFS filesystem (irix)])

AH_TEMPLATE([MNTTAB_TYPE_NFS],
[Mount-table entry name for NFS filesystem])

AH_TEMPLATE([MNTTAB_TYPE_NFS3],
[Mount-table entry name for NFS3 filesystem])

AH_TEMPLATE([MNTTAB_TYPE_PCFS],
[Mount-table entry name for PCFS filesystem])

AH_TEMPLATE([MNTTAB_TYPE_LOFS],
[Mount-table entry name for LOFS filesystem])

AH_TEMPLATE([MNTTAB_TYPE_CDFS],
[Mount-table entry name for CDFS filesystem])

AH_TEMPLATE([MNTTAB_TYPE_TFS],
[Mount-table entry name for TFS filesystem])

AH_TEMPLATE([MNTTAB_TYPE_TMPFS],
[Mount-table entry name for TMPFS filesystem])

AH_TEMPLATE([MNTTAB_TYPE_MFS],
[Mount-table entry name for MFS filesystem])

AH_TEMPLATE([MNTTAB_TYPE_CFS],
[Mount-table entry name for CFS (crypto) filesystem])

AH_TEMPLATE([MNTTAB_TYPE_AUTOFS],
[Mount-table entry name for AUTOFS filesystem])

AH_TEMPLATE([MNTTAB_TYPE_CACHEFS],
[Mount-table entry name for CACHEFS filesystem])

AH_TEMPLATE([MNTTAB_TYPE_NULLFS],
[Mount-table entry name for NULLFS (loopback on bsd44) filesystem])

AH_TEMPLATE([MNTTAB_TYPE_UNIONFS],
[Mount-table entry name for UNIONFS filesystem])

AH_TEMPLATE([MNTTAB_TYPE_UMAPFS],
[Mount-table entry name for UMAPFS (uid/gid mapping) filesystem])

AH_TEMPLATE([MNTTAB_FILE_NAME],
[Name of mount table file name])

AH_TEMPLATE([HIDE_MOUNT_TYPE],
[Name of mount type to hide amd mount from df(1)])

AH_TEMPLATE([MNTTAB_OPT_RO],
[Mount Table option string: Read only])

AH_TEMPLATE([MNTTAB_OPT_RW],
[Mount Table option string: Read/write])

AH_TEMPLATE([MNTTAB_OPT_RQ],
[Mount Table option string: Read/write with quotas])

AH_TEMPLATE([MNTTAB_OPT_QUOTA],
[Mount Table option string: Check quotas])

AH_TEMPLATE([MNTTAB_OPT_NOQUOTA],
[Mount Table option string: Don't check quotas])

AH_TEMPLATE([MNTTAB_OPT_ONERROR],
[Mount Table option string: action to taken on error])

AH_TEMPLATE([MNTTAB_OPT_TOOSOON],
[Mount Table option string: min. time between inconsistencies])

AH_TEMPLATE([MNTTAB_OPT_SOFT],
[Mount Table option string: Soft mount])

AH_TEMPLATE([MNTTAB_OPT_SPONGY],
[Mount Table option string: spongy mount])

AH_TEMPLATE([MNTTAB_OPT_HARD],
[Mount Table option string: Hard mount])

AH_TEMPLATE([MNTTAB_OPT_SUID],
[Mount Table option string: Set uid allowed])

AH_TEMPLATE([MNTTAB_OPT_NOSUID],
[Mount Table option string: Set uid not allowed])

AH_TEMPLATE([MNTTAB_OPT_GRPID],
[Mount Table option string: SysV-compatible gid on create])

AH_TEMPLATE([MNTTAB_OPT_REMOUNT],
[Mount Table option string: Change mount options])

AH_TEMPLATE([MNTTAB_OPT_NOSUB],
[Mount Table option string: Disallow mounts on subdirs])

AH_TEMPLATE([MNTTAB_OPT_MULTI],
[Mount Table option string: Do multi-component lookup])

AH_TEMPLATE([MNTTAB_OPT_INTR],
[Mount Table option string: Allow NFS ops to be interrupted])

AH_TEMPLATE([MNTTAB_OPT_NOINTR],
[Mount Table option string: Don't allow interrupted ops])

AH_TEMPLATE([MNTTAB_OPT_PORT],
[Mount Table option string: NFS server IP port number])

AH_TEMPLATE([MNTTAB_OPT_SECURE],
[Mount Table option string: Secure (AUTH_DES) mounting])

AH_TEMPLATE([MNTTAB_OPT_KERB],
[Mount Table option string: Secure (AUTH_Kerb) mounting])

AH_TEMPLATE([MNTTAB_OPT_RSIZE],
[Mount Table option string: Max NFS read size (bytes)])

AH_TEMPLATE([MNTTAB_OPT_WSIZE],
[Mount Table option string: Max NFS write size (bytes)])

AH_TEMPLATE([MNTTAB_OPT_TIMEO],
[Mount Table option string: NFS timeout (1/10 sec)])

AH_TEMPLATE([MNTTAB_OPT_RETRANS],
[Mount Table option string: Max retransmissions (soft mnts)])

AH_TEMPLATE([MNTTAB_OPT_ACTIMEO],
[Mount Table option string: Attr cache timeout (sec)])

AH_TEMPLATE([MNTTAB_OPT_ACREGMIN],
[Mount Table option string: Min attr cache timeout (files)])

AH_TEMPLATE([MNTTAB_OPT_ACREGMAX],
[Mount Table option string: Max attr cache timeout (files)])

AH_TEMPLATE([MNTTAB_OPT_ACDIRMIN],
[Mount Table option string: Min attr cache timeout (dirs)])

AH_TEMPLATE([MNTTAB_OPT_ACDIRMAX],
[Mount Table option string: Max attr cache timeout (dirs)])

AH_TEMPLATE([MNTTAB_OPT_NOAC],
[Mount Table option string: Don't cache attributes at all])

AH_TEMPLATE([MNTTAB_OPT_NOCTO],
[Mount Table option string: No close-to-open consistency])

AH_TEMPLATE([MNTTAB_OPT_BG],
[Mount Table option string: Do mount retries in background])

AH_TEMPLATE([MNTTAB_OPT_FG],
[Mount Table option string: Do mount retries in foreground])

AH_TEMPLATE([MNTTAB_OPT_RETRY],
[Mount Table option string: Number of mount retries])

AH_TEMPLATE([MNTTAB_OPT_DEV],
[Mount Table option string: Device id of mounted fs])

AH_TEMPLATE([MNTTAB_OPT_FSID],
[Mount Table option string: Filesystem id of mounted fs])

AH_TEMPLATE([MNTTAB_OPT_POSIX],
[Mount Table option string: Get static pathconf for mount])

AH_TEMPLATE([MNTTAB_OPT_MAP],
[Mount Table option string: Automount map])

AH_TEMPLATE([MNTTAB_OPT_DIRECT],
[Mount Table option string: Automount   direct map mount])

AH_TEMPLATE([MNTTAB_OPT_INDIRECT],
[Mount Table option string: Automount indirect map mount])

AH_TEMPLATE([MNTTAB_OPT_LLOCK],
[Mount Table option string: Local locking (no lock manager)])

AH_TEMPLATE([MNTTAB_OPT_IGNORE],
[Mount Table option string: Ignore this entry])

AH_TEMPLATE([MNTTAB_OPT_NOAUTO],
[Mount Table option string: No auto (what?)])

AH_TEMPLATE([MNTTAB_OPT_NOCONN],
[Mount Table option string: No connection])

AH_TEMPLATE([MNTTAB_OPT_VERS],
[Mount Table option string: protocol version number indicator])

AH_TEMPLATE([MNTTAB_OPT_PROTO],
[Mount Table option string: protocol network_id indicator])

AH_TEMPLATE([MNTTAB_OPT_SYNCDIR],
[Mount Table option string: Synchronous local directory ops])

AH_TEMPLATE([MNTTAB_OPT_NOSETSEC],
[Mount Table option string: Do no allow setting sec attrs])

AH_TEMPLATE([MNTTAB_OPT_SYMTTL],
[Mount Table option string: set symlink cache time-to-live])

AH_TEMPLATE([MNTTAB_OPT_COMPRESS],
[Mount Table option string: compress])

AH_TEMPLATE([MNTTAB_OPT_PGTHRESH],
[Mount Table option string: paging threshold])

AH_TEMPLATE([MNTTAB_OPT_MAXGROUPS],
[Mount Table option string: max groups])

AH_TEMPLATE([MNTTAB_OPT_PROPLIST],
[Mount Table option string: support property lists (ACLs)])

AH_TEMPLATE([MNT2_GEN_OPT_ASYNC],
[asynchronous filesystem access])

AH_TEMPLATE([MNT2_GEN_OPT_AUTOMNTFS],
[automounter filesystem (ignore) flag, used in bsdi-4.1])

AH_TEMPLATE([MNT2_GEN_OPT_AUTOMOUNTED],
[automounter filesystem flag, used in Mac OS X / Darwin])

AH_TEMPLATE([MNT2_GEN_OPT_BIND],
[directory hardlink])

AH_TEMPLATE([MNT2_GEN_OPT_CACHE],
[cache (what?)])

AH_TEMPLATE([MNT2_GEN_OPT_DATA],
[6-argument mount])

AH_TEMPLATE([MNT2_GEN_OPT_FSS],
[old (4-argument) mount (compatibility)])

AH_TEMPLATE([MNT2_GEN_OPT_IGNORE],
[ignore mount entry in df output])

AH_TEMPLATE([MNT2_GEN_OPT_JFS],
[journaling filesystem (AIX's UFS/FFS)])

AH_TEMPLATE([MNT2_GEN_OPT_GRPID],
[old BSD group-id on create])

AH_TEMPLATE([MNT2_GEN_OPT_MULTI],
[do multi-component lookup on files])

AH_TEMPLATE([MNT2_GEN_OPT_NEWTYPE],
[use type string instead of int])

AH_TEMPLATE([MNT2_GEN_OPT_NFS],
[NFS mount])

AH_TEMPLATE([MNT2_GEN_OPT_NOCACHE],
[nocache (what?)])

AH_TEMPLATE([MNT2_GEN_OPT_NODEV],
[do not interpret special device files])

AH_TEMPLATE([MNT2_GEN_OPT_NOEXEC],
[no exec calls allowed])

AH_TEMPLATE([MNT2_GEN_OPT_NONDEV],
[do not interpret special device files])

AH_TEMPLATE([MNT2_GEN_OPT_NOSUB],
[Disallow mounts beneath this mount])

AH_TEMPLATE([MNT2_GEN_OPT_NOSUID],
[Setuid programs disallowed])

AH_TEMPLATE([MNT2_GEN_OPT_NOTRUNC],
[Return ENAMETOOLONG for long filenames])

AH_TEMPLATE([MNT2_GEN_OPT_OPTIONSTR],
[Pass mount option string to kernel])

AH_TEMPLATE([MNT2_GEN_OPT_OVERLAY],
[allow overlay mounts])

AH_TEMPLATE([MNT2_GEN_OPT_QUOTA],
[check quotas])

AH_TEMPLATE([MNT2_GEN_OPT_RDONLY],
[Read-only])

AH_TEMPLATE([MNT2_GEN_OPT_REMOUNT],
[change options on an existing mount])

AH_TEMPLATE([MNT2_GEN_OPT_RONLY],
[read only])

AH_TEMPLATE([MNT2_GEN_OPT_SYNC],
[synchronize data immediately to filesystem])

AH_TEMPLATE([MNT2_GEN_OPT_SYNCHRONOUS],
[synchronous filesystem access (same as SYNC)])

AH_TEMPLATE([MNT2_GEN_OPT_SYS5],
[Mount with Sys 5-specific semantics])

AH_TEMPLATE([MNT2_GEN_OPT_UNION],
[Union mount])

AH_TEMPLATE([MNT2_NFS_OPT_AUTO],
[hide mount type from df(1)])

AH_TEMPLATE([MNT2_NFS_OPT_ACDIRMAX],
[set max secs for dir attr cache])

AH_TEMPLATE([MNT2_NFS_OPT_ACDIRMIN],
[set min secs for dir attr cache])

AH_TEMPLATE([MNT2_NFS_OPT_ACREGMAX],
[set max secs for file attr cache])

AH_TEMPLATE([MNT2_NFS_OPT_ACREGMIN],
[set min secs for file attr cache])

AH_TEMPLATE([MNT2_NFS_OPT_AUTHERR],
[Authentication error])

AH_TEMPLATE([MNT2_NFS_OPT_DEADTHRESH],
[set dead server retry thresh])

AH_TEMPLATE([MNT2_NFS_OPT_DISMINPROG],
[Dismount in progress])

AH_TEMPLATE([MNT2_NFS_OPT_DISMNT],
[Dismounted])

AH_TEMPLATE([MNT2_NFS_OPT_DUMBTIMR],
[Don't estimate rtt dynamically])

AH_TEMPLATE([MNT2_NFS_OPT_GRPID],
[System V-style gid inheritance])

AH_TEMPLATE([MNT2_NFS_OPT_HASAUTH],
[Has authenticator])

AH_TEMPLATE([MNT2_NFS_OPT_FSNAME],
[provide name of server's fs to system])

AH_TEMPLATE([MNT2_NFS_OPT_HOSTNAME],
[set hostname for error printf])

AH_TEMPLATE([MNT2_NFS_OPT_IGNORE],
[ignore mount point])

AH_TEMPLATE([MNT2_NFS_OPT_INT],
[allow interrupts on hard mount])

AH_TEMPLATE([MNT2_NFS_OPT_INTR],
[allow interrupts on hard mount])

AH_TEMPLATE([MNT2_NFS_OPT_INTERNAL],
[Bits set internally])

AH_TEMPLATE([MNT2_NFS_OPT_KERB],
[Use Kerberos authentication])

AH_TEMPLATE([MNT2_NFS_OPT_KERBEROS],
[use kerberos credentials])

AH_TEMPLATE([MNT2_NFS_OPT_KNCONF],
[transport's knetconfig structure])

AH_TEMPLATE([MNT2_NFS_OPT_LEASETERM],
[set lease term (nqnfs)])

AH_TEMPLATE([MNT2_NFS_OPT_LLOCK],
[Local locking (no lock manager)])

AH_TEMPLATE([MNT2_NFS_OPT_MAXGRPS],
[set maximum grouplist size])

AH_TEMPLATE([MNT2_NFS_OPT_MNTD],
[Mnt server for mnt point])

AH_TEMPLATE([MNT2_NFS_OPT_MYWRITE],
[Assume writes were mine])

AH_TEMPLATE([MNT2_NFS_OPT_NFSV3],
[mount NFS Version 3])

AH_TEMPLATE([MNT2_NFS_OPT_NOAC],
[don't cache attributes])

AH_TEMPLATE([MNT2_NFS_OPT_NOCONN],
[Don't Connect the socket])

AH_TEMPLATE([MNT2_NFS_OPT_NOCTO],
[no close-to-open consistency])

AH_TEMPLATE([MNT2_NFS_OPT_NOINT],
[disallow interrupts on hard mounts])

AH_TEMPLATE([MNT2_NFS_OPT_NQLOOKLEASE],
[Get lease for lookup])

AH_TEMPLATE([MNT2_NFS_OPT_NONLM],
[Don't use locking])

AH_TEMPLATE([MNT2_NFS_OPT_NQNFS],
[Use Nqnfs protocol])

AH_TEMPLATE([MNT2_NFS_OPT_POSIX],
[static pathconf kludge info])

AH_TEMPLATE([MNT2_NFS_OPT_RCVLOCK],
[Rcv socket lock])

AH_TEMPLATE([MNT2_NFS_OPT_RDIRALOOK],
[Do lookup with readdir (nqnfs)])

AH_TEMPLATE([MNT2_NFS_OPT_PROPLIST],
[allow property list operations (ACLs over NFS)])

AH_TEMPLATE([MNT2_NFS_OPTS_RDIRPLUS],
[Use Readdirplus for NFSv3])

AH_TEMPLATE([MNT2_NFS_OPT_READAHEAD],
[set read ahead])

AH_TEMPLATE([MNT2_NFS_OPT_READDIRSIZE],
[Set readdir size])

AH_TEMPLATE([MNT2_NFS_OPT_RESVPORT],
[Allocate a reserved port])

AH_TEMPLATE([MNT2_NFS_OPT_RETRANS],
[set number of request retries])

AH_TEMPLATE([MNT2_NFS_OPT_RONLY],
[read only])

AH_TEMPLATE([MNT2_NFS_OPT_RPCTIMESYNC],
[use RPC to do secure NFS time sync])

AH_TEMPLATE([MNT2_NFS_OPT_RSIZE],
[set read size])

AH_TEMPLATE([MNT2_NFS_OPT_SECURE],
[secure mount])

AH_TEMPLATE([MNT2_NFS_OPT_SNDLOCK],
[Send socket lock])

AH_TEMPLATE([MNT2_NFS_OPT_SOFT],
[soft mount (hard is default)])

AH_TEMPLATE([MNT2_NFS_OPT_SPONGY],
[spongy mount])

AH_TEMPLATE([MNT2_NFS_OPT_TIMEO],
[set initial timeout])

AH_TEMPLATE([MNT2_NFS_OPT_TCP],
[use TCP for mounts])

AH_TEMPLATE([MNT2_NFS_OPT_VER3],
[linux NFSv3])

AH_TEMPLATE([MNT2_NFS_OPT_WAITAUTH],
[Wait for authentication])

AH_TEMPLATE([MNT2_NFS_OPT_WANTAUTH],
[Wants an authenticator])

AH_TEMPLATE([MNT2_NFS_OPT_WANTRCV],
[Want receive socket lock])

AH_TEMPLATE([MNT2_NFS_OPT_WANTSND],
[Want send socket lock])

AH_TEMPLATE([MNT2_NFS_OPT_WSIZE],
[set write size])

AH_TEMPLATE([MNT2_NFS_OPT_SYMTTL],
[set symlink cache time-to-live])

AH_TEMPLATE([MNT2_NFS_OPT_PGTHRESH],
[paging threshold])

AH_TEMPLATE([MNT2_NFS_OPT_XLATECOOKIE],
[32<->64 dir cookie translation])

AH_TEMPLATE([MNT2_CDFS_OPT_DEFPERM],
[Ignore permission bits])

AH_TEMPLATE([MNT2_CDFS_OPT_NODEFPERM],
[Use on-disk permission bits])

AH_TEMPLATE([MNT2_CDFS_OPT_NOVERSION],
[Strip off extension from version string])

AH_TEMPLATE([MNT2_CDFS_OPT_RRIP],
[Use Rock Ridge Interchange Protocol (RRIP) extensions])

AH_TEMPLATE([HAVE_MNTENT_T_MNT_TIME_STRING],
[does mntent_t have mnt_time field and is of type "char *" ?])

AH_TEMPLATE([REINSTALL_SIGNAL_HANDLER],
[should signal handlers be reinstalled?])

AH_TEMPLATE([DEBUG],
[Turn off general debugging by default])

AH_TEMPLATE([DEBUG_MEM],
[Turn off memory debugging by default])

AH_TEMPLATE([PACKAGE_NAME],
[Define package name (must be defined by configure.in)])

AH_TEMPLATE([PACKAGE_VERSION],
[Define version of package (must be defined by configure.in)])

AH_TEMPLATE([PACKAGE_BUGREPORT],
[Define bug-reporting address (must be defined by configure.in)])

AH_TEMPLATE([HOST_CPU],
[Define name of host machine's cpu (eg. sparc)])

AH_TEMPLATE([HOST_ARCH],
[Define name of host machine's architecture (eg. sun4)])

AH_TEMPLATE([HOST_VENDOR],
[Define name of host machine's vendor (eg. sun)])

AH_TEMPLATE([HOST_OS],
[Define name and version of host machine (eg. solaris2.5.1)])

AH_TEMPLATE([HOST_OS_NAME],
[Define only name of host machine OS (eg. solaris2)])

AH_TEMPLATE([HOST_OS_VERSION],
[Define only version of host machine (eg. 2.5.1)])

AH_TEMPLATE([HOST_HEADER_VERSION],
[Define the header version of (linux) hosts (eg. 2.2.10)])

AH_TEMPLATE([HOST_NAME],
[Define name of host])

AH_TEMPLATE([USER_NAME],
[Define user name])

AH_TEMPLATE([CONFIG_DATE],
[Define configuration date])

AH_TEMPLATE([HAVE_TRANSPORT_TYPE_TLI],
[what type of network transport type is in use?  TLI or sockets?])

AH_TEMPLATE([time_t],
[Define to `long' if <sys/types.h> doesn't define time_t])

AH_TEMPLATE([voidp],
[Define to "void *" if compiler can handle, otherwise "char *"])

AH_TEMPLATE([am_nfs_fh],
[Define a type/structure for an NFS V2 filehandle])

AH_TEMPLATE([am_nfs_fh3],
[Define a type/structure for an NFS V3 filehandle])

AH_TEMPLATE([HAVE_NFS_PROT_HEADERS],
[define if the host has NFS protocol headers in system headers])

AH_TEMPLATE([AMU_NFS_PROTOCOL_HEADER],
[define name of am-utils' NFS protocol header])

AH_TEMPLATE([nfs_args_t],
[Define a type for the nfs_args structure])

AH_TEMPLATE([NFS_FH_FIELD],
[Define the field name for the filehandle within nfs_args_t])

AH_TEMPLATE([HAVE_FHANDLE],
[Define if plain fhandle type exists])

AH_TEMPLATE([SVC_IN_ARG_TYPE],
[Define the type of the 3rd argument ('in') to svc_getargs()])

AH_TEMPLATE([XDRPROC_T_TYPE],
[Define to the type of xdr procedure type])

AH_TEMPLATE([MOUNT_TABLE_ON_FILE],
[Define if mount table is on file, undefine if in kernel])

AH_TEMPLATE([HAVE_STRUCT_MNTENT],
[Define if have struct mntent in one of the standard headers])

AH_TEMPLATE([HAVE_STRUCT_MNTTAB],
[Define if have struct mnttab in one of the standard headers])

AH_TEMPLATE([HAVE_STRUCT_NFS_ARGS],
[Define if have struct nfs_args in one of the standard nfs headers])

AH_TEMPLATE([HAVE_STRUCT_NFS_GFS_MOUNT],
[Define if have struct nfs_gfs_mount in one of the standard nfs headers])

AH_TEMPLATE([YP_ORDER_OUTORDER_TYPE],
[Type of the 3rd argument to yp_order()])

AH_TEMPLATE([RECVFROM_FROMLEN_TYPE],
[Type of the 6th argument to recvfrom()])

AH_TEMPLATE([AUTH_CREATE_GIDLIST_TYPE],
[Type of the 5rd argument to authunix_create()])

AH_TEMPLATE([MTYPE_PRINTF_TYPE],
[The string used in printf to print the mount-type field of mount(2)])

AH_TEMPLATE([MTYPE_TYPE],
[Type of the mount-type field in the mount() system call])

AH_TEMPLATE([pcfs_args_t],
[Define a type for the pcfs_args structure])

AH_TEMPLATE([autofs_args_t],
[Define a type for the autofs_args structure])

AH_TEMPLATE([cachefs_args_t],
[Define a type for the cachefs_args structure])

AH_TEMPLATE([tmpfs_args_t],
[Define a type for the tmpfs_args structure])

AH_TEMPLATE([ufs_args_t],
[Define a type for the ufs_args structure])

AH_TEMPLATE([efs_args_t],
[Define a type for the efs_args structure])

AH_TEMPLATE([xfs_args_t],
[Define a type for the xfs_args structure])

AH_TEMPLATE([lofs_args_t],
[Define a type for the lofs_args structure])

AH_TEMPLATE([cdfs_args_t],
[Define a type for the cdfs_args structure])

AH_TEMPLATE([mfs_args_t],
[Define a type for the mfs_args structure])

AH_TEMPLATE([rfs_args_t],
[Define a type for the rfs_args structure])

AH_TEMPLATE([HAVE_BAD_MEMCMP],
[define if have a bad version of memcmp()])

AH_TEMPLATE([HAVE_BAD_YP_ALL],
[define if have a bad version of yp_all()])

AH_TEMPLATE([USE_UNCONNECTED_NFS_SOCKETS],
[define if must use NFS "noconn" option])

AH_TEMPLATE([USE_CONNECTED_NFS_SOCKETS],
[define if must NOT use NFS "noconn" option])

AH_TEMPLATE([HAVE_GNU_GETOPT],
[define if your system's getopt() is GNU getopt() (are you using glibc)])

AH_TEMPLATE([HAVE_EXTERN_SYS_ERRLIST],
[does extern definition for sys_errlist[] exist?])

AH_TEMPLATE([HAVE_EXTERN_OPTARG],
[does extern definition for optarg exist?])

AH_TEMPLATE([HAVE_EXTERN_CLNT_SPCREATEERROR],
[does extern definition for clnt_spcreateerror() exist?])

AH_TEMPLATE([HAVE_EXTERN_CLNT_SPERRNO],
[does extern definition for clnt_sperrno() exist?])

AH_TEMPLATE([HAVE_EXTERN_FREE],
[does extern definition for free() exist?])

AH_TEMPLATE([HAVE_EXTERN_GET_MYADDRESS],
[does extern definition for get_myaddress() exist?])

AH_TEMPLATE([HAVE_EXTERN_GETCCENT],
[does extern definition for getccent() (hpux) exist?])

AH_TEMPLATE([HAVE_EXTERN_GETDOMAINNAME],
[does extern definition for getdomainname() exist?])

AH_TEMPLATE([HAVE_EXTERN_GETHOSTNAME],
[does extern definition for gethostname() exist?])

AH_TEMPLATE([HAVE_EXTERN_GETLOGIN],
[does extern definition for getlogin() exist?])

AH_TEMPLATE([HAVE_EXTERN_GETTABLESIZE],
[does extern definition for gettablesize() exist?])

AH_TEMPLATE([HAVE_EXTERN_GETPAGESIZE],
[does extern definition for getpagesize() exist?])

AH_TEMPLATE([HAVE_EXTERN_INNETGR],
[does extern definition for innetgr() exist?])

AH_TEMPLATE([HAVE_EXTERN_MKSTEMP],
[does extern definition for mkstemp() exist?])

AH_TEMPLATE([HAVE_EXTERN_SBRK],
[does extern definition for sbrk() exist?])

AH_TEMPLATE([HAVE_EXTERN_SETEUID],
[does extern definition for seteuid() exist?])

AH_TEMPLATE([HAVE_EXTERN_SETITIMER],
[does extern definition for setitimer() exist?])

AH_TEMPLATE([HAVE_EXTERN_STRCASECMP],
[does extern definition for strcasecmp() exist?])

AH_TEMPLATE([HAVE_EXTERN_STRDUP],
[does extern definition for strdup() exist?])

AH_TEMPLATE([HAVE_EXTERN_STRSTR],
[does extern definition for strstr() exist?])

AH_TEMPLATE([HAVE_EXTERN_USLEEP],
[does extern definition for usleep() exist?])

AH_TEMPLATE([HAVE_EXTERN_WAIT3],
[does extern definition for wait3() exist?])

AH_TEMPLATE([HAVE_EXTERN_VSNPRINTF],
[does extern definition for vsnprintf() exist?])

AH_TEMPLATE([HAVE_EXTERN_XDR_CALLMSG],
[does extern definition for xdr_callmsg() exist?])

AH_TEMPLATE([HAVE_EXTERN_XDR_OPAQUE_AUTH],
[does extern definition for xdr_opaque_auth() exist?])
