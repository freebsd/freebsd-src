#ifndef __libgxx_sys_dir_h

extern "C" {

#ifdef __sys_dir_h_recursive
#include_next <sys/dir.h>
#else
#define __sys_dir_h_recursive
#define opendir __hide_opendir
#define closedir __hide_closedir
#define readdir __hide_readdir
#define telldir __hide_telldir
#define seekdir __hide_seekdir

#include_next <sys/dir.h>

#define __libgxx_sys_dir_h
#undef opendir
#undef closedir
#undef readdir
#undef telldir
#undef seekdir

DIR *opendir(const char *);
int closedir(DIR *);
#ifdef __dirent_h_recursive
// Some operating systems we won't mention (such as the imitation
// of Unix marketed by IBM) implement dirent.h by including sys/dir.h,
// in which case sys/dir.h defines struct dirent, rather than
// the struct direct originally used by BSD.
struct dirent *readdir(DIR *);
#else
struct direct *readdir(DIR *);
#endif
long telldir(DIR *);
void seekdir(DIR *, long);
// We don't bother with rewinddir (many systems define it as a macro).
// void rewinddir(DIR *);
#endif
}

#endif
