///////////////////////////////////////////////////////////////////////////////
//
/// \file       file_io.c
/// \brief      File opening, unlinking, and closing
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "private.h"

#include <fcntl.h>

#ifdef TUKLIB_DOSLIKE
#	include <io.h>
#else
static bool warn_fchown;
#endif

#if defined(HAVE_FUTIMES) || defined(HAVE_FUTIMESAT) || defined(HAVE_UTIMES)
#	include <sys/time.h>
#elif defined(HAVE_UTIME)
#	include <utime.h>
#endif

#include "tuklib_open_stdxxx.h"

#ifndef O_BINARY
#	define O_BINARY 0
#endif

#ifndef O_NOCTTY
#	define O_NOCTTY 0
#endif


/// If true, try to create sparse files when decompressing.
static bool try_sparse = true;

#ifndef TUKLIB_DOSLIKE
/// Original file status flags of standard output. This is used by
/// io_open_dest() and io_close_dest() to save and restore the flags.
static int stdout_flags;
static bool restore_stdout_flags = false;
#endif


static bool io_write_buf(file_pair *pair, const uint8_t *buf, size_t size);


extern void
io_init(void)
{
	// Make sure that stdin, stdout, and stderr are connected to
	// a valid file descriptor. Exit immediately with exit code ERROR
	// if we cannot make the file descriptors valid. Maybe we should
	// print an error message, but our stderr could be screwed anyway.
	tuklib_open_stdxxx(E_ERROR);

#ifndef TUKLIB_DOSLIKE
	// If fchown() fails setting the owner, we warn about it only if
	// we are root.
	warn_fchown = geteuid() == 0;
#endif

#ifdef __DJGPP__
	// Avoid doing useless things when statting files.
	// This isn't important but doesn't hurt.
	_djstat_flags = _STAT_INODE | _STAT_EXEC_EXT
			| _STAT_EXEC_MAGIC | _STAT_DIRSIZE;
#endif

	return;
}


extern void
io_no_sparse(void)
{
	try_sparse = false;
	return;
}


/// \brief      Unlink a file
///
/// This tries to verify that the file being unlinked really is the file that
/// we want to unlink by verifying device and inode numbers. There's still
/// a small unavoidable race, but this is much better than nothing (the file
/// could have been moved/replaced even hours earlier).
static void
io_unlink(const char *name, const struct stat *known_st)
{
#if defined(TUKLIB_DOSLIKE)
	// On DOS-like systems, st_ino is meaningless, so don't bother
	// testing it. Just silence a compiler warning.
	(void)known_st;
#else
	struct stat new_st;

	// If --force was used, use stat() instead of lstat(). This way
	// (de)compressing symlinks works correctly. However, it also means
	// that xz cannot detect if a regular file foo is renamed to bar
	// and then a symlink foo -> bar is created. Because of stat()
	// instead of lstat(), xz will think that foo hasn't been replaced
	// with another file. Thus, xz will remove foo even though it no
	// longer is the same file that xz used when it started compressing.
	// Probably it's not too bad though, so this doesn't need a more
	// complex fix.
	const int stat_ret = opt_force
			? stat(name, &new_st) : lstat(name, &new_st);

	if (stat_ret
#	ifdef __VMS
			// st_ino is an array, and we don't want to
			// compare st_dev at all.
			|| memcmp(&new_st.st_ino, &known_st->st_ino,
				sizeof(new_st.st_ino)) != 0
#	else
			// Typical POSIX-like system
			|| new_st.st_dev != known_st->st_dev
			|| new_st.st_ino != known_st->st_ino
#	endif
			)
		// TRANSLATORS: When compression or decompression finishes,
		// and xz is going to remove the source file, xz first checks
		// if the source file still exists, and if it does, does its
		// device and inode numbers match what xz saw when it opened
		// the source file. If these checks fail, this message is
		// shown, %s being the filename, and the file is not deleted.
		// The check for device and inode numbers is there, because
		// it is possible that the user has put a new file in place
		// of the original file, and in that case it obviously
		// shouldn't be removed.
		message_error(_("%s: File seems to have been moved, "
				"not removing"), name);
	else
#endif
		// There's a race condition between lstat() and unlink()
		// but at least we have tried to avoid removing wrong file.
		if (unlink(name))
			message_error(_("%s: Cannot remove: %s"),
					name, strerror(errno));

	return;
}


