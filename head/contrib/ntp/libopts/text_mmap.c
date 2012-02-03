/*
 * $Id: text_mmap.c,v 4.15 2006/11/27 01:52:23 bkorb Exp $
 *
 * Time-stamp:      "2006-09-10 14:50:04 bkorb"
 */

#ifndef MAP_ANONYMOUS
#  ifdef   MAP_ANON
#  define  MAP_ANONYMOUS   MAP_ANON
#  endif
#endif

/*
 *  Some weird systems require that a specifically invalid FD number
 *  get passed in as an argument value.  Which value is that?  Well,
 *  as everybody knows, if open(2) fails, it returns -1, so that must
 *  be the value.  :)
 */
#define AO_INVALID_FD  -1

#define FILE_WRITABLE(_prt,_flg) \
        (   (_prt & PROT_WRITE) \
         && ((_flg & (MAP_SHARED|MAP_PRIVATE)) == MAP_SHARED))
#define MAP_FAILED_PTR ((void*)MAP_FAILED)

/*=export_func  text_mmap
 * private:
 *
 * what:  map a text file with terminating NUL
 *
 * arg:   char const*,  pzFile,  name of the file to map
 * arg:   int,          prot,    mmap protections (see mmap(2))
 * arg:   int,          flags,   mmap flags (see mmap(2))
 * arg:   tmap_info_t*, mapinfo, returned info about the mapping
 *
 * ret-type:   void*
 * ret-desc:   The mmaped data address
 *
 * doc:
 *
 * This routine will mmap a file into memory ensuring that there is at least
 * one @file{NUL} character following the file data.  It will return the
 * address where the file contents have been mapped into memory.  If there is a
 * problem, then it will return @code{MAP_FAILED} and set @file{errno}
 * appropriately.
 *
 * The named file does not exist, @code{stat(2)} will set @file{errno} as it
 * will.  If the file is not a regular file, @file{errno} will be
 * @code{EINVAL}.  At that point, @code{open(2)} is attempted with the access
 * bits set appropriately for the requested @code{mmap(2)} protections and flag
 * bits.  On failure, @file{errno} will be set according to the documentation
 * for @code{open(2)}.  If @code{mmap(2)} fails, @file{errno} will be set as
 * that routine sets it.  If @code{text_mmap} works to this point, a valid
 * address will be returned, but there may still be ``issues''.
 *
 * If the file size is not an even multiple of the system page size, then
 * @code{text_map} will return at this point and @file{errno} will be zero.
 * Otherwise, an anonymous map is attempted.  If not available, then an attempt
 * is made to @code{mmap(2)} @file{/dev/zero}.  If any of these fail, the
 * address of the file's data is returned, bug @code{no} @file{NUL} characters
 * are mapped after the end of the data.
 *
 * see: mmap(2), open(2), stat(2)
 *
 * err: Any error code issued by mmap(2), open(2), stat(2) is possible.
 *      Additionally, if the specified file is not a regular file, then
 *      errno will be set to @code{EINVAL}.
 *
 * example:
 * #include <mylib.h>
 * tmap_info_t mi;
 * int no_nul;
 * void* data = text_mmap( "file", PROT_WRITE, MAP_PRIVATE, &mi );
 * if (data == MAP_FAILED) return;
 * no_nul = (mi.txt_size == mi.txt_full_size);
 * << use the data >>
 * text_munmap( &mi );
=*/
void*
text_mmap( char const* pzFile, int prot, int flags, tmap_info_t* pMI )
{
    memset( pMI, 0, sizeof(*pMI) );
#ifdef HAVE_MMAP
    pMI->txt_zero_fd = -1;
#endif
    pMI->txt_fd = -1;

    /*
     *  Make sure we can stat the regular file.  Save the file size.
     */
    {
        struct stat sb;
        if (stat( pzFile, &sb ) != 0) {
            pMI->txt_errno = errno;
            return MAP_FAILED_PTR;
        }

        if (! S_ISREG( sb.st_mode )) {
            pMI->txt_errno = errno = EINVAL;
            return MAP_FAILED_PTR;
        }

        pMI->txt_size = sb.st_size;
    }

    /*
     *  Map mmap flags and protections into open flags and do the open.
     */
    {
        int o_flag;
        /*
         *  See if we will be updating the file.  If we can alter the memory
         *  and if we share the data and we are *not* copy-on-writing the data,
         *  then our updates will show in the file, so we must open with
         *  write access.
         */
        if (FILE_WRITABLE(prot,flags))
            o_flag = O_RDWR;
        else
            o_flag = O_RDONLY;

        /*
         *  If you're not sharing the file and you are writing to it,
         *  then don't let anyone else have access to the file.
         */
        if (((flags & MAP_SHARED) == 0) && (prot & PROT_WRITE))
            o_flag |= O_EXCL;

        pMI->txt_fd = open( pzFile, o_flag );
    }

    if (pMI->txt_fd == AO_INVALID_FD) {
        pMI->txt_errno = errno;
        return MAP_FAILED_PTR;
    }

#ifdef HAVE_MMAP /* * * * * WITH MMAP * * * * * */
    /*
     *  do the mmap.  If we fail, then preserve errno, close the file and
     *  return the failure.
     */
    pMI->txt_data =
        mmap(NULL, pMI->txt_size+1, prot, flags, pMI->txt_fd, (size_t)0);
    if (pMI->txt_data == MAP_FAILED_PTR) {
        pMI->txt_errno = errno;
        goto fail_return;
    }

    /*
     *  Most likely, everything will turn out fine now.  The only difficult
     *  part at this point is coping with files with sizes that are a multiple
     *  of the page size.  Handling that is what this whole thing is about.
     */
    pMI->txt_zero_fd = -1;
    pMI->txt_errno   = 0;

    {
        void* pNuls;
#ifdef _SC_PAGESIZE
        size_t pgsz = sysconf(_SC_PAGESIZE);
#else
        size_t pgsz = getpagesize();
#endif
        /*
         *  Compute the pagesize rounded mapped memory size.
         *  IF this is not the same as the file size, then there are NUL's
         *  at the end of the file mapping and all is okay.
         */
        pMI->txt_full_size = (pMI->txt_size + (pgsz - 1)) & ~(pgsz - 1);
        if (pMI->txt_size != pMI->txt_full_size)
            return pMI->txt_data;

        /*
         *  Still here?  We have to remap the trailing inaccessible page
         *  either anonymously or to /dev/zero.
         */
        pMI->txt_full_size += pgsz;
#if defined(MAP_ANONYMOUS)
        pNuls = mmap(
                (void*)(((char*)pMI->txt_data) + pMI->txt_size),
                pgsz, PROT_READ|PROT_WRITE,
                MAP_ANONYMOUS|MAP_FIXED|MAP_PRIVATE, AO_INVALID_FD, (size_t)0);

        if (pNuls != MAP_FAILED_PTR)
            return pMI->txt_data;

        pMI->txt_errno = errno;

#elif defined(HAVE_DEV_ZERO)
        pMI->txt_zero_fd = open( "/dev/zero", O_RDONLY );

        if (pMI->txt_zero_fd == AO_INVALID_FD) {
            pMI->txt_errno = errno;

        } else {
            pNuls = mmap(
                    (void*)(((char*)pMI->txt_data) + pMI->txt_size), pgsz,
                    PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED,
                    pMI->txt_zero_fd, 0 );

            if (pNuls != MAP_FAILED_PTR)
                return pMI->txt_data;

            pMI->txt_errno = errno;
            close( pMI->txt_zero_fd );
            pMI->txt_zero_fd = -1;
        }
#endif

        pMI->txt_full_size = pMI->txt_size;
    }

    {
        void* p = AGALOC( pMI->txt_size+1, "file text" );
        memcpy( p, pMI->txt_data, pMI->txt_size );
        ((char*)p)[pMI->txt_size] = NUL;
        munmap(pMI->txt_data, pMI->txt_size );
        pMI->txt_data = p;
    }
    pMI->txt_alloc = 1;
    return pMI->txt_data;

#else /* * * * * * no HAVE_MMAP * * * * * */

    pMI->txt_data = AGALOC( pMI->txt_size+1, "file text" );
    if (pMI->txt_data == NULL) {
        pMI->txt_errno = ENOMEM;
        goto fail_return;
    }

    {
        size_t sz = pMI->txt_size;
        char*  pz = pMI->txt_data;

        while (sz > 0) {
            ssize_t rdct = read( pMI->txt_fd, pz, sz );
            if (rdct <= 0) {
                pMI->txt_errno = errno;
                fprintf( stderr, zFSErrReadFile,
                         errno, strerror( errno ), pzFile );
                free( pMI->txt_data );
                goto fail_return;
            }

            pz += rdct;
            sz -= rdct;
        }

        *pz = NUL;
    }

    /*
     *  We never need a dummy page mapped in
     */
    pMI->txt_zero_fd = -1;
    pMI->txt_errno   = 0;

    return pMI->txt_data;

#endif /* * * * * * no HAVE_MMAP * * * * * */

 fail_return:
    if (pMI->txt_fd >= 0) {
        close( pMI->txt_fd );
        pMI->txt_fd = -1;
    }
    errno = pMI->txt_errno;
    pMI->txt_data = MAP_FAILED_PTR;
    return pMI->txt_data;
}


