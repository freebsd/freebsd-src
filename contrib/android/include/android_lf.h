/* 
 * Macros for file64 functions
 *
 * Android does not support the macro _FILE_OFFSET_BITS=64
 * As of android-21 it does however support many file64 functions
*/

#ifndef ARCHIVE_ANDROID_LF_H_INCLUDED
#define ARCHIVE_ANDROID_LF_H_INCLUDED

#if __ANDROID_API__ > 20

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/vfs.h>

//dirent.h
#define readdir_r readdir64_r
#define readdir readdir64
#define dirent dirent64
//fcntl.h
#define openat openat64
#define open open64
#define mkstemp mkstemp64
//unistd.h
#define lseek lseek64
#define ftruncate ftruncate64
//sys/stat.h
#define fstatat fstatat64
#define fstat fstat64
#define lstat lstat64
#define stat stat64
//sys/statvfs.h
#define fstatvfs fstatvfs64
#define statvfs statvfs64
//sys/types.h
#define off_t off64_t
//sys/vfs.h
#define fstatfs fstatfs64
#define statfs statfs64
#endif

#endif /* ARCHIVE_ANDROID_LF_H_INCLUDED */