/// \brief      Copies owner/group and permissions
///
/// \todo       ACL and EA support
///
static void
io_copy_attrs(const file_pair *pair)
{
	// Skip chown and chmod on Windows.
#ifndef TUKLIB_DOSLIKE
	// This function is more tricky than you may think at first.
	// Blindly copying permissions may permit users to access the
	// destination file who didn't have permission to access the
	// source file.

	// Try changing the owner of the file. If we aren't root or the owner
	// isn't already us, fchown() probably doesn't succeed. We warn
	// about failing fchown() only if we are root.
	if (fchown(pair->dest_fd, pair->src_st.st_uid, -1) && warn_fchown)
		message_warning(_("%s: Cannot set the file owner: %s"),
				pair->dest_name, strerror(errno));

	mode_t mode;

	if (fchown(pair->dest_fd, -1, pair->src_st.st_gid)) {
		message_warning(_("%s: Cannot set the file group: %s"),
				pair->dest_name, strerror(errno));
		// We can still safely copy some additional permissions:
		// `group' must be at least as strict as `other' and
		// also vice versa.
		//
		// NOTE: After this, the owner of the source file may
		// get additional permissions. This shouldn't be too bad,
		// because the owner would have had permission to chmod
		// the original file anyway.
		mode = ((pair->src_st.st_mode & 0070) >> 3)
				& (pair->src_st.st_mode & 0007);
		mode = (pair->src_st.st_mode & 0700) | (mode << 3) | mode;
	} else {
		// Drop the setuid, setgid, and sticky bits.
		mode = pair->src_st.st_mode & 0777;
	}

	if (fchmod(pair->dest_fd, mode))
		message_warning(_("%s: Cannot set the file permissions: %s"),
				pair->dest_name, strerror(errno));
#endif

	// Copy the timestamps. We have several possible ways to do this, of
	// which some are better in both security and precision.
	//
	// First, get the nanosecond part of the timestamps. As of writing,
	// it's not standardized by POSIX, and there are several names for
	// the same thing in struct stat.
	long atime_nsec;
	long mtime_nsec;

#	if defined(HAVE_STRUCT_STAT_ST_ATIM_TV_NSEC)
	// GNU and Solaris
	atime_nsec = pair->src_st.st_atim.tv_nsec;
	mtime_nsec = pair->src_st.st_mtim.tv_nsec;

#	elif defined(HAVE_STRUCT_STAT_ST_ATIMESPEC_TV_NSEC)
	// BSD
	atime_nsec = pair->src_st.st_atimespec.tv_nsec;
	mtime_nsec = pair->src_st.st_mtimespec.tv_nsec;

#	elif defined(HAVE_STRUCT_STAT_ST_ATIMENSEC)
	// GNU and BSD without extensions
	atime_nsec = pair->src_st.st_atimensec;
	mtime_nsec = pair->src_st.st_mtimensec;

#	elif defined(HAVE_STRUCT_STAT_ST_UATIME)
	// Tru64
	atime_nsec = pair->src_st.st_uatime * 1000;
	mtime_nsec = pair->src_st.st_umtime * 1000;

#	elif defined(HAVE_STRUCT_STAT_ST_ATIM_ST__TIM_TV_NSEC)
	// UnixWare
	atime_nsec = pair->src_st.st_atim.st__tim.tv_nsec;
	mtime_nsec = pair->src_st.st_mtim.st__tim.tv_nsec;

#	else
	// Safe fallback
	atime_nsec = 0;
	mtime_nsec = 0;
#	endif

	// Construct a structure to hold the timestamps and call appropriate
	// function to set the timestamps.
#if defined(HAVE_FUTIMENS)
	// Use nanosecond precision.
	struct timespec tv[2];
	tv[0].tv_sec = pair->src_st.st_atime;
	tv[0].tv_nsec = atime_nsec;
	tv[1].tv_sec = pair->src_st.st_mtime;
	tv[1].tv_nsec = mtime_nsec;

	(void)futimens(pair->dest_fd, tv);

#elif defined(HAVE_FUTIMES) || defined(HAVE_FUTIMESAT) || defined(HAVE_UTIMES)
	// Use microsecond precision.
	struct timeval tv[2];
	tv[0].tv_sec = pair->src_st.st_atime;
	tv[0].tv_usec = atime_nsec / 1000;
	tv[1].tv_sec = pair->src_st.st_mtime;
	tv[1].tv_usec = mtime_nsec / 1000;

#	if defined(HAVE_FUTIMES)
	(void)futimes(pair->dest_fd, tv);
#	elif defined(HAVE_FUTIMESAT)
	(void)futimesat(pair->dest_fd, NULL, tv);
#	else
	// Argh, no function to use a file descriptor to set the timestamp.
	(void)utimes(pair->dest_name, tv);
#	endif

#elif defined(HAVE_UTIME)
	// Use one-second precision. utime() doesn't support using file
	// descriptor either. Some systems have broken utime() prototype
	// so don't make this const.
	struct utimbuf buf = {
		.actime = pair->src_st.st_atime,
		.modtime = pair->src_st.st_mtime,
	};

	// Avoid warnings.
	(void)atime_nsec;
	(void)mtime_nsec;

	(void)utime(pair->dest_name, &buf);
#endif

	return;
}