/*=export_func  text_munmap
 * private:
 *
 * what:  unmap the data mapped in by text_mmap
 *
 * arg:   tmap_info_t*, mapinfo, info about the mapping
 *
 * ret-type:   int
 * ret-desc:   -1 or 0.  @file{errno} will have the error code.
 *
 * doc:
 *
 * This routine will unmap the data mapped in with @code{text_mmap} and close
 * the associated file descriptors opened by that function.
 *
 * see: munmap(2), close(2)
 *
 * err: Any error code issued by munmap(2) or close(2) is possible.
=*/
int
text_munmap( tmap_info_t* pMI )
{
#ifdef HAVE_MMAP
    int res = 0;
    if (pMI->txt_alloc) {
        /*
         *  IF the user has write permission and the text is not mapped private,
         *  then write back any changes.  Hopefully, nobody else has modified
         *  the file in the mean time.
         */
        if (   ((pMI->txt_prot & PROT_WRITE) != 0)
            && ((pMI->txt_flags & MAP_PRIVATE) == 0))  {

            if (lseek(pMI->txt_fd, (size_t)0, SEEK_SET) != 0)
                goto error_return;

            res = (write( pMI->txt_fd, pMI->txt_data, pMI->txt_size ) < 0)
                ? errno : 0;
        }

        AGFREE( pMI->txt_data );
        errno = res;
    } else {
        res = munmap( pMI->txt_data, pMI->txt_full_size );
    }
    if (res != 0)
        goto error_return;

    res = close( pMI->txt_fd );
    if (res != 0)
        goto error_return;

    pMI->txt_fd = -1;
    errno = 0;
    if (pMI->txt_zero_fd != -1) {
        res = close( pMI->txt_zero_fd );
        pMI->txt_zero_fd = -1;
    }

 error_return:
    pMI->txt_errno = errno;
    return res;
#else  /* HAVE_MMAP */

    errno = 0;
    /*
     *  IF the memory is writable *AND* it is not private (copy-on-write)
     *     *AND* the memory is "sharable" (seen by other processes)
     *  THEN rewrite the data.
     */
    if (   FILE_WRITABLE(pMI->txt_prot, pMI->txt_flags)
        && (lseek( pMI->txt_fd, 0, SEEK_SET ) >= 0) ) {
        write( pMI->txt_fd, pMI->txt_data, pMI->txt_size );
    }

    close( pMI->txt_fd );
    pMI->txt_fd = -1;
    pMI->txt_errno = errno;
    free( pMI->txt_data );

    return pMI->txt_errno;
#endif /* HAVE_MMAP */
}

/*
 * Local Variables:
 * mode: C
 * c-file-style: "stroustrup"
 * indent-tabs-mode: nil
 * End:
 * end of autoopts/text_mmap.c */