/// Opens the source file. Returns false on success, true on error.
static bool
io_open_src_real(file_pair *pair)
{
	// There's nothing to open when reading from stdin.
	if (pair->src_name == stdin_filename) {
		pair->src_fd = STDIN_FILENO;
#ifdef TUKLIB_DOSLIKE
		setmode(STDIN_FILENO, O_BINARY);
#endif
		return false;
	}

	// Symlinks are not followed unless writing to stdout or --force
	// was used.
	const bool follow_symlinks = opt_stdout || opt_force;

	// We accept only regular files if we are writing the output
	// to disk too. bzip2 allows overriding this with --force but
	// gzip and xz don't.
	const bool reg_files_only = !opt_stdout;

	// Flags for open()
	int flags = O_RDONLY | O_BINARY | O_NOCTTY;

#ifndef TUKLIB_DOSLIKE
	// If we accept only regular files, we need to be careful to avoid
	// problems with special files like devices and FIFOs. O_NONBLOCK
	// prevents blocking when opening such files. When we want to accept
	// special files, we must not use O_NONBLOCK, or otherwise we won't
	// block waiting e.g. FIFOs to become readable.
	if (reg_files_only)
		flags |= O_NONBLOCK;
#endif

#if defined(O_NOFOLLOW)
	if (!follow_symlinks)
		flags |= O_NOFOLLOW;
#elif !defined(TUKLIB_DOSLIKE)
	// Some POSIX-like systems lack O_NOFOLLOW (it's not required
	// by POSIX). Check for symlinks with a separate lstat() on
	// these systems.
	if (!follow_symlinks) {
		struct stat st;
		if (lstat(pair->src_name, &st)) {
			message_error("%s: %s", pair->src_name,
					strerror(errno));
			return true;

		} else if (S_ISLNK(st.st_mode)) {
			message_warning(_("%s: Is a symbolic link, "
					"skipping"), pair->src_name);
			return true;
		}
	}
#else
	// Avoid warnings.
	(void)follow_symlinks;
#endif

	// Try to open the file. If we are accepting non-regular files,
	// unblock the caught signals so that open() can be interrupted
	// if it blocks e.g. due to a FIFO file.
	if (!reg_files_only)
		signals_unblock();

	// Maybe this wouldn't need a loop, since all the signal handlers for
	// which we don't use SA_RESTART set user_abort to true. But it
	// doesn't hurt to have it just in case.
	do {
		pair->src_fd = open(pair->src_name, flags);
	} while (pair->src_fd == -1 && errno == EINTR && !user_abort);

	if (!reg_files_only)
		signals_block();

	if (pair->src_fd == -1) {
		// If we were interrupted, don't display any error message.
		if (errno == EINTR) {
			// All the signals that don't have SA_RESTART
			// set user_abort.
			assert(user_abort);
			return true;
		}

#ifdef O_NOFOLLOW
		// Give an understandable error message if the reason
		// for failing was that the file was a symbolic link.
		//
		// Note that at least Linux, OpenBSD, Solaris, and Darwin
		// use ELOOP to indicate that O_NOFOLLOW was the reason
		// that open() failed. Because there may be
		// directories in the pathname, ELOOP may occur also
		// because of a symlink loop in the directory part.
		// So ELOOP doesn't tell us what actually went wrong,
		// and this stupidity went into POSIX-1.2008 too.
		//
		// FreeBSD associates EMLINK with O_NOFOLLOW and
		// Tru64 uses ENOTSUP. We use these directly here
		// and skip the lstat() call and the associated race.
		// I want to hear if there are other kernels that
		// fail with something else than ELOOP with O_NOFOLLOW.
		bool was_symlink = false;

#	if defined(__FreeBSD__) || defined(__DragonFly__)
		if (errno == EMLINK)
			was_symlink = true;

#	elif defined(__digital__) && defined(__unix__)
		if (errno == ENOTSUP)
			was_symlink = true;

#	elif defined(__NetBSD__)
		if (errno == EFTYPE)
			was_symlink = true;

#	else
		if (errno == ELOOP && !follow_symlinks) {
			const int saved_errno = errno;
			struct stat st;
			if (lstat(pair->src_name, &st) == 0
					&& S_ISLNK(st.st_mode))
				was_symlink = true;

			errno = saved_errno;
		}
#	endif

		if (was_symlink)
			message_warning(_("%s: Is a symbolic link, "
					"skipping"), pair->src_name);
		else
#endif
			// Something else than O_NOFOLLOW failing
			// (assuming that the race conditions didn't
			// confuse us).
			message_error("%s: %s", pair->src_name,
					strerror(errno));

		return true;
	}

#ifndef TUKLIB_DOSLIKE
	// Drop O_NONBLOCK, which is used only when we are accepting only
	// regular files. After the open() call, we want things to block
	// instead of giving EAGAIN.
	if (reg_files_only) {
		flags = fcntl(pair->src_fd, F_GETFL);
		if (flags == -1)
			goto error_msg;

		flags &= ~O_NONBLOCK;

		if (fcntl(pair->src_fd, F_SETFL, flags) == -1)
			goto error_msg;
	}
#endif

	// Stat the source file. We need the result also when we copy
	// the permissions, and when unlinking.
	if (fstat(pair->src_fd, &pair->src_st))
		goto error_msg;

	if (S_ISDIR(pair->src_st.st_mode)) {
		message_warning(_("%s: Is a directory, skipping"),
				pair->src_name);
		goto error;
	}

	if (reg_files_only && !S_ISREG(pair->src_st.st_mode)) {
		message_warning(_("%s: Not a regular file, skipping"),
				pair->src_name);
		goto error;
	}

#ifndef TUKLIB_DOSLIKE
	if (reg_files_only && !opt_force) {
		if (pair->src_st.st_mode & (S_ISUID | S_ISGID)) {
			// gzip rejects setuid and setgid files even
			// when --force was used. bzip2 doesn't check
			// for them, but calls fchown() after fchmod(),
			// and many systems automatically drop setuid
			// and setgid bits there.
			//
			// We accept setuid and setgid files if
			// --force was used. We drop these bits
			// explicitly in io_copy_attr().
			message_warning(_("%s: File has setuid or "
					"setgid bit set, skipping"),
					pair->src_name);
			goto error;
		}

		if (pair->src_st.st_mode & S_ISVTX) {
			message_warning(_("%s: File has sticky bit "
					"set, skipping"),
					pair->src_name);
			goto error;
		}

		if (pair->src_st.st_nlink > 1) {
			message_warning(_("%s: Input file has more "
					"than one hard link, "
					"skipping"), pair->src_name);
			goto error;
		}
	}
#endif

	return false;

error_msg:
	message_error("%s: %s", pair->src_name, strerror(errno));
error:
	(void)close(pair->src_fd);
	return true;
}


extern file_pair *
io_open_src(const char *src_name)
{
	if (is_empty_filename(src_name))
		return NULL;

	// Since we have only one file open at a time, we can use
	// a statically allocated structure.
	static file_pair pair;

	pair = (file_pair){
		.src_name = src_name,
		.dest_name = NULL,
		.src_fd = -1,
		.dest_fd = -1,
		.src_eof = false,
		.dest_try_sparse = false,
		.dest_pending_sparse = 0,
	};

	// Block the signals, for which we have a custom signal handler, so
	// that we don't need to worry about EINTR.
	signals_block();
	const bool error = io_open_src_real(&pair);
	signals_unblock();

	return error ? NULL : &pair;
}


/// \brief      Closes source file of the file_pair structure
///
/// \param      pair    File whose src_fd should be closed
/// \param      success If true, the file will be removed from the disk if
///                     closing succeeds and --keep hasn't been used.
static void
io_close_src(file_pair *pair, bool success)
{
	if (pair->src_fd != STDIN_FILENO && pair->src_fd != -1) {
#ifdef TUKLIB_DOSLIKE
		(void)close(pair->src_fd);
#endif

		// If we are going to unlink(), do it before closing the file.
		// This way there's no risk that someone replaces the file and
		// happens to get same inode number, which would make us
		// unlink() wrong file.
		//
		// NOTE: DOS-like systems are an exception to this, because
		// they don't allow unlinking files that are open. *sigh*
		if (success && !opt_keep_original)
			io_unlink(pair->src_name, &pair->src_st);

#ifndef TUKLIB_DOSLIKE
		(void)close(pair->src_fd);
#endif
	}

	return;
}


static bool
io_open_dest_real(file_pair *pair)
{
	if (opt_stdout || pair->src_fd == STDIN_FILENO) {
		// We don't modify or free() this.
		pair->dest_name = (char *)"(stdout)";
		pair->dest_fd = STDOUT_FILENO;
#ifdef TUKLIB_DOSLIKE
		setmode(STDOUT_FILENO, O_BINARY);
#endif
	} else {
		pair->dest_name = suffix_get_dest_name(pair->src_name);
		if (pair->dest_name == NULL)
			return true;

		// If --force was used, unlink the target file first.
		if (opt_force && unlink(pair->dest_name) && errno != ENOENT) {
			message_error(_("%s: Cannot remove: %s"),
					pair->dest_name, strerror(errno));
			free(pair->dest_name);
			return true;
		}

		// Open the file.
		const int flags = O_WRONLY | O_BINARY | O_NOCTTY
				| O_CREAT | O_EXCL;
		const mode_t mode = S_IRUSR | S_IWUSR;
		pair->dest_fd = open(pair->dest_name, flags, mode);

		if (pair->dest_fd == -1) {
			message_error("%s: %s", pair->dest_name,
					strerror(errno));
			free(pair->dest_name);
			return true;
		}
	}

	// If this really fails... well, we have a safe fallback.
	if (fstat(pair->dest_fd, &pair->dest_st)) {
#if defined(__VMS)
		pair->dest_st.st_ino[0] = 0;
		pair->dest_st.st_ino[1] = 0;
		pair->dest_st.st_ino[2] = 0;
#elif !defined(TUKLIB_DOSLIKE)
		pair->dest_st.st_dev = 0;
		pair->dest_st.st_ino = 0;
#endif
#ifndef TUKLIB_DOSLIKE
	} else if (try_sparse && opt_mode == MODE_DECOMPRESS) {
		// When writing to standard output, we need to be extra
		// careful:
		//  - It may be connected to something else than
		//    a regular file.
		//  - We aren't necessarily writing to a new empty file
		//    or to the end of an existing file.
		//  - O_APPEND may be active.
		//
		// TODO: I'm keeping this disabled for DOS-like systems
		// for now. FAT doesn't support sparse files, but NTFS
		// does, so maybe this should be enabled on Windows after
		// some testing.
		if (pair->dest_fd == STDOUT_FILENO) {
			if (!S_ISREG(pair->dest_st.st_mode))
				return false;

			stdout_flags = fcntl(STDOUT_FILENO, F_GETFL);
			if (stdout_flags == -1)
				return false;

			if (stdout_flags & O_APPEND) {
				// Creating a sparse file is not possible
				// when O_APPEND is active (it's used by
				// shell's >> redirection). As I understand
				// it, it is safe to temporarily disable
				// O_APPEND in xz, because if someone
				// happened to write to the same file at the
				// same time, results would be bad anyway
				// (users shouldn't assume that xz uses any
				// specific block size when writing data).
				//
				// The write position may be something else
				// than the end of the file, so we must fix
				// it to start writing at the end of the file
				// to imitate O_APPEND.
				if (lseek(STDOUT_FILENO, 0, SEEK_END) == -1)
					return false;

				if (fcntl(STDOUT_FILENO, F_SETFL,
						stdout_flags & ~O_APPEND)
						== -1)
					return false;

				// Disabling O_APPEND succeeded. Mark
				// that the flags should be restored
				// in io_close_dest().
				restore_stdout_flags = true;

			} else if (lseek(STDOUT_FILENO, 0, SEEK_CUR)
					!= pair->dest_st.st_size) {
				// Writing won't start exactly at the end
				// of the file. We cannot use sparse output,
				// because it would probably corrupt the file.
				return false;
			}
		}

		pair->dest_try_sparse = true;
#endif
	}

	return false;
}


extern bool
io_open_dest(file_pair *pair)
{
	signals_block();
	const bool ret = io_open_dest_real(pair);
	signals_unblock();
	return ret;
}


/// \brief      Closes destination file of the file_pair structure
///
/// \param      pair    File whose dest_fd should be closed
/// \param      success If false, the file will be removed from the disk.
///
/// \return     Zero if closing succeeds. On error, -1 is returned and
///             error message printed.
static bool
io_close_dest(file_pair *pair, bool success)
{
#ifndef TUKLIB_DOSLIKE
	// If io_open_dest() has disabled O_APPEND, restore it here.
	if (restore_stdout_flags) {
		assert(pair->dest_fd == STDOUT_FILENO);

		restore_stdout_flags = false;

		if (fcntl(STDOUT_FILENO, F_SETFL, stdout_flags) == -1) {
			message_error(_("Error restoring the O_APPEND flag "
					"to standard output: %s"),
					strerror(errno));
			return true;
		}
	}
#endif

	if (pair->dest_fd == -1 || pair->dest_fd == STDOUT_FILENO)
		return false;

	if (close(pair->dest_fd)) {
		message_error(_("%s: Closing the file failed: %s"),
				pair->dest_name, strerror(errno));

		// Closing destination file failed, so we cannot trust its
		// contents. Get rid of junk:
		io_unlink(pair->dest_name, &pair->dest_st);
		free(pair->dest_name);
		return true;
	}

	// If the operation using this file wasn't successful, we git rid
	// of the junk file.
	if (!success)
		io_unlink(pair->dest_name, &pair->dest_st);

	free(pair->dest_name);

	return false;
}


extern void
io_close(file_pair *pair, bool success)
{
	// Take care of sparseness at the end of the output file.
	if (success && pair->dest_try_sparse
			&& pair->dest_pending_sparse > 0) {
		// Seek forward one byte less than the size of the pending
		// hole, then write one zero-byte. This way the file grows
		// to its correct size. An alternative would be to use
		// ftruncate() but that isn't portable enough (e.g. it
		// doesn't work with FAT on Linux; FAT isn't that important
		// since it doesn't support sparse files anyway, but we don't
		// want to create corrupt files on it).
		if (lseek(pair->dest_fd, pair->dest_pending_sparse - 1,
				SEEK_CUR) == -1) {
			message_error(_("%s: Seeking failed when trying "
					"to create a sparse file: %s"),
					pair->dest_name, strerror(errno));
			success = false;
		} else {
			const uint8_t zero[1] = { '\0' };
			if (io_write_buf(pair, zero, 1))
				success = false;
		}
	}

	signals_block();

	// Copy the file attributes. We need to skip this if destination
	// file isn't open or it is standard output.
	if (success && pair->dest_fd != -1 && pair->dest_fd != STDOUT_FILENO)
		io_copy_attrs(pair);

	// Close the destination first. If it fails, we must not remove
	// the source file!
	if (io_close_dest(pair, success))
		success = false;

	// Close the source file, and unlink it if the operation using this
	// file pair was successful and we haven't requested to keep the
	// source file.
	io_close_src(pair, success);

	signals_unblock();

	return;
}


extern size_t
io_read(file_pair *pair, io_buf *buf_union, size_t size)
{
	// We use small buffers here.
	assert(size < SSIZE_MAX);

	uint8_t *buf = buf_union->u8;
	size_t left = size;

	while (left > 0) {
		const ssize_t amount = read(pair->src_fd, buf, left);

		if (amount == 0) {
			pair->src_eof = true;
			break;
		}

		if (amount == -1) {
			if (errno == EINTR) {
				if (user_abort)
					return SIZE_MAX;

				continue;
			}

			message_error(_("%s: Read error: %s"),
					pair->src_name, strerror(errno));

			// FIXME Is this needed?
			pair->src_eof = true;

			return SIZE_MAX;
		}

		buf += (size_t)(amount);
		left -= (size_t)(amount);
	}

	return size - left;
}


extern bool
io_pread(file_pair *pair, io_buf *buf, size_t size, off_t pos)
{
	// Using lseek() and read() is more portable than pread() and
	// for us it is as good as real pread().
	if (lseek(pair->src_fd, pos, SEEK_SET) != pos) {
		message_error(_("%s: Error seeking the file: %s"),
				pair->src_name, strerror(errno));
		return true;
	}

	const size_t amount = io_read(pair, buf, size);
	if (amount == SIZE_MAX)
		return true;

	if (amount != size) {
		message_error(_("%s: Unexpected end of file"),
				pair->src_name);
		return true;
	}

	return false;
}


static bool
is_sparse(const io_buf *buf)
{
	assert(IO_BUFFER_SIZE % sizeof(uint64_t) == 0);

	for (size_t i = 0; i < ARRAY_SIZE(buf->u64); ++i)
		if (buf->u64[i] != 0)
			return false;

	return true;
}


static bool
io_write_buf(file_pair *pair, const uint8_t *buf, size_t size)
{
	assert(size < SSIZE_MAX);

	while (size > 0) {
		const ssize_t amount = write(pair->dest_fd, buf, size);
		if (amount == -1) {
			if (errno == EINTR) {
				if (user_abort)
					return true;

				continue;
			}

			// Handle broken pipe specially. gzip and bzip2
			// don't print anything on SIGPIPE. In addition,
			// gzip --quiet uses exit status 2 (warning) on
			// broken pipe instead of whatever raise(SIGPIPE)
			// would make it return. It is there to hide "Broken
			// pipe" message on some old shells (probably old
			// GNU bash).
			//
			// We don't do anything special with --quiet, which
			// is what bzip2 does too. If we get SIGPIPE, we
			// will handle it like other signals by setting
			// user_abort, and get EPIPE here.
			if (errno != EPIPE)
				message_error(_("%s: Write error: %s"),
					pair->dest_name, strerror(errno));

			return true;
		}

		buf += (size_t)(amount);
		size -= (size_t)(amount);
	}

	return false;
}


extern bool
io_write(file_pair *pair, const io_buf *buf, size_t size)
{
	assert(size <= IO_BUFFER_SIZE);

	if (pair->dest_try_sparse) {
		// Check if the block is sparse (contains only zeros). If it
		// sparse, we just store the amount and return. We will take
		// care of actually skipping over the hole when we hit the
		// next data block or close the file.
		//
		// Since io_close() requires that dest_pending_sparse > 0
		// if the file ends with sparse block, we must also return
		// if size == 0 to avoid doing the lseek().
		if (size == IO_BUFFER_SIZE) {
			if (is_sparse(buf)) {
				pair->dest_pending_sparse += size;
				return false;
			}
		} else if (size == 0) {
			return false;
		}

		// This is not a sparse block. If we have a pending hole,
		// skip it now.
		if (pair->dest_pending_sparse > 0) {
			if (lseek(pair->dest_fd, pair->dest_pending_sparse,
					SEEK_CUR) == -1) {
				message_error(_("%s: Seeking failed when "
						"trying to create a sparse "
						"file: %s"), pair->dest_name,
						strerror(errno));
				return true;
			}

			pair->dest_pending_sparse = 0;
		}
	}

	return io_write_buf(pair, buf->u8, size);
}
