;;;
;;;#ident "@(#)OrigId: pcl-cvs.el,v 1.93 1993/05/31 22:44:00 ceder Exp "
;;;
;;;#ident "@(#)cvs/contrib/pcl-cvs:$Name:  $:$Id: pcl-cvs.el,v 1.7 1998/01/04 14:24:13 kingdon Exp $"
;;;
;;; pcl-cvs.el -- A Front-end to CVS 1.3 or later.
;;; Release 1.05-CVS-$Name:  $.
;;; Copyright (C) 1991, 1992, 1993  Per Cederqvist

;;; This program is free software; you can redistribute it and/or modify
;;; it under the terms of the GNU General Public License as published by
;;; the Free Software Foundation; either version 2 of the License, or
;;; (at your option) any later version.
;;;
;;; This program is distributed in the hope that it will be useful,
;;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;; GNU General Public License for more details.
;;;
;;; You should have received a copy of the GNU General Public License
;;; along with this program; if not, write to the Free Software
;;; Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

;;; See below for installation instructions.

;;; This package requires ELIB-1.0 to run.  Elib is included in the
;;; CVS distribution in the contrib/elib/ subdirectory, but you can
;;; also download it at the following URL:
;;;
;;;       ftp://ftp.lysator.liu.se/pub/emacs/elib-1.0.tar.gz
;;;

;;; There is an TeXinfo file that describes this package.  You should read it
;;; to get the most from this package.

;;; Mail questions and bug reports regarding this version (as included in
;;; CVS-1.7 or newer) to the pcl-cvs support team at <pcl-cvs@cyclic.com>.

;;; Don't try to use this with CVS 1.2 or earlier.  It won't work.  Get CVS 1.7
;;; or newer.  Use the version of RCS best suited for the version of CVS you're
;;; using.

(require 'cookie)			; from ELIB-1.0
(require 'add-log)			; for all the ChangeLog goodies

;;; -------------------------------------------------------
;;;	    START OF THINGS TO CHECK WHEN INSTALLING

;; also use $GNU here, since may folks might install CVS as a GNU package
;;
(defun cvs-find-program (program)
  (let ((path (list (getenv "LOCAL")
		    (getenv "GNU")
		    "/usr/local/bin"
		    "/usr/bin"
		    "/bin")))
    (while path
      (if (stringp (car path))
	  (let ((abs-program (expand-file-name program (car path))))
	    (if (file-executable-p abs-program)
		(setq path nil
		      program abs-program))))
      (setq path (cdr path)))
    program))

(defvar cvs-program (cvs-find-program "cvs")
  "*Full path to the cvs executable.")

;; SunOS-4.1.1_U1 has "diff.c 1.12 88/08/04 SMI; from UCB 4.6 86/04/03"
;;
(defvar cvs-diff-program (cvs-find-program "diff")
  "*Full path to the best diff program you've got.
NOTE:  there are some nasty bugs in the context diff variants of some vendor
versions, such as the one in SunOS-4.1.1_U1")

(defvar cvs-rmdir-program "/bin/rmdir"
  "*Full path to the rmdir program.  Typically /bin/rmdir.")

(defvar cvs-shell "/bin/sh"
  "*Full path to a shell that can do redirection on stdout.")

;;; Options to control various features:

(defvar cvs-changelog-full-paragraphs t
  "If non-nil, include full ChangeLog paragraphs in the CVS log.
This may be set in the ``local variables'' section of a ChangeLog, to
indicate the policy for that ChangeLog.

A ChangeLog paragraph is a bunch of log text containing no blank lines;
a paragraph usually describes a set of changes with a single purpose,
but perhaps spanning several functions in several files.  Changes in
different paragraphs are unrelated.

You could argue that the CVS log entry for a file should contain the
full ChangeLog paragraph mentioning the change to the file, even though
it may mention other files, because that gives you the full context you
need to understand the change.  This is the behaviour you get when this
variable is set to t.

On the other hand, you could argue that the CVS log entry for a change
should contain only the text for the changes which occurred in that
file, because the CVS log is per-file.  This is the behaviour you get
when this variable is set to nil.")

(defvar cvs-cvsroot-required nil
  "*Specifies whether CVS needs to be told where the repository is.

In CVS 1.3, if your CVSROOT environment variable is not set, and you
do not set the `cvs-cvsroot' lisp variable, CVS will have no idea
where to find the repository, and refuse to run.  CVS 1.4 and later
store the repository path with the working directories, so most
operations don't need to be told where the repository is.

If you work with multiple repositories with CVS 1.4, it's probably
advisable to leave your CVSROOT environment variable unset, set this
variable to nil, and let CVS figure out where the repository is for
itself.")

(defvar cvs-cvsroot nil
  "*Specifies where the (current) cvs master repository is.
Overrides the $CVSROOT variable by sending \" -d dir\" to all cvs commands.
This switch is useful if you have multiple CVS repositories, and are not using
a modern version of CVS that stores the current repository in CVS/Root.")

;; Uncomment the following line if you are running on 18.57 or earlier.
;(setq delete-exited-processes nil)
;; Emacs version 18.57 and earlier is likely to crash if
;; delete-exited-processes is t, since the sentinel uses lots of
;; memory, and 18.57 forgets to GCPROT a variable if
;; delete-exited-processes is t.

;;;	     END OF THINGS TO CHECK WHEN INSTALLING
;;; --------------------------------------------------------

(defconst pcl-cvs-version "1.05-CVS-$Name:  $"
  "A string denoting the current release version of pcl-cvs.")

;; You are NOT allowed to disable this message by default.  However, you
;; are encouraged to inform your users that by adding
;;	(setq cvs-inhibit-copyright-message t)
;; to their .emacs they can get rid of it.  Just don't add that line
;; to your default.el!
(defvar cvs-inhibit-copyright-message nil
  "*Non-nil means don't display a Copyright message in the ``*cvs*'' buffer.")

(defconst cvs-startup-message
  (if cvs-inhibit-copyright-message
      "PCL-CVS release 1.05-CVS-$Name:  $"
    "PCL-CVS release 1.05 from CVS release $Name:  $.
Copyright (C) 1992, 1993 Per Cederqvist
Pcl-cvs comes with absolutely no warranty; for details consult the manual.
This is free software, and you are welcome to redistribute it under certain
conditions; again, consult the TeXinfo manual for details.")
  "*Startup message for CVS.")

(defconst pcl-cvs-bugs-address "pcl-cvs-auto-bugs@cyclic.com"
  "The destination address used for the default bug report form.")

(defvar cvs-stdout-file nil
  "Name of the file that holds the output that CVS sends to stdout.
This variable is buffer local.")

(defvar cvs-lock-file nil
  "Full path to a lock file that CVS is waiting for (or was waiting for).")

(defvar cvs-bakprefix ".#"
  "The prefix that CVS prepends to files when rcsmerge'ing.")

(defvar cvs-erase-input-buffer nil
  "*Non-nil if input buffers should be cleared before asking for new info.")

(defvar cvs-auto-remove-handled nil
  "*Non-nil if cvs-mode-remove-handled should be called automatically.
If this is set to any non-nil value, entries that do not need to be checked in
will be removed from the *cvs* buffer after every cvs-mode-commit command.")

(defvar cvs-auto-remove-handled-directories nil
  "*Non-nil if cvs-mode-remove-handled and cvs-update should automatically
remove empty directories.
If this is set to any non-nil value, directories that do not contain any files
to be checked in will be removed from the *cvs* buffer.")

(defvar cvs-sort-ignore-file t
  "*Non-nil if cvs-mode-ignore should sort the .cvsignore automatically.")

(defvar cvs-auto-revert-after-commit t
  "*Non-nil if committed buffers should be automatically reverted.")

(defconst cvs-cursor-column 14
  "Column to position cursor in in cvs-mode.
Column 0 is left-most column.")

(defvar cvs-mode-map nil
  "Keymap for the cvs mode.")

(defvar cvs-edit-mode-map nil
  "Keymap for the cvs edit mode (used when editing cvs log messages).")

(defvar cvs-buffer-name "*cvs*"
  "Name of the cvs buffer.")

(defvar cvs-commit-prompt-buffer "*cvs-commit-message*"
  "Name of buffer in which the user is prompted for a log message when
committing files.")

(defvar cvs-commit-buffer-require-final-newline t
  "*t says silently put a newline at the end of commit log messages.
Non-nil but not t says ask user whether to add a newline in each such case.
nil means don't add newlines.")

(defvar cvs-temp-buffer-name "*cvs-tmp*"
  "*Name of the cvs temporary buffer.
Output from cvs is placed here by synchronous commands.")

(defvar cvs-diff-ignore-marks nil
  "*Non-nil if cvs-diff and cvs-mode-diff-backup should ignore any marked files.
Normally they run diff on the files that are marked (with cvs-mode-mark),
or the file under the cursor if no files are marked.  If this variable
is set to a non-nil value they will always run diff on the file on the
current line.")

;;; (setq cvs-status-flags '("-v"))
(defvar cvs-status-flags '("-v")
  "*List of flags to pass to ``cvs status''.  Default is \"-v\".")

;;; (setq cvs-log-flags nil)
(defvar cvs-log-flags nil
  "*List of flags to pass to ``cvs log''.  Default is none.")

;;; (setq cvs-tag-flags nil)
(defvar cvs-tag-flags nil
  "*List of extra flags to pass to ``cvs tag''.  Default is none.")

;;; (setq cvs-rtag-flags nil)
(defvar cvs-rtag-flags nil
  "*List of extra flags to pass to ``cvs rtag''.  Default is none.")

;;; (setq cvs-diff-flags '("-u"))
(defvar cvs-diff-flags '("-u")
  "*List of flags to use as flags to pass to ``diff'' and ``cvs diff''.
Used by cvs-mode-diff-cvs and cvs-mode-diff-backup.  Default is \"-u\".

Set this to \"-u\" to get a Unidiff format, or \"-c\" to get context diffs.")

;;; (setq cvs-update-optional-flags nil)
(defvar cvs-update-optional-flags nil
  "*List of strings to use as optional flags to pass to ``cvs update''.  Used
by cvs-do-update, called by cvs-update, cvs-update-other-window,
cvs-mode-update-no-prompt, and cvs-examine.  Default is none.

For example set this to \"-j VENDOR_PREV_RELEASE -j VENDOR_TOP_RELEASE\" to
perform an update after a new vendor release has been imported.

To restrict the update to the current working directory, set this to \"-l\".")

(defvar cvs-update-prog-output-skip-regexp "$"
  "*A regexp that matches the end of the output from all cvs update programs.
That is, output from any programs that are run by CVS (by the flag -u in the
`modules' file - see cvs(5)) when `cvs update' is performed should terminate
with a line that this regexp matches.  It is enough that some part of the line
is matched.

The default (a single $) fits programs without output.")

;;; --------------------------------------------------------
;;; The variables below are used internally by pcl-cvs.  You should
;;; never change them.

(defvar cvs-buffers-to-delete nil
  "List of temporary buffers that should be discarded as soon as possible.
Due to a bug in emacs 18.57 the sentinel can't discard them reliably.")

(defvar cvs-update-running nil
  "This is set to nil when no process is running, and to
the process when a cvs update process is running.")

(defvar cvs-cookie-handle nil
  "Handle for the cookie structure that is displayed in the *cvs* buffer.")

(defvar cvs-commit-list nil
  "Used internally by pcl-cvs.")

;;; The cvs data structure:
;;;
;;; When the `cvs update' is ready we parse the output.  Every file
;;; that is affected in some way is added as a cookie of fileinfo
;;; (as defined below).
;;;

;;; cvs-fileinfo

;;; Constructor:

(defun cvs-create-fileinfo (type
			    dir
			    file-name
			    full-log)
  "Create a fileinfo from all parameters.
Arguments:  TYPE DIR FILE-NAME FULL-LOG.
A fileinfo is a vector with the following fields:

[0]  handled	      True if this file doesn't require further action.
[1]  marked	      t/nil
[2]  type	      One of
			UPDATED	   - file copied from repository
			PATCHED	   - file update with patch from repository
			MODIFIED   - modified by you, unchanged in
				     repository
			ADDED	   - added by you, not yet committed
			REMOVED	   - removed by you, not yet committed
			CVS-REMOVED- removed, since file no longer exists
				     in the repository.
			MERGED	   - successful merge
			CONFLICT   - conflict when merging (if pcl-cvs did it)
			REM-CONFLICT-removed in repository, but altered
				     locally.
			MOD-CONFLICT-removed locally, changed in repository.
                        REM-EXIST  - removed locally, but still exists.
			DIRCHANGE  - A change of directory.
			UNKNOWN	   - An unknown file.
			UNKNOWN-DIR- An unknown directory.
			MOVE-AWAY  - A file that is in the way.
			REPOS-MISSING- The directory has vanished from the
				       repository.
                        MESSAGE    - This is a special fileinfo that is used
  				       to display a text that should be in
                                       full-log.
[3]  dir	      Directory the file resides in.  Should not end with slash.
[4]  file-name	      The file name.
[5]  backup-file      The name of a backup file created during a merge.
                        Only valid for MERGED and CONFLICT files.
[6]  base-revision    The revision that the working file was based on.
                        Only valid for MERGED and CONFLICT files.
[7]  head-revision    The revision that the newly merged changes came from
                        Only valid for MERGED and CONFLICT files.
[8]  backup-revision  The revision of the cvs backup file (original working rev.)
                        Only valid for MERGED and CONFLICT files.
[9]  cvs-diff-buffer  A buffer that contains a 'cvs diff file'.
[10] vendor-diff-buffer  A buffer that contains a 'diff base-file head-file'.
[11] backup-diff-buffer  A buffer that contains a 'diff file backup-file'.
[12] full-log	      The output from cvs, unparsed.
[13] mod-time	      Modification time of file used for *-diff-buffer."

  (cons
   'CVS-FILEINFO
   (vector nil nil type dir file-name nil nil nil nil nil nil nil full-log nil nil)))

;;; Selectors:

(defun cvs-fileinfo->handled (cvs-fileinfo)
  "Get the  `handled' field from CVS-FILEINFO."
  (elt (cdr cvs-fileinfo) 0))

(defun cvs-fileinfo->marked (cvs-fileinfo)
  "Check if CVS-FILEINFO is marked."
  (elt (cdr cvs-fileinfo) 1))

(defun cvs-fileinfo->type (cvs-fileinfo)
  "Get type from CVS-FILEINFO.
Type is one of UPDATED, PATCHED, MODIFIED, ADDED, REMOVED, CVS-REMOVED, MERGED,
CONFLICT, REM-CONFLICT, MOD-CONFLICT, REM-EXIST, DIRCHANGE, UNKNOWN,
UNKNOWN-DIR, MOVE-AWAY, REPOS-MISSING or MESSAGE."
  (elt (cdr cvs-fileinfo) 2))

(defun cvs-fileinfo->dir (cvs-fileinfo)
  "Get dir from CVS-FILEINFO.
The directory name does not end with a slash."
  (elt (cdr cvs-fileinfo) 3))

(defun cvs-fileinfo->file-name (cvs-fileinfo)
  "Get file-name from CVS-FILEINFO."
  (elt (cdr cvs-fileinfo) 4))

(defun cvs-fileinfo->backup-file (cvs-fileinfo)
  "Get backup-file from CVS-FILEINFO."
  (elt (cdr cvs-fileinfo) 5))

(defun cvs-fileinfo->base-revision (cvs-fileinfo)
  "Get the base revision from CVS-FILEINFO."
  (elt (cdr cvs-fileinfo) 6))

(defun cvs-fileinfo->head-revision (cvs-fileinfo)
  "Get the head revision from CVS-FILEINFO."
  (elt (cdr cvs-fileinfo) 7))

(defun cvs-fileinfo->backup-revision (cvs-fileinfo)
  "Get the backup revision from CVS-FILEINFO."
  (elt (cdr cvs-fileinfo) 8))

(defun cvs-fileinfo->cvs-diff-buffer (cvs-fileinfo)
  "Get cvs-diff-buffer from CVS-FILEINFO."
  (elt (cdr cvs-fileinfo) 9))

(defun cvs-fileinfo->vendor-diff-buffer (cvs-fileinfo)
  "Get backup-diff-buffer from CVS-FILEINFO."
  (elt (cdr cvs-fileinfo) 10))

(defun cvs-fileinfo->backup-diff-buffer (cvs-fileinfo)
  "Get backup-diff-buffer from CVS-FILEINFO."
  (elt (cdr cvs-fileinfo) 11))

(defun cvs-fileinfo->full-log (cvs-fileinfo)
  "Get full-log from CVS-FILEINFO."
  (elt (cdr cvs-fileinfo) 12))

(defun cvs-fileinfo->mod-time (cvs-fileinfo)
  "Get mod-time from CVS-FILEINFO."
  (elt (cdr cvs-fileinfo) 13))

;;; Modifiers:

(defun cvs-set-fileinfo->handled (cvs-fileinfo newval)
  "Set handled in CVS-FILEINFO to NEWVAL."
  (aset (cdr cvs-fileinfo) 0 newval))

(defun cvs-set-fileinfo->marked (cvs-fileinfo newval)
  "Set marked in CVS-FILEINFO to NEWVAL."
  (aset (cdr cvs-fileinfo) 1 newval))

(defun cvs-set-fileinfo->type (cvs-fileinfo newval)
  "Set type in CVS-FILEINFO to NEWVAL."
  (aset (cdr cvs-fileinfo) 2 newval))

(defun cvs-set-fileinfo->dir (cvs-fileinfo newval)
  "Set dir in CVS-FILEINFO to NEWVAL.
The directory should now end with a slash."
  (aset (cdr cvs-fileinfo) 3 newval))

(defun cvs-set-fileinfo->file-name (cvs-fileinfo newval)
  "Set file-name in CVS-FILEINFO to NEWVAL."
  (aset (cdr cvs-fileinfo) 4 newval))

(defun cvs-set-fileinfo->backup-file (cvs-fileinfo newval)
  "Set backup-file in CVS-FILEINFO to NEWVAL."
  (aset (cdr cvs-fileinfo) 5 newval))

(defun cvs-set-fileinfo->base-revision (cvs-fileinfo newval)
  "Set base-revision in CVS-FILEINFO to NEWVAL."
  (aset (cdr cvs-fileinfo) 6 newval))

(defun cvs-set-fileinfo->head-revision (cvs-fileinfo newval)
  "Set head-revision in CVS-FILEINFO to NEWVAL."
  (aset (cdr cvs-fileinfo) 7 newval))

(defun cvs-set-fileinfo->backup-revision (cvs-fileinfo newval)
  "Set backup-revision in CVS-FILEINFO to NEWVAL."
  (aset (cdr cvs-fileinfo) 8 newval))

(defun cvs-set-fileinfo->cvs-diff-buffer (cvs-fileinfo newval)
  "Set cvs-diff-buffer in CVS-FILEINFO to NEWVAL."
  (aset (cdr cvs-fileinfo) 9 newval))

(defun cvs-set-fileinfo->vendor-diff-buffer (cvs-fileinfo newval)
  "Set vendor-diff-buffer in CVS-FILEINFO to NEWVAL."
  (aset (cdr cvs-fileinfo) 10 newval))

(defun cvs-set-fileinfo->backup-diff-buffer (cvs-fileinfo newval)
  "Set backup-diff-buffer in CVS-FILEINFO to NEWVAL."
  (aset (cdr cvs-fileinfo) 11 newval))

(defun cvs-set-fileinfo->full-log (cvs-fileinfo newval)
  "Set full-log in CVS-FILEINFO to NEWVAL."
  (aset (cdr cvs-fileinfo) 12 newval))

(defun cvs-set-fileinfo->mod-time (cvs-fileinfo newval)
  "Set full-log in CVS-FILEINFO to NEWVAL."
  (aset (cdr cvs-fileinfo) 13 newval))

;;; Predicate:

(defun cvs-fileinfo-p (object)
  "Return t if OBJECT is a cvs-fileinfo."
  (eq (car-safe object) 'CVS-FILEINFO))

;;;; End of types.

;;----------
(defun cvs-use-temp-buffer ()
  "Display a temporary buffer in another window and select it.
The selected window will not be changed.  The temporary buffer will
be erased and writable."

  (let ((dir default-directory))
    (display-buffer (get-buffer-create cvs-temp-buffer-name))
    (set-buffer cvs-temp-buffer-name)
    (setq buffer-read-only nil)
    (setq default-directory dir)
    (erase-buffer)))

;;----------
(defun cvs-examine (directory &optional local)
  "Run a 'cvs -n update' in the current working directory.
That is, check what needs to be done, but don't change the disc.
Feed the output to a *cvs* buffer and run cvs-mode on it.
If optional prefix argument LOCAL is non-nil, 'cvs update -l' is run.
WARNING:  this doesn't work very well yet...."

  ;; TODO:  this should do everything cvs-update does...
  ;; for example, for CONFLICT files, it should setup fileinfo appropriately

  (interactive (list (read-file-name "CVS Update (directory): "
				     nil default-directory nil)
		     current-prefix-arg))
  (cvs-do-update directory local 'noupdate))

;;----------
(defun cvs-update (directory &optional local)
  "Run a 'cvs update' in the current working directory.  Feed the
output to a *cvs* buffer and run cvs-mode on it.
If optional prefix argument LOCAL is non-nil, 'cvs update -l' is run."

  (interactive (list (read-file-name "CVS Update (directory): "
				     nil default-directory nil)
		     current-prefix-arg))
  (cvs-do-update directory local nil)
  (switch-to-buffer cvs-buffer-name))

;;----------
(defun cvs-update-other-window (directory &optional local)
  "Run a 'cvs update' in the current working directory.  Feed the
output to a *cvs* buffer, display it in the other window, and run
cvs-mode on it.

If optional prefix argument LOCAL is non-nil, 'cvs update -l' is run."

  (interactive (list (read-file-name "CVS Update other window (directory): "
				     nil default-directory nil)
		     current-prefix-arg))
  (cvs-do-update directory local nil)
  (switch-to-buffer-other-window cvs-buffer-name))

;;----------
(defun cvs-filter (predicate list &rest extra-args)
  "Apply PREDICATE to each element on LIST.
Args:  PREDICATE LIST &rest EXTRA-ARGS.

Return a new list consisting of those elements that PREDICATE
returns non-nil for.

If more than two arguments are given the remaining args are
passed to PREDICATE."

  ;; Avoid recursion - this should work for LONG lists also!
  (let* ((head (cons 'dummy-header nil))
	 (tail head))
    (while list
      (if (apply predicate (car list) extra-args)
	  (setq tail (setcdr tail (list (car list)))))
      (setq list (cdr list)))
    (cdr head)))

;;----------
(defun cvs-mode-update-no-prompt ()
  "Run cvs update in current directory."

  (interactive)
  (cvs-do-update default-directory nil nil))

;;----------
(defun cvs-do-update (directory local dont-change-disc)
  "Do a 'cvs update' in DIRECTORY.
Args:  DIRECTORY LOCAL DONT-CHANGE-DISC.

If LOCAL is non-nil 'cvs update -l' is executed.
If DONT-CHANGE-DISC is non-nil 'cvs -n update' is executed.
Both LOCAL and DONT-CHANGE-DISC may be non-nil simultaneously.

*Note*:  DONT-CHANGE-DISC does not yet work.  The parser gets confused."

  (save-some-buffers)
  ;; Ensure that it is safe to do an update.  If not, ask user
  ;; for confirmation.
  (if (and (boundp 'cvs-cookie-handle) (collection-buffer cvs-cookie-handle))
      (if (collection-collect-tin
	   cvs-cookie-handle
	   '(lambda (cookie) (eq (cvs-fileinfo->type cookie) 'CONFLICT)))
	  (if (not
	       (yes-or-no-p
		"Only update if conflicts have been resolved.  Continue? "))
	      (error "Update aborted by user request."))))
  (if (not (file-exists-p cvs-program))
      (error "%s: file not found (check setting of cvs-program)"
	     cvs-program))
  (let* ((this-dir (file-name-as-directory (expand-file-name directory)))
	 (update-buffer (generate-new-buffer
			 (concat " " (file-name-nondirectory
				      (substring this-dir 0 -1))
				 "-update")))
	 (temp-name (make-temp-name
		     (concat (file-name-as-directory
			      (or (getenv "TMPDIR") "/tmp"))
			     "pcl-cvs.")))
	 (args nil))

    ;; Check that this-dir exists and is a directory that is under CVS contr.

    (if (not (file-directory-p this-dir))
	(error "%s is not a directory." this-dir))
    (if (not (file-directory-p (concat this-dir "CVS")))
	(error "%s does not contain CVS controlled files." this-dir))
    (if (file-readable-p (concat this-dir "CVS/Root"))
	(save-excursion		; read CVS/Root into cvs-cvsroot
	  (find-file (concat this-dir "CVS/Root"))
	  (goto-char (point-min))
	  (setq cvs-cvsroot (buffer-substring (point)
					      (progn (end-of-line) (point))))
	  (if (not cvs-cvsroot)
	      (error "Invalid contents of %sCVS/Root" this-dir))
	  (kill-buffer (current-buffer)))
      (if (and cvs-cvsroot-required
	       (not (or (getenv "CVSROOT") cvs-cvsroot)))
	  (error "Both cvs-cvsroot and environment variable CVSROOT are unset, and no CVS/Root.")))

    ;; Check that at most one `cvs update' is run at any time.

    (if (and cvs-update-running (process-status cvs-update-running)
	     (or (eq (process-status cvs-update-running) 'run)
		 (eq (process-status cvs-update-running) 'stop)))
	(error "Can't run two `cvs update' simultaneously."))

    (if (not (listp cvs-update-optional-flags))
	(error "cvs-update-optional-flags should be set using cvs-set-update-optional-flags"))

    ;; Generate "-d /master -n update -l".
    (setq args (concat (if cvs-cvsroot (concat " -d " cvs-cvsroot))
		       (if dont-change-disc " -n ")
		       " update "
		       (if local " -l ")
		       (if cvs-update-optional-flags
			   (mapconcat 'identity
				      (copy-sequence cvs-update-optional-flags)
				      " "))))

    ;; Set up the buffer that receives the stderr output from "cvs update".
    (set-buffer update-buffer)
    (setq default-directory this-dir)
    (make-local-variable 'cvs-stdout-file)
    (setq cvs-stdout-file temp-name)

    (setq cvs-update-running
	  (let ((process-connection-type nil)) ; Use a pipe, not a pty.
	    (start-process "cvs" update-buffer cvs-shell "-c"
			   (concat cvs-program " " args " > " temp-name))))

    (setq mode-line-process
	  (concat ": "
		  (symbol-name (process-status cvs-update-running))))
    (set-buffer-modified-p (buffer-modified-p))	; Update the mode line.
    (set-process-sentinel cvs-update-running 'cvs-sentinel)
    (set-process-filter cvs-update-running 'cvs-update-filter)
    (set-marker (process-mark cvs-update-running) (point-min))

    (save-excursion
      (set-buffer (get-buffer-create cvs-buffer-name))
      (setq buffer-read-only nil)
      (erase-buffer)
      (cvs-mode))
      
    (setq cvs-cookie-handle
	  (collection-create
	   cvs-buffer-name 'cvs-pp
	   cvs-startup-message		;See comment above cvs-startup-message.
	   "---------- End -----"))

    (cookie-enter-first
     cvs-cookie-handle
     (cvs-create-fileinfo
      'MESSAGE nil nil (concat "\n    Running `cvs " args "' in " this-dir
			       "...\n")))

    (save-excursion
      (set-buffer cvs-buffer-name)
      (setq mode-line-process
	    (concat ": "
		    (symbol-name (process-status cvs-update-running))))
      (set-buffer-modified-p (buffer-modified-p))	; Update the mode line.
      (setq buffer-read-only t))

    ;; Work around a bug in emacs 18.57 and earlier.
    (setq cvs-buffers-to-delete
	  (cvs-delete-unused-temporary-buffers cvs-buffers-to-delete)))

  ;; The following line is said to improve display updates on some
  ;; emacses.  It shouldn't be needed, but it does no harm.
  (sit-for 0))

;;----------
(defun cvs-delete-unused-temporary-buffers (list)
  "Delete all buffers on LIST that is not visible.
Return a list of all buffers that still is alive."

  (cond
   ((null list) nil)
   ((get-buffer-window (car list))
    (cons (car list)
	  (cvs-delete-unused-temporary-buffers (cdr list))))
   (t
    (kill-buffer (car list))
    (cvs-delete-unused-temporary-buffers (cdr list)))))

;;----------
(put 'cvs-mode 'mode-class 'special)

;;----------
(defun cvs-mode ()
  "\\<cvs-mode-map>Mode used for pcl-cvs, a front-end to CVS.

To get to the \"*cvs*\" buffer you should use ``\\[execute-extended-command] cvs-update''.

Full documentation is in the Texinfo file.  Here are the most useful commands:

\\[cvs-mode-previous-line] Move up.                    \\[cvs-mode-next-line] Move down.
\\[cvs-mode-commit]   Commit file.                \\[cvs-mode-update-no-prompt]   Re-update directory.
\\[cvs-mode-mark]   Mark file/dir.              \\[cvs-mode-unmark]   Unmark file/dir.
\\[cvs-mode-mark-all-files]   Mark all files.             \\[cvs-mode-unmark-all-files]   Unmark all files.
\\[cvs-mode-find-file]   Edit file/run Dired.        \\[cvs-mode-find-file-other-window]   Find file or run Dired in other window.
\\[cvs-mode-ignore]   Add file to ./.cvsignore.   \\[cvs-mode-add-change-log-entry-other-window]   Write ChangeLog in other window.
\\[cvs-mode-add]   Add to repository.          \\[cvs-mode-remove-file]   Remove file.
\\[cvs-mode-diff-cvs]   Diff with base revision.    \\[cvs-mode-diff-backup]   Diff backup file.
\\[cvs-mode-ediff]   Ediff base rev & backup.    \\[cvs-mode-diff-vendor]   Show merge from vendor branch.
\\[cvs-mode-emerge]   Emerge base rev & backup.   \\[cvs-mode-diff-backup]   Diff backup file.
\\[cvs-mode-acknowledge] Delete line from buffer.    \\[cvs-mode-remove-handled]   Remove processed entries.   
\\[cvs-mode-log]   Run ``cvs log''.            \\[cvs-mode-status]   Run ``cvs status''.
\\[cvs-mode-tag]   Run ``cvs tag''.            \\[cvs-mode-rtag]   Run ``cvs rtag''.
\\[cvs-mode-changelog-commit]   Like \\[cvs-mode-commit], but get default log text from ChangeLog.
\\[cvs-mode-undo-local-changes]   Revert the last checked in version - discard your changes to the file.

Entry to this mode runs cvs-mode-hook.
This description is updated for release 1.05-CVS-$Name:  $ of pcl-cvs.

All bindings:
\\{cvs-mode-map}"

  (interactive)
  (setq major-mode 'cvs-mode)
  (setq mode-name "CVS")
  (setq mode-line-process nil)
;; for older v18 emacs
;;(buffer-flush-undo (current-buffer))
  (buffer-disable-undo (current-buffer))
  (make-local-variable 'goal-column)
  (setq goal-column cvs-cursor-column)
  (use-local-map cvs-mode-map)
  (run-hooks 'cvs-mode-hook))

;;----------
(defun cvs-sentinel (proc msg)
  "Sentinel for the cvs update process.
This is responsible for parsing the output from the cvs update when
it is finished."

  (cond
   ((null (buffer-name (process-buffer proc)))
    ;; buffer killed
    (set-process-buffer proc nil))
   ((memq (process-status proc) '(signal exit))
    (let* ((obuf (current-buffer))
	   (omax (point-max))
	   (opoint (point)))
      ;; save-excursion isn't the right thing if
      ;;  process-buffer is current-buffer
      (unwind-protect
	  (progn
	    (set-buffer (process-buffer proc))
	    (setq mode-line-process
		  (concat ": "
			  (symbol-name (process-status proc))))
	    (let* ((out-file cvs-stdout-file)
		   (stdout-buffer (find-file-noselect out-file)))
	      (save-excursion
		(set-buffer stdout-buffer)
		(rename-buffer (concat " "
				       (file-name-nondirectory out-file)) t))
	      (cvs-parse-update stdout-buffer (process-buffer proc))
	      (setq cvs-buffers-to-delete
		    (cons (process-buffer proc)
			  (cons stdout-buffer
				cvs-buffers-to-delete)))
	      (delete-file out-file)))
	(set-buffer-modified-p (buffer-modified-p))
	(setq cvs-update-running nil))
      (if (equal obuf (process-buffer proc))
	  nil
	(set-buffer (process-buffer proc))
	(if (< opoint omax)
	    (goto-char opoint))
	(set-buffer obuf))))))

;;----------
(defun cvs-update-filter (proc string)
  "Filter function for pcl-cvs.
This function gets the output that CVS sends to stderr.  It inserts it
into (process-buffer proc) but it also checks if CVS is waiting for a
lock file.  If so, it inserts a message cookie in the *cvs* buffer."

  (let ((old-buffer (current-buffer))
	(data (match-data)))
    (unwind-protect
	(progn
     	  (set-buffer (process-buffer proc))
     	  (save-excursion
     	    ;; Insert the text, moving the process-marker.
     	    (goto-char (process-mark proc))
     	    (insert string)
     	    (set-marker (process-mark proc) (point))
	    ;; Delete any old lock message
	    (if (tin-nth cvs-cookie-handle 1)
		(tin-delete cvs-cookie-handle
			    (tin-nth cvs-cookie-handle 1)))
	    ;; Check if CVS is waiting for a lock.
	    (beginning-of-line 0)	;Move to beginning of last
					;complete line.
	    (cond
	     ((looking-at
	       "^cvs \\(update\\|server\\): \\[..:..:..\\] waiting for \\(.*\\)lock in \\(.*\\)$")
	      (setq cvs-lock-file (buffer-substring (match-beginning 3)
						    (match-end 3)))
	      (cookie-enter-last
	       cvs-cookie-handle
	       (cvs-create-fileinfo
		'MESSAGE nil nil
		(concat "\tWaiting for "
			(buffer-substring (match-beginning 2)
					  (match-end 2))
			"lock in " cvs-lock-file
			".\n\t (type M-x cvs-delete-lock to delete it)")))))))
      (store-match-data data)
      (set-buffer old-buffer))))

;;----------
(defun cvs-delete-lock ()
  "Delete the lock file that CVS is waiting for.
Note that this can be dangerous.  You should only do this
if you are convinced that the process that created the lock is dead."

  (interactive)
  (cond
   ((not (or (file-exists-p
	      (concat (file-name-as-directory cvs-lock-file) "#cvs.lock"))
	     (cvs-filter (function cvs-lock-file-p)
			 (directory-files cvs-lock-file))))
    (error "No lock files found."))
   ((yes-or-no-p (concat "Really delete locks in " cvs-lock-file "? "))
    ;; Re-read the directory -- the locks might have disappeared.
    (let ((locks (cvs-filter (function cvs-lock-file-p)
			     (directory-files cvs-lock-file))))
      (while locks
	(delete-file (concat (file-name-as-directory cvs-lock-file)
			     (car locks)))
	(setq locks (cdr locks)))
      (cvs-remove-directory
       (concat (file-name-as-directory cvs-lock-file) "#cvs.lock"))))))

;;----------
(defun cvs-remove-directory (dir)
  "Remove a directory."

  (if (file-directory-p dir)
      (call-process cvs-rmdir-program nil nil nil dir)
    (error "Not a directory: %s" dir))
  (if (file-exists-p dir)
      (error "Could not remove directory %s" dir)))

;;----------
(defun cvs-lock-file-p (file)
  "Return true if FILE looks like a CVS lock file."

  (or
   (string-match "^#cvs.tfl.[0-9]+$" file)
   (string-match "^#cvs.rfl.[0-9]+$" file)
   (string-match "^#cvs.wfl.[0-9]+$" file)))

;;----------
(defun cvs-quote-multiword-string (str)
  "Return STR surrounded in single quotes if it contains whitespace."
  (cond ((string-match "[ \t\n]" str)
	 (concat "'" str "'"))
	(t
	 str)))

;;----------
;; this should be in subr.el or some similar place....
(defun parse-string (str &optional regexp)
  "Explode the string STR into a list of words ala strtok(3).  Optional REGEXP
defines regexp matching word separator, which defaults to \"[ \\t\\n]+\"."
  (let (str-list			; new list
	str-token			; "index" of next token
	(str-start 0)			; "index" of current token
	(str-sep (if regexp
		     regexp
		   "[ \t\n]+")))
    (while (setq str-token (string-match str-sep str str-start))
      (setq str-list
	    (nconc str-list
		   (list (substring str str-start str-token))))
      (setq str-start (match-end 0)))
    ;; tag on the remainder as the final item
    (if (not (>= str-start (length str)))
	(setq str-list
	      (nconc str-list
		     (list (substring str str-start)))))
    str-list))

;;----------
(defun cvs-make-list (str)
  "Return list of words made from the string STR."
  (cond ((string-match "[ \t\n]+" str)
	 (let ((new-str (parse-string str "[ \t\n]+")))
	   ;; this is ugly, but assume if the first element is empty, there are
	   ;; no more elements.
	   (cond ((string= (car new-str) "")
		  nil)
		 (t
		  new-str))))
	((string= str "")
	 nil)
	(t
	 (list str))))

;;----------
(defun cvs-skip-line (stdout stderr regexp &optional arg)
  "Like forward-line, but check that the skipped line matches REGEXP.
Args:  STDOUT STDERR REGEXP &optional ARG.

If it doesn't match REGEXP a bug report is generated and displayed.
STDOUT and STDERR is only used to do that.

If optional ARG, a number, is given the ARGth parenthesized expression
in the REGEXP is returned as a string.
Point should be in column 1 when this function is called."

  (cond
   ((looking-at regexp)
    (forward-line 1)
    (if arg
	(buffer-substring (match-beginning arg)
			  (match-end arg))))
   (t
    (cvs-parse-error stdout
		     stderr
		     (if (eq (current-buffer) stdout)
			 'STDOUT
		       'STDERR)
		     (point)
		     regexp))))

;;----------
(defun cvs-get-current-dir (root-dir dirname)
  "Return current working directory, suitable for cvs-parse-update.
Args:  ROOT-DIR DIRNAME.

Concatenates ROOT-DIR and DIRNAME to form an absolute path."

  (if (string= "." dirname)
      (substring root-dir 0 -1)
    (concat root-dir dirname)))

;;----------
(defun cvs-compare-fileinfos (a b)
  "Compare fileinfo A with fileinfo B and return t if A is `less'."

  (cond
   ;; Sort acording to directories.
   ((string< (cvs-fileinfo->dir a) (cvs-fileinfo->dir b)) t)
   ((not (string= (cvs-fileinfo->dir a) (cvs-fileinfo->dir b))) nil)
   ;; The DIRCHANGE entry is always first within the directory.
   ((and (eq (cvs-fileinfo->type a) 'DIRCHANGE)
	 (not (eq (cvs-fileinfo->type b) 'DIRCHANGE))) t)
   ((and (eq (cvs-fileinfo->type b) 'DIRCHANGE)
	 (not (eq (cvs-fileinfo->type a) 'DIRCHANGE))) nil)
   ;; All files are sorted by file name.
   ((string< (cvs-fileinfo->file-name a) (cvs-fileinfo->file-name b)))))

;;----------
(defun cvs-parse-error (stdout-buffer stderr-buffer err-buf pos &optional indicator)
  "Handle a parse error when parsing the output from cvs.
Args:  STDOUT-BUFFER STDERR-BUFFER ERR-BUF POS &optional INDICATOR.

ERR-BUF should be 'STDOUT or 'STDERR."

  (setq pos (1- pos))
  (set-buffer cvs-buffer-name)
  (setq buffer-read-only nil)
  (erase-buffer)
  (insert "To: " pcl-cvs-bugs-address "\n")
  (insert "Subject: pcl-cvs release" pcl-cvs-version " parse error.\n")
  (insert (concat mail-header-separator "\n"))
  (insert "This bug report is automatically generated by pcl-cvs\n")
  (insert "because it doesn't understand some output from CVS.  Below\n")
  (insert "is detailed information about the error.  Please send\n")
  (insert "this, together with any information you think might be\n")
  (insert "useful for me to fix the bug, to the address above.  But\n")
  (insert "please check the \"known problems\" section of the\n")
  (insert "documentation first.  Note that this buffer contains\n")
  (insert "information that you might consider confidential.  You\n")
  (insert "are encouraged to read through it before sending it.\n")
  (insert "\n")
  (insert "Press C-c C-c to send this email.\n\n")
  (insert "Please state the version of these programs you are using:\n\n")
  (insert "RCS:  \ndiff: \n\n")

  (let* ((stdout (save-excursion (set-buffer stdout-buffer) (buffer-string)))
	 (stderr (save-excursion (set-buffer stderr-buffer) (buffer-string)))
	 (errstr (if (eq err-buf 'STDOUT) stdout stderr))
	 (errline-end (string-match "\n" errstr pos))
	 (errline (substring errstr pos errline-end)))
    (insert (format "Offending line (%d chars): >" (- errline-end pos)))
    (insert errline)
    (insert "<\n")
    (insert "Sent to " (symbol-name err-buf) " at pos " (format "%d\n" pos))
    (if indicator
	(insert "Optional args: \"" indicator "\".\n"))
    (insert "\nEmacs-version: " (emacs-version) "\n")
    (insert "Pcl-cvs Version: "
	    "@(#)OrigId: pcl-cvs.el,v 1.93 1993/05/31 22:44:00 ceder Exp\n")
    (insert "CVS Version: "
	    "@(#)cvs/contrib/pcl-cvs:$Name:  $:$Id: pcl-cvs.el,v 1.7 1998/01/04 14:24:13 kingdon Exp $\n\n")
    (insert (format "--- Contents of stdout buffer (%d chars) ---\n"
		    (length stdout)))
    (insert stdout)
    (insert "--- End of stdout buffer ---\n")
    (insert (format "--- Contents of stderr buffer (%d chars) ---\n"
		    (length stderr)))
    (insert stderr)
    (insert "--- End of stderr buffer ---\n")
    (insert "\nEnd of bug report.\n")
    (require 'sendmail)
    (mail-mode)
    (error "CVS parse error - please report this bug.")))
      
;;----------
(defun cvs-parse-update (stdout-buffer stderr-buffer)
  "Parse the output from `cvs update'.

Args:  STDOUT-BUFFER STDERR-BUFFER.

This functions parses the from `cvs update' (which should be
separated in its stdout- and stderr-components) and prints a
pretty representation of it in the *cvs* buffer.

Signals an error if unexpected output was detected in the buffer."

  (let* ((head (cons 'dummy nil))
	 (tail (cvs-parse-stderr stdout-buffer stderr-buffer
				 head default-directory))
	 (root-dir default-directory))
    (cvs-parse-stdout stdout-buffer stderr-buffer tail root-dir)
    (setq head (sort (cdr head) (function cvs-compare-fileinfos)))
    (collection-clear cvs-cookie-handle)
    (collection-append-cookies cvs-cookie-handle head)
    (cvs-remove-stdout-shadows)
    (if cvs-auto-remove-handled-directories
	(cvs-remove-empty-directories))
    (set-buffer cvs-buffer-name)
    (cvs-mode)
    (goto-char (point-min))
    (tin-goto-previous cvs-cookie-handle (point-min) 1)
    (setq default-directory root-dir)))

;;----------
(defun cvs-remove-stdout-shadows ()
  "Remove entries in the *cvs* buffer that comes from both stdout and stderr.
If there is two entries for a single file the second one should be
deleted.  (Remember that sort uses a stable sort algorithm, so one can
be sure that the stderr entry is always first)."

  (collection-filter-tins cvs-cookie-handle
			  (function
			   (lambda (tin)
			     (not (cvs-shadow-entry-p tin))))))

;;----------
(defun cvs-shadow-entry-p (tin)
  "Return non-nil if TIN is a shadow entry.
Args:  TIN.

A TIN is a shadow entry if the previous tin contains the same file."

  (let* ((previous-tin (tin-previous cvs-cookie-handle tin))
	 (curr (tin-cookie cvs-cookie-handle tin))
	 (prev (and previous-tin
		    (tin-cookie cvs-cookie-handle previous-tin))))
    (and
     prev curr
     (string= (cvs-fileinfo->file-name prev)
	      (cvs-fileinfo->file-name curr))
     (string= (cvs-fileinfo->dir prev)
	      (cvs-fileinfo->dir curr))
     (or
      (and (eq (cvs-fileinfo->type prev) 'CONFLICT)
	   (eq (cvs-fileinfo->type curr) 'CONFLICT))
      (and (eq (cvs-fileinfo->type prev) 'MERGED)
	   (eq (cvs-fileinfo->type curr) 'MODIFIED))
      (and (eq (cvs-fileinfo->type prev) 'REM-EXIST)
	   (eq (cvs-fileinfo->type curr) 'REMOVED))))))

;;----------
(defun cvs-find-backup-file (filename &optional dirname)
  "Look for a backup file for FILENAME, optionally in directory DIRNAME, and if
there is one, return the name of the first file found as a string."

  (if (eq dirname nil)
      (setq dirname default-directory))
  (car (directory-files dirname nil (concat "^\\" cvs-bakprefix filename
					    "\\."))))

;;----------
(defun cvs-find-backup-revision (filename)
  "Take FILENAME as the name of a cvs backup file and return the revision of
that file as a string."

    (substring filename
	       (+ 1 (string-match "\\.\\([0-9.]+\\)$" filename))))

;;----------
(defun cvs-parse-stderr (stdout-buffer stderr-buffer head dir)
  "Parse the output from CVS that is written to stderr.
Args:  STDOUT-BUFFER STDERR-BUFFER HEAD DIR

STDOUT-BUFFER holds the output that cvs sent to stdout.  It is only
used to create a bug report in case there is a parse error.
STDERR-BUFFER is the buffer that holds the output to parse.
HEAD is a cons-cell, the head of the list that is built.
DIR is the directory the `cvs update' was run in.

This function returns the last cons-cell in the list that is built."

  (save-window-excursion
    (set-buffer stderr-buffer)
    (goto-char (point-min))
    (let ((current-dir dir)
	  (root-dir dir))

      (while (< (point) (point-max))
	(cond

	 ;; CVS is descending a subdirectory.

	 ((looking-at
	   "^cvs \\(server\\|update\\): Updating \\(.*\\)$")
	  (setq current-dir
		(cvs-get-current-dir
		 root-dir
		 (buffer-substring (match-beginning 2) (match-end 2))))
	  (setcdr head (list (cvs-create-fileinfo
			      'DIRCHANGE
			      current-dir
			      "."	; the old version had nil here???
			      (buffer-substring (match-beginning 0)
						(match-end 0)))))
	  (setq head (cdr head))
	  (forward-line 1))

	 ;; File removed, since it is removed (by third party) in repository.
       
	 ((or (looking-at
	       "^cvs \\(update\\|server\\): warning: \\(.*\\) is not (any longer) pertinent")
	      (looking-at
	       "^cvs \\(update\\|server\\): \\(.*\\) is no longer in the repository"))

	  (setcdr head (list (cvs-create-fileinfo
			      'CVS-REMOVED
			      current-dir
			      (file-name-nondirectory
			       (buffer-substring (match-beginning 2)
						 (match-end 2)))
			      (buffer-substring (match-beginning 0)
						(match-end 0)))))
	  (setq head (cdr head))
	  (forward-line 1))

	 ;; File removed by you, but recreated by cvs.  Ignored.  Will say
	 ;; "Updated" on the next line.

	 ((looking-at
	   "^cvs \\(update\\|server\\): warning: .* was lost$")
	  (forward-line 1))

	 ;; Patch failed; CVS will refetch the file.  Ignored.
	 ((looking-at
	   "^[0-9]+ out of [0-9]+ hunks failed--saving rejects to .*$")
	  (forward-line 1))

	 ;; File unknown for some reason.
	 ;; FIXME:  is it really a good idea to add this as unknown here?

	 ((looking-at
	   "cvs \\(update\\|server\\): nothing known about \\(.*\\)$")
	  (let ((filename (buffer-substring (match-beginning 2)
					    (match-end 2))))
	    (if (file-directory-p filename)
		(setcdr head (list (cvs-create-fileinfo
				    'UNKNOWN-DIR
				    current-dir
				    "."
				    (buffer-substring (match-beginning 0)
						      (match-end 0)))))
	      (setcdr head (list (cvs-create-fileinfo
				  'UNKNOWN
				  current-dir
				  (file-name-nondirectory filename)
				  (buffer-substring (match-beginning 0)
						    (match-end 0)))))))
	  (setq head (cdr head))
	  (forward-line 1))

	 ;; A file that has been created by you, but added to the cvs
	 ;; repository by another.

	 ((looking-at
	   "^cvs \\(update\\|server\\): move away \\(.*\\); it is in the way$")
	  (setcdr head (list (cvs-create-fileinfo
			      'MOVE-AWAY
			      current-dir
			      (file-name-nondirectory
			       (buffer-substring (match-beginning 2)
						 (match-end 2)))
			      (buffer-substring (match-beginning 0)
						(match-end 0)))))
	  (setq head (cdr head))
	  (forward-line 1))

	 ;; Cvs waits for a lock.  Ignore.

	 ((looking-at
	   "^cvs \\(update\\|server\\): \\[..:..:..\\] waiting for .*lock in ")
	  (forward-line 1))
	 ((looking-at
	   "^cvs \\(update\\|server\\): \\[..:..:..\\] obtained lock in ")
	  (forward-line 1))

	 ;; File removed in repository, but edited by you.

	 ((looking-at
	   "^cvs \\(update\\|server\\): conflict: \\(.*\\) is modified but no longer in the repository$")
	  (setcdr head (list
			(cvs-create-fileinfo
			 'REM-CONFLICT
			 current-dir
			 (file-name-nondirectory
			  (buffer-substring (match-beginning 2)
					    (match-end 2)))
			 (buffer-substring (match-beginning 0)
					   (match-end 0)))))
	  (setq head (cdr head))
	  (forward-line 1))

	 ;; File removed in repository, but edited by someone else.

	 ((looking-at
	   "^cvs \\(update\\|server\\): conflict: removed \\(.*\\) was modified by second party")
	  (setcdr head
		  (list
		   (cvs-create-fileinfo
		    'MOD-CONFLICT
		    current-dir
		    (buffer-substring (match-beginning 1)
				      (match-end 1))
		    (buffer-substring (match-beginning 0)
				      (match-end 0)))))
	  (setq head (cdr head))
	  (forward-line 1))

	 ;; File removed in repository, but not in local directory.

	 ((looking-at
	   "^cvs \\(update\\|server\\): \\(.*\\) should be removed and is still there")
	  (setcdr head
		  (list
		   (cvs-create-fileinfo
		    'REM-EXIST
		    current-dir
		    (buffer-substring (match-beginning 2)
				      (match-end 2))
		    (buffer-substring (match-beginning 0)
				      (match-end 0)))))
	  (setq head (cdr head))
	  (forward-line 1))

	 ;; Error searching for repository

	 ((looking-at
	   "^cvs \\(update\\|server\\): in directory ")
	  (let ((start (point)))
	    (forward-line 1)
	    (cvs-skip-line stdout-buffer stderr-buffer
			   (regexp-quote "cvs [update aborted]: there is no repository "))
	    (setcdr head (list (cvs-create-fileinfo
				'REPOS-MISSING
				current-dir
				nil
				(buffer-substring start (point)))))
	    (setq head (cdr head))))

	 ;; Silly warning from attempted conflict resolution.  Ignored.
	 ;; FIXME:  Should it be?
	 ;; eg.:  "cvs update: cannot find revision APC-web-update in file .cvsignore"
	 ;;
	 ((looking-at
	   "^cvs \\(update\\|server\\): cannot find revision \\(.*\\) in file \\(.*\\)$")
	  (forward-line 1)
	  (message "%s" (buffer-substring (match-beginning 0) (match-end 0))))

	 ;; CVS has decided to merge someone elses changes into this document.
	 ;; About to start an rcsmerge operation...
	 ;;
	 ((looking-at
	   "^RCS file: ")

	  ;; skip the "RCS file:" line...
	  (forward-line 1)

	  (let ((complex-start (point))
		base-revision		; the first revision retrieved to merge from
		head-revision		; the second revision retrieved to merge from
		filename		; the name of the file being merged
		backup-file		; the name of the backup of the working file
		backup-revision)	; the revision of the original working file

	    (setq base-revision
		  (cvs-skip-line stdout-buffer stderr-buffer
				 "^retrieving revision \\(.*\\)$"
				 1))
	    (setq head-revision
		  (cvs-skip-line stdout-buffer stderr-buffer
				 "^retrieving revision \\(.*\\)$"
				 1))
	    (setq filename
		  (cvs-skip-line stdout-buffer stderr-buffer
				 "^Merging differences between [0-9.]+ and [0-9.]+ into \\(.*\\)$"
				 1))
	    (setq backup-file
		  (cvs-find-backup-file filename current-dir))
	    (setq backup-revision
		  (cvs-find-backup-revision backup-file))

	     ;; Was there a conflict during the merge?

	    (cond

	     ;;;; From CVS-1.3 & RCS-5.6.0.1 with GNU-Diffutils-2.5:
	     ;;;; "cvs update -j OLD-REV -j NEW-REV ."
	     ;;
	     ;; RCS file: /big/web-CVS/apc/cmd/Main/logout.sh,v
	     ;; retrieving revision 1.1.1.1
	     ;; retrieving revision 1.1.1.2
	     ;; Merging differences between 1.1.1.1 and 1.1.1.2 into logout.sh
	     ;; rcsmerge warning: overlaps during merge

	     ((looking-at
	       ;; Allow both RCS 5.5 and 5.6.  (5.6 prints "rcs" and " warning").
	       "^\\(rcs\\)?merge[:]*\\( warning\\)?: \\(overlaps\\|conflicts\\) during merge$")

	      ;; Yes, this is a conflict.
	      (cvs-skip-line stdout-buffer stderr-buffer
			     "^\\(rcs\\)?merge[:]*\\( warning\\)?: \\(overlaps\\|conflicts\\) during merge$")

	      ;; this line doesn't seem to appear in all cases -- perhaps only
	      ;; in "-j A -j B" usage, in which case this indicates ????
	      (cvs-skip-line stdout-buffer stderr-buffer
			     "^cvs \\(update\\|server\\): conflicts found in ")

	      (let ((fileinfo
		     (cvs-create-fileinfo
		      'CONFLICT current-dir
		      filename
		      (buffer-substring complex-start (point)))))

		;; squirrel away info about the files that were retrieved for merging
		(cvs-set-fileinfo->base-revision fileinfo base-revision)
		(cvs-set-fileinfo->head-revision fileinfo head-revision)
		(cvs-set-fileinfo->backup-revision fileinfo backup-revision)
		(cvs-set-fileinfo->backup-file fileinfo backup-file)

		(setcdr head (list fileinfo))
		(setq head (cdr head))))

	     ;; Was it a conflict, and was RCS compiled without DIFF3_BIN, in
	     ;; which case this is a failed conflict resolution?

	     ((looking-at
	       ;; Allow both RCS 5.5 and 5.6.  (5.6 prints "rcs" and " warning").
	       "^\\(rcs\\)?merge\\( warning\\)?: overlaps or other problems during merge$")

	      (cvs-skip-line stdout-buffer stderr-buffer
			     "^\\(rcs\\)?merge\\( warning\\)?: overlaps or other problems during merge$")
	      (cvs-skip-line stdout-buffer stderr-buffer
			     "^cvs update: could not merge ")
	      (cvs-skip-line stdout-buffer stderr-buffer
			     "^cvs update: restoring .* from backup file ")
	      (let ((fileinfo
		     (cvs-create-fileinfo
		      'CONFLICT current-dir
		      filename
		      (buffer-substring complex-start (point)))))
		(setcdr head (list fileinfo))
		(setq head (cdr head))))	   

	     ;; Not a conflict; it must be a succesful merge.

	     (t
	      (let ((fileinfo
		     (cvs-create-fileinfo
		      'MERGED current-dir
		      filename
		      (buffer-substring complex-start (point)))))
		(cvs-set-fileinfo->base-revision fileinfo base-revision)
		(cvs-set-fileinfo->head-revision fileinfo head-revision)
		(cvs-set-fileinfo->backup-revision fileinfo backup-revision)
		(cvs-set-fileinfo->backup-file fileinfo backup-file)
		(setcdr head (list fileinfo))
		(setq head (cdr head)))))))

	 ;; Error messages from CVS (incomplete)

	 ((looking-at
	   "^cvs \\(update\\|server\\): \\(invalid option .*\\)$")
	  (error "Interface problem with CVS: %s"
		 (buffer-substring (match-beginning 2) (match-end 2))))

	 ;; network errors

	 ;; Kerberos connection attempted but failed.  This is not
         ;; really an error, as CVS will automatically fall back to
         ;; rsh.  Plus it tries kerberos, if available, even when rsh
         ;; is what you really wanted.

	 ((looking-at
	   "^cvs update: kerberos connect:.*$")
	  (forward-line 1)
	  (message "Remote CVS: %s"
		   (buffer-substring (match-beginning 0) (match-end 0))))

         ;; And when kerberos *does* fail, cvs prints out some stuff
         ;; as it tries rsh.  Ignore that stuff too.

	 ((looking-at
	   "^cvs update: trying to start server using rsh$")
	  (forward-line 1))

	 ((looking-at
	   "^\\([^:]*\\) Connection timed out")
	  (error "Remote CVS: %s"
		   (buffer-substring (match-beginning 0) (match-end 0))))

	 ((looking-at
	   "^Permission denied.")
	  (error "Remote CVS: %s"
		 (buffer-substring (match-beginning 0) (match-end 0))))

	 ((looking-at
	   "^cvs \\[update aborted\\]: premature end of file from server")
	  (error "Remote CVS: %s"
		 (buffer-substring (match-beginning 0) (match-end 0))))

	 ;; Empty line.  Probably inserted by mistake by user (or developer :-)
	 ;; Ignore.

	 ((looking-at
	   "^$")
	  (forward-line 1))

	 ;; top-level parser (cond) default clause

	 (t
	  (cvs-skip-line stdout-buffer stderr-buffer
			 "^UN-MATCHABLE-OUTPUT"))))))

  ;; cause this function to return the head of the parser output list
  head)

;;----------
(defun cvs-parse-stdout (stdout-buffer stderr-buffer head root-dir)
  "Parse the output from CVS that is written to stderr.
Args:  STDOUT-BUFFER STDERR-BUFFER HEAD ROOT-DIR

STDOUT-BUFFER is the buffer that holds the output to parse.
STDERR-BUFFER holds the output that cvs sent to stderr.  It is only
used to create a bug report in case there is a parse error.

HEAD is a cons-cell, the head of the list that is built.
ROOT-DIR is the directory the `cvs update' was run in.

This function doesn't return anything particular."

  (save-window-excursion
    (set-buffer stdout-buffer)
    (goto-char (point-min))
    (while (< (point) (point-max))
      (cond

       ;; M:  The file is modified by the user, and untouched in the repository.
       ;; A:  The file is "cvs add"ed, but not "cvs ci"ed.
       ;; R:  The file is "cvs remove"ed, but not "cvs ci"ed.
       ;; C:  Conflict (only useful if a join was done and stderr has info...)
       ;; U:  The file is copied from the repository.
       ;; ?:  Unknown file or directory.

       ((looking-at
	 "^\\([MARCUP?]\\) \\(.*\\)$")
	(let*
	    ((c (char-after (match-beginning 1)))
	     (full-path (concat (file-name-as-directory root-dir)
				(buffer-substring (match-beginning 2)
						  (match-end 2))))
	     (isdir (file-directory-p full-path))
	     (fileinfo (cvs-create-fileinfo
			(cond ((eq c ?M) 'MODIFIED)
			      ((eq c ?A) 'ADDED)
			      ((eq c ?R) 'REMOVED)
			      ((eq c ?C) 'CONFLICT)
			      ((eq c ?U) 'UPDATED)
			      ((eq c ?P) 'PATCHED)
			      ((eq c ??) (if isdir
					     'UNKNOWN-DIR
					   'UNKNOWN)))
			(substring (file-name-directory full-path) 0 -1)
			(file-name-nondirectory full-path)
			(buffer-substring (match-beginning 0) (match-end 0)))))
	  ;; Updated and Patched files require no further action.
	  (if (memq c '(?U ?P))
	      (cvs-set-fileinfo->handled fileinfo t))

	  ;; Link this last on the list.
	  (setcdr head (list fileinfo))
	  (setq head (cdr head))
	  (forward-line 1)))

       ;; Executing a program because of the -u option in modules.
       ((looking-at
	 "^cvs \\(update\\|server\\): Executing")
	;; Skip by any output the program may generate to stdout.
	;; Note that pcl-cvs will get seriously confused if the
	;; program prints anything to stderr.
	(re-search-forward cvs-update-prog-output-skip-regexp)
	(forward-line 1))

       (t
	(cvs-parse-error stdout-buffer stderr-buffer 'STDOUT (point)
			 "cvs-parse-stdout"))))))

;;----------
(defun cvs-pp (fileinfo)
  "Pretty print FILEINFO.  Insert a printed representation in current buffer.
For use by the cookie package."

  (let ((a (cvs-fileinfo->type fileinfo))
        (s (if (cvs-fileinfo->marked fileinfo)
               "*" " "))
        (f (cvs-fileinfo->file-name fileinfo))
        (ci (if (cvs-fileinfo->handled fileinfo)
                "  " "ci")))
    (insert
     (cond
      ((eq a 'UPDATED)
       (format "%s Updated     %s" s f))
      ((eq a 'PATCHED)
       (format "%s Patched     %s" s f))
      ((eq a 'MODIFIED)
       (format "%s Modified %s %s" s ci f))
      ((eq a 'MERGED)
       (format "%s Merged   %s %s" s ci f))
      ((eq a 'CONFLICT)
       (format "%s Conflict    %s" s f))
      ((eq a 'ADDED)
       (format "%s Added    %s %s" s ci f))
      ((eq a 'REMOVED)
       (format "%s Removed  %s %s" s ci f))
      ((eq a 'UNKNOWN)
       (format "%s Unknown     %s" s f))
      ((eq a 'UNKNOWN-DIR)
       (format "%s Unknown dir %s" s f))
      ((eq a 'CVS-REMOVED)
       (format "%s Removed from repository:  %s" s f))
      ((eq a 'REM-CONFLICT)
       (format "%s Conflict: Removed from repository, changed by you: %s" s f))
      ((eq a 'MOD-CONFLICT)
       (format "%s Conflict: Removed by you, changed in repository: %s" s f))
      ((eq a 'REM-EXIST)
       (format "%s Conflict: Removed by you, but still exists: %s" s f))
      ((eq a 'DIRCHANGE)
       (format "\nIn directory %s:" (cvs-fileinfo->dir fileinfo)))
      ((eq a 'MOVE-AWAY)
       (format "%s Move away %s - it is in the way" s f))
      ((eq a 'REPOS-MISSING)
       (format "  This repository directory is missing!  Remove this directory manually."))
      ((eq a 'MESSAGE)
       (cvs-fileinfo->full-log fileinfo))
      (t
       (format "%s Internal error!  %s" s f))))))


;;; You can define your own keymap in .emacs.  pcl-cvs.el won't overwrite it.

(if cvs-mode-map
    nil
  (setq cvs-mode-map (make-keymap))
  (suppress-keymap cvs-mode-map)
  (define-prefix-command 'cvs-mode-map-control-c-prefix)
  (define-key cvs-mode-map "\C-?"	'cvs-mode-unmark-up)
  (define-key cvs-mode-map "\C-k"	'cvs-mode-acknowledge)
  (define-key cvs-mode-map "\C-n"	'cvs-mode-next-line)
  (define-key cvs-mode-map "\C-p"	'cvs-mode-previous-line)
  ;; ^C- keys are used to set various flags to control CVS features
  (define-key cvs-mode-map "\C-c"	'cvs-mode-map-control-c-prefix)
  (define-key cvs-mode-map "\C-c\C-c"	'cvs-change-cvsroot)
  (define-key cvs-mode-map "\C-c\C-d"	'cvs-set-diff-flags)
  (define-key cvs-mode-map "\C-c\C-l"	'cvs-set-log-flags)
  (define-key cvs-mode-map "\C-c\C-s"	'cvs-set-status-flags)
  (define-key cvs-mode-map "\C-c\C-u"	'cvs-set-update-optional-flags)
  ;; M- keys are usually those that operate on modules
  (define-key cvs-mode-map "\M-\C-?"	'cvs-mode-unmark-all-files)
  (define-key cvs-mode-map "\M-C"	'cvs-mode-rcs2log) ; i.e. "Create a ChangeLog"
  (define-key cvs-mode-map "\M-a"	'cvs-mode-admin)
  (define-key cvs-mode-map "\M-c"	'cvs-mode-checkout)
  (define-key cvs-mode-map "\M-o"	'cvs-mode-checkout-other-window)
  (define-key cvs-mode-map "\M-p"	'cvs-mode-rdiff) ; i.e. "create a Patch"
  (define-key cvs-mode-map "\M-r"	'cvs-mode-release)
  (define-key cvs-mode-map "\M-t"	'cvs-mode-rtag)
  ;; keys that operate on files
  (define-key cvs-mode-map " "	'cvs-mode-next-line)
  (define-key cvs-mode-map "?"	'describe-mode)
  (define-key cvs-mode-map "A"	'cvs-mode-add-change-log-entry-other-window)
  (define-key cvs-mode-map "B"	'cvs-mode-byte-compile-files)
  (define-key cvs-mode-map "C"  'cvs-mode-changelog-commit)
  (define-key cvs-mode-map "E"	'cvs-mode-emerge)
  (define-key cvs-mode-map "G"	'cvs-update)
  (define-key cvs-mode-map "M"	'cvs-mode-mark-all-files)
  (define-key cvs-mode-map "Q"	'cvs-examine)
  (define-key cvs-mode-map "R"	'cvs-mode-revert-updated-buffers)
  (define-key cvs-mode-map "U"	'cvs-mode-undo-local-changes)
  (define-key cvs-mode-map "a"	'cvs-mode-add)
  (define-key cvs-mode-map "b"	'cvs-mode-diff-backup)
  (define-key cvs-mode-map "c"	'cvs-mode-commit)
  (define-key cvs-mode-map "d"	'cvs-mode-diff-cvs)
  (define-key cvs-mode-map "e"	'cvs-mode-ediff)
  (define-key cvs-mode-map "f"	'cvs-mode-find-file)
  (define-key cvs-mode-map "g"	'cvs-mode-update-no-prompt)
  (define-key cvs-mode-map "i"	'cvs-mode-ignore)
  (define-key cvs-mode-map "l"	'cvs-mode-log)
  (define-key cvs-mode-map "m"	'cvs-mode-mark)
  (define-key cvs-mode-map "n"	'cvs-mode-next-line)
  (define-key cvs-mode-map "o"	'cvs-mode-find-file-other-window)
  (define-key cvs-mode-map "p"	'cvs-mode-previous-line)
  (define-key cvs-mode-map "q"	'bury-buffer)
  (define-key cvs-mode-map "r"	'cvs-mode-remove-file)
  (define-key cvs-mode-map "s"	'cvs-mode-status)
  (define-key cvs-mode-map "t"	'cvs-mode-tag)
  (define-key cvs-mode-map "u"	'cvs-mode-unmark)
  (define-key cvs-mode-map "v"	'cvs-mode-diff-vendor)
  (define-key cvs-mode-map "x"	'cvs-mode-remove-handled))

;;----------
(defun cvs-get-marked (&optional ignore-marks ignore-contents)
  "Return a list of all selected tins.
Args:  &optional IGNORE-MARKS IGNORE-CONTENTS.

If there are any marked tins, and IGNORE-MARKS is nil, return them.  Otherwise,
if the cursor selects a directory, return all files in it, unless there are
none, in which case just return the directory; or unless IGNORE-CONTENTS is not
nil, in which case also just return the directory.  Otherwise return (a list
containing) the file the cursor points to, or an empty list if it doesn't point
to a file at all."

  (cond
   ;; Any marked cookies?
   ((and (not ignore-marks)
	 (collection-collect-tin cvs-cookie-handle 'cvs-fileinfo->marked)))
   ;; Nope.
   ((and (not ignore-contents)
	 (let ((sel (tin-locate cvs-cookie-handle (point))))
	   (cond
	    ;; If a directory is selected, all it members are returned.
	    ((and sel (eq (cvs-fileinfo->type (tin-cookie cvs-cookie-handle
							  sel))
			  'DIRCHANGE))
	     (let ((retsel
		    (collection-collect-tin cvs-cookie-handle
					    'cvs-dir-member-p
					    (cvs-fileinfo->dir (tin-cookie
								cvs-cookie-handle sel)))))
	       (if retsel
		   retsel
		 (list sel))))
	    (t
	     (list sel))))))
   (t
    (list (tin-locate cvs-cookie-handle (point))))))

;;----------
(defun cvs-dir-member-p (fileinfo dir)
  "Return true if FILEINFO represents a file in directory DIR."

  (and (not (eq (cvs-fileinfo->type fileinfo) 'DIRCHANGE))
       (string= (cvs-fileinfo->dir fileinfo) dir)))

;;----------
(defun cvs-dir-empty-p (tin)
  "Return non-nil if TIN is a directory that is empty.
Args:  CVS-BUF TIN."

  (and (eq (cvs-fileinfo->type (tin-cookie cvs-cookie-handle tin)) 'DIRCHANGE)
       (or (not (tin-next cvs-cookie-handle tin))
	   (eq (cvs-fileinfo->type
		(tin-cookie cvs-cookie-handle
				    (tin-next cvs-cookie-handle tin)))
	       'DIRCHANGE))))

;;----------
(defun cvs-mode-revert-updated-buffers ()
  "Revert any buffers that are UPDATED, PATCHED, MERGED or CONFLICT."

  (interactive)
  (cookie-map (function cvs-revert-fileinfo) cvs-cookie-handle))

;;----------
(defun cvs-revert-fileinfo (fileinfo)
  "Revert the buffer that holds the file in FILEINFO if it has changed,
and if the type is UPDATED, PATCHED, MERGED or CONFLICT."

  (let* ((type (cvs-fileinfo->type fileinfo))
	 (file (cvs-fileinfo->full-path fileinfo))
	 (buffer (get-file-buffer file)))
    ;; For a revert to happen...
    (cond
     ((and
       ;; ...the type must be one that justifies a revert...
       (or (eq type 'UPDATED)
	   (eq type 'PATCHED)
	   (eq type 'MERGED)
	   (eq type 'CONFLICT))
       ;; ...and the user must be editing the file...
       buffer)
      (save-excursion
	(set-buffer buffer)
	(cond
	 ((buffer-modified-p)
	  (error "%s: edited since last cvs-update."
		 (buffer-file-name)))
	 ;; Go ahead and revert the file.
	 (t (revert-buffer 'dont-use-auto-save-file 'dont-ask))))))))

;;----------
(defun cvs-mode-remove-handled ()
  "Remove all lines that are handled.
Empty directories are removed."

  (interactive)
  ;; Pass one:  remove files that are handled.
  (collection-filter-cookies cvs-cookie-handle
			     (function
			      (lambda (fileinfo)
				(not (cvs-fileinfo->handled fileinfo)))))
  ;; Pass two:  remove empty directories.
  (if cvs-auto-remove-handled-directories
      (cvs-remove-empty-directories)))

;;----------
(defun cvs-remove-empty-directories ()
  "Remove empty directories in the *cvs* buffer."

  (collection-filter-tins cvs-cookie-handle
			  (function
			   (lambda (tin)
			     (not (cvs-dir-empty-p tin))))))

;;----------
(defun cvs-mode-mark (pos)
  "Mark a fileinfo.
Args:  POS.

If the fileinfo is a directory, all the contents of that directory are marked
instead.  A directory can never be marked.  POS is a buffer position."

  (interactive "d")
  (let* ((tin (tin-locate cvs-cookie-handle pos))
	 (sel (tin-cookie cvs-cookie-handle tin)))
    (cond
     ;; Does POS point to a directory?  If so, mark all files in that directory.
     ((eq (cvs-fileinfo->type sel) 'DIRCHANGE)
      (cookie-map
       (function (lambda (f dir)
		   (cond
		    ((cvs-dir-member-p f dir)
		     (cvs-set-fileinfo->marked f t)
		     t))))		; Tell cookie to redisplay this cookie.
       cvs-cookie-handle
       (cvs-fileinfo->dir sel)))
     (t
      (cvs-set-fileinfo->marked sel t)
      (tin-invalidate cvs-cookie-handle tin)
      (tin-goto-next cvs-cookie-handle pos 1)))))
  
;;----------
(defun cvs-committable (tin)
  "Check if the TIN is committable.
It is committable if it
   a) is not handled and
   b) is either MODIFIED, ADDED, REMOVED, MERGED or CONFLICT."

  (let* ((fileinfo (tin-cookie cvs-cookie-handle tin))
	 (type (cvs-fileinfo->type fileinfo)))
    (and (not (cvs-fileinfo->handled fileinfo))
	 (or (eq type 'MODIFIED)
	     (eq type 'ADDED)
	     (eq type 'REMOVED)
	     (eq type 'MERGED)
	     (eq type 'CONFLICT)))))

;;----------
(defun cvs-mode-commit ()
  "Check in all marked files, or the current file.
The user will be asked for a log message in a buffer.
If cvs-erase-input-buffer is non-nil that buffer will be erased.
Otherwise mark and point will be set around the entire contents of the
buffer so that it is easy to kill the contents of the buffer with \\[kill-region]."

  (interactive)
  (let* ((cvs-buf (current-buffer))
	 (marked (cvs-filter (function cvs-committable)
			     (cvs-get-marked))))
    (if (null marked)
	(error "Nothing to commit!")
      (pop-to-buffer (get-buffer-create cvs-commit-prompt-buffer))
      (goto-char (point-min))

      (if cvs-erase-input-buffer
	  (erase-buffer)
	(push-mark (point-max)))
      (cvs-edit-mode)
      (make-local-variable 'cvs-commit-list)
      (setq cvs-commit-list marked)
      (message "Press C-c C-c when you are done editing."))))

;;----------
(defun cvs-edit-done ()
  "Commit the files to the repository."

  (interactive)
  (if (null cvs-commit-list)
      (error "You have already committed the files"))
  (if (and (> (point-max) 1)
	   (/= (char-after (1- (point-max))) ?\n)
	   (or (eq cvs-commit-buffer-require-final-newline t)
	       (and cvs-commit-buffer-require-final-newline
		    (yes-or-no-p
		     (format "Buffer %s does not end in newline.  Add one? "
			     (buffer-name))))))
      (save-excursion
	(goto-char (point-max))
	(insert ?\n)))
  (save-some-buffers)
  (let ((cc-list cvs-commit-list)
	(cc-buffer (get-buffer cvs-buffer-name))
	(msg-buffer (current-buffer))
	(msg (buffer-substring (point-min) (point-max))))
    (pop-to-buffer cc-buffer)
    (bury-buffer msg-buffer)
    (cvs-use-temp-buffer)
    (message "Committing...")
    (if (cvs-execute-list cc-list cvs-program
			  (if cvs-cvsroot
			      (list "-d" cvs-cvsroot "commit" "-m" msg)
			    (list "commit" "-m" msg))
			  "Committing %s...")
	(error "Something went wrong.  Check the %s buffer carefully."
	       cvs-temp-buffer-name))
    ;; FIXME: don't do any of this if the commit fails.
    (let ((ccl cc-list))
      (while ccl
	(cvs-after-commit-function (tin-cookie cvs-cookie-handle (car ccl)))
	(setq ccl (cdr ccl))))
    (apply 'tin-invalidate cvs-cookie-handle cc-list)
    (set-buffer msg-buffer)
    (setq cvs-commit-list nil)
    (set-buffer cc-buffer)
    (if cvs-auto-remove-handled
	(cvs-mode-remove-handled)))
  
  (message "Committing... Done."))

;;----------
(defun cvs-after-commit-function (fileinfo)
  "Do everything that needs to be done when FILEINFO has been committed.
The fileinfo->handle is set, and if the buffer is present it is reverted."

  (cvs-set-fileinfo->handled fileinfo t)
  (if cvs-auto-revert-after-commit
      (let* ((file (cvs-fileinfo->full-path fileinfo))
	     (buffer (get-file-buffer file)))
	;; For a revert to happen...
	(if buffer
	    ;; ...the user must be editing the file...
	    (save-excursion
	      (set-buffer buffer)
	      (if (not (buffer-modified-p))
		  ;; ...but it must be unmodified.
		  (revert-buffer 'dont-use-auto-save-file 'dont-ask)))))))

;;----------
(defun cvs-execute-list (tin-list program constant-args &optional message-fmt)
  "Run PROGRAM on all elements on TIN-LIST.
Args:  TIN-LIST PROGRAM CONSTANT-ARGS.

The PROGRAM will be called with pwd set to the directory the files reside
in.  CONSTANT-ARGS should be a list of strings.  The arguments given to the
program will be CONSTANT-ARGS followed by all the files (from TIN-LIST) that
resides in that directory.  If the files in TIN-LIST resides in different
directories the PROGRAM will be run once for each directory (if all files in
the same directory appears after each other).

Any output from PROGRAM will be inserted in the current buffer.

This function return nil if all went well, or the numerical exit status or a
signal name as a string.  Note that PROGRAM might be called several times.  This
will return non-nil if something goes wrong, but there is no way to know which
process that failed.

If MESSAGE-FMT is not nil, then message is called to display progress with
MESSAGE-FMT as the string.  MESSAGE-FMT should contain one %s for the arg-list
being passed to PROGRAM."

  ;; FIXME:  something seems wrong with the error checking here....

  (let ((exitstatus nil))
    (while tin-list
      (let ((current-dir (cvs-fileinfo->dir (tin-cookie cvs-cookie-handle
							(car tin-list))))
	    arg-list
	    arg-str)

	;; Collect all marked files in this directory.

	(while (and tin-list
		    (string= current-dir
			     (cvs-fileinfo->dir (tin-cookie cvs-cookie-handle
							    (car tin-list)))))
	  (setq arg-list
		(cons (cvs-fileinfo->file-name
		       (tin-cookie cvs-cookie-handle (car tin-list)))
		      arg-list))
	  (setq tin-list (cdr tin-list)))

	(setq arg-list (nreverse arg-list))

	;; Execute the command on all the files that were collected.

	(if message-fmt
	    (message message-fmt
		     (mapconcat 'cvs-quote-multiword-string
				arg-list
				" ")))
	(setq default-directory (file-name-as-directory current-dir))
	(insert (format "=== cd %s\n" default-directory))
	(insert (format "=== %s %s\n\n"
			program
			(mapconcat 'cvs-quote-multiword-string
				   (nconc (copy-sequence constant-args)
					  arg-list)
				   " ")))
	(let ((res (apply 'call-process program nil t t
			  (nconc (copy-sequence constant-args) arg-list))))
	  ;; Remember the first, or highest, exitstatus.
	  (if (and (not (and (integerp res) (zerop res)))
		   (or (null exitstatus)
		       (and (integerp exitstatus) (= 1 exitstatus))))
	      (setq exitstatus res)))
	(goto-char (point-max))
	(if message-fmt
	    (message message-fmt
		     (mapconcat 'cvs-quote-multiword-string
				(nconc (copy-sequence arg-list) '("Done."))
				" ")))
	exitstatus))))

;;----------
;;;; +++ not currently used!
(defun cvs-execute-single-file-list (tin-list extractor program constant-args
					      &optional cleanup message-fmt)
  "Run PROGRAM on all elements on TIN-LIST.
Args:  TIN-LIST EXTRACTOR PROGRAM CONSTANT-ARGS &optional CLEANUP.

The PROGRAM will be called with pwd set to the directory the files
reside in.  CONSTANT-ARGS is a list of strings to pass as arguments to
PROGRAM.  The arguments given to the program will be CONSTANT-ARGS
followed by the list that EXTRACTOR returns.

EXTRACTOR will be called once for each file on TIN-LIST.  It is given
one argument, the cvs-fileinfo.  It can return t, which means ignore
this file, or a list of arguments to send to the program.

If CLEANUP is not nil, the filenames returned by EXTRACTOR are deleted.

If MESSAGE-FMT is not nil, then message is called to display progress with
MESSAGE-FMT as the string.  MESSAGE-FMT should contain one %s for the arg-list
being passed to PROGRAM."

    (while tin-list
      (let ((current-dir (file-name-as-directory
			  (cvs-fileinfo->dir
			   (tin-cookie cvs-cookie-handle
				       (car tin-list)))))
	    (arg-list
	     (funcall extractor
		      (tin-cookie cvs-cookie-handle (car tin-list)))))

	;; Execute the command unless extractor returned t.

	(if (eq arg-list t)
	    nil
	  (setq default-directory current-dir)
	  (insert (format "=== cd %s\n" default-directory))
	  (insert (format "=== %s %s\n\n"
			  program
			  (mapconcat 'cvs-quote-multiword-string
				     (nconc (copy-sequence constant-args)
					    arg-list)
				     " ")))
	  (if message-fmt
	      (message message-fmt (mapconcat 'cvs-quote-multiword-string
					      arg-list
					      " ")))
	  (apply 'call-process program nil t t
		 (nconc (copy-sequence constant-args) arg-list))
	  (goto-char (point-max))
	  (if message-fmt
	      (message message-fmt (mapconcat 'cvs-quote-multiword-string
					      (nconc arg-list '("Done."))
					      " ")))
	  (if cleanup
	      (while arg-list
;;;;		(kill-buffer ?????)
		(delete-file (car arg-list))
		(setq arg-list (cdr arg-list))))))
      (setq tin-list (cdr tin-list))))

;;----------
(defun cvs-edit-mode ()
  "\\<cvs-edit-mode-map>Mode for editing cvs log messages.
Commands:
\\[cvs-edit-done] checks in the file when you are ready.
This mode is based on fundamental mode."

  (interactive)
  (use-local-map cvs-edit-mode-map)
  (setq major-mode 'cvs-edit-mode)
  (setq mode-name "CVS Log")
  (auto-fill-mode 1))

;;----------
(if cvs-edit-mode-map
    nil
  (setq cvs-edit-mode-map (make-sparse-keymap))
  (define-prefix-command 'cvs-edit-mode-control-c-prefix)
  (define-key cvs-edit-mode-map "\C-c" 'cvs-edit-mode-control-c-prefix)
  (define-key cvs-edit-mode-map "\C-c\C-c" 'cvs-edit-done))

;;----------
(defun cvs-diffable (tins)
  "Return a list of all tins on TINS that it makes sense to run
``cvs diff'' on."

  ;; +++ There is an unnecessary (nreverse) here.  Get the list the
  ;; other way around instead!
  (let ((result nil))
    (while tins
      (let ((type (cvs-fileinfo->type
		   (tin-cookie cvs-cookie-handle (car tins)))))
	(if (or (eq type 'MODIFIED)
		(eq type 'UPDATED)
		(eq type 'PATCHED)
		(eq type 'MERGED)
		(eq type 'CONFLICT)
		(eq type 'REMOVED)	;+++Does this line make sense?
		(eq type 'ADDED))	;+++Does this line make sense?
	    (setq result (cons (car tins) result)))
	(setq tins (cdr tins))))
    (nreverse result)))
	  
;;----------
(defun cvs-mode-diff-cvs (&optional ignore-marks)
  "Diff the selected files against the head revisions in the repository.

If the variable cvs-diff-ignore-marks is non-nil any marked files will not be
considered to be selected.  An optional prefix argument will invert the
influence from cvs-diff-ignore-marks.

The flags in the variable cvs-diff-flags will be passed to ``cvs diff''.

The resulting diffs are placed in the cvs-fileinfo->cvs-diff-buffer."

  (interactive "P")
  (if (not (listp cvs-diff-flags))
      (error "cvs-diff-flags should be set using cvs-set-diff-flags."))
  (save-some-buffers)
  (message "cvsdiffing...")
  (let ((marked-file-list (cvs-diffable
		 (cvs-get-marked
		  (or (and ignore-marks (not cvs-diff-ignore-marks))
		      (and (not ignore-marks) cvs-diff-ignore-marks))))))
    (while marked-file-list
      (let ((fileinfo-to-diff (tin-cookie cvs-cookie-handle
					  (car marked-file-list)))
	    (local-def-directory (file-name-as-directory
				  (cvs-fileinfo->dir
				   (tin-cookie cvs-cookie-handle
					       (car marked-file-list))))))
	(message "cvsdiffing %s..."
		 (cvs-fileinfo->file-name fileinfo-to-diff))

	;; FIXME:  this seems messy to test and set buffer name at this point....
	(if (not (cvs-fileinfo->cvs-diff-buffer fileinfo-to-diff))
	    (cvs-set-fileinfo->cvs-diff-buffer fileinfo-to-diff
					       (concat "*cvs-diff-"
						       (cvs-fileinfo->file-name
							fileinfo-to-diff)
						       "-in-"
						       local-def-directory
						       "*")))
	(display-buffer (get-buffer-create
			 (cvs-fileinfo->cvs-diff-buffer fileinfo-to-diff)))
	(set-buffer (cvs-fileinfo->cvs-diff-buffer fileinfo-to-diff))
	(setq buffer-read-only nil)
	(setq default-directory local-def-directory)
	(erase-buffer)
	(insert (format "=== cd %s\n" default-directory))
	(insert (format "=== cvs %s\n\n"
			(mapconcat 'cvs-quote-multiword-string
				   (nconc (if cvs-cvsroot
					      (list "-d" cvs-cvsroot "diff")
					    '("diff"))
					  (copy-sequence cvs-diff-flags)
					  (list (cvs-fileinfo->file-name
						 fileinfo-to-diff)))
				   " ")))
	(if (apply 'call-process cvs-program nil t t
		   (nconc (if cvs-cvsroot
			      (list "-d" cvs-cvsroot "diff")
			    '("diff"))
			  (copy-sequence cvs-diff-flags)
			  (list (cvs-fileinfo->file-name fileinfo-to-diff))))
	    (message "cvsdiffing %s... Done."
		     (cvs-fileinfo->file-name fileinfo-to-diff))
	  (message "cvsdiffing %s... No differences found."
		   (cvs-fileinfo->file-name fileinfo-to-diff)))
	(goto-char (point-max))
	(setq marked-file-list (cdr marked-file-list)))))
  (message "cvsdiffing... Done."))

;;----------
(defun cvs-mode-diff-backup (&optional ignore-marks)
  "Diff the files against the backup file.
This command can be used on files that are marked with \"Merged\"
or \"Conflict\" in the *cvs* buffer.

If the variable cvs-diff-ignore-marks is non-nil any marked files will
not be considered to be selected.  An optional prefix argument will
invert the influence from cvs-diff-ignore-marks.

The flags in cvs-diff-flags will be passed to ``diff''.

The resulting diffs are placed in the cvs-fileinfo->backup-diff-buffer."

  (interactive "P")
  (if (not (listp cvs-diff-flags))
      (error "cvs-diff-flags should be set using cvs-set-diff-flags."))
  (save-some-buffers)
  (let ((marked-file-list (cvs-filter
			   (function cvs-backup-diffable)
			   (cvs-get-marked
			    (or
			     (and ignore-marks (not cvs-diff-ignore-marks))
			     (and (not ignore-marks) cvs-diff-ignore-marks))))))
    (if (null marked-file-list)
	(error "No ``Conflict'' or ``Merged'' file selected!"))
    (message "backup diff...")
    (while marked-file-list
      (let ((fileinfo-to-diff (tin-cookie cvs-cookie-handle
					  (car marked-file-list)))
	    (local-def-directory (file-name-as-directory
				  (cvs-fileinfo->dir
				   (tin-cookie cvs-cookie-handle
					       (car marked-file-list)))))
	    (backup-temp-files (cvs-diff-backup-extractor
				(tin-cookie cvs-cookie-handle
					    (car marked-file-list)))))
	(message "backup diff %s..."
		 (cvs-fileinfo->file-name fileinfo-to-diff))

	;; FIXME:  this seems messy to test and set buffer name at this point....
	(if (not (cvs-fileinfo->backup-diff-buffer fileinfo-to-diff))
	    (cvs-set-fileinfo->backup-diff-buffer fileinfo-to-diff
						  (concat "*cvs-diff-"
							  (cvs-fileinfo->backup-file
							   fileinfo-to-diff)
							  "-to-"
							  (cvs-fileinfo->file-name
							   fileinfo-to-diff)
							  "-in"
							  local-def-directory
							  "*")))
	(display-buffer (get-buffer-create
			 (cvs-fileinfo->backup-diff-buffer fileinfo-to-diff)))
	(set-buffer (cvs-fileinfo->backup-diff-buffer fileinfo-to-diff))
	(setq buffer-read-only nil)
	(setq default-directory local-def-directory)
	(erase-buffer)
	(insert (format "=== cd %s\n" default-directory))
	(insert (format "=== %s %s\n\n"
			cvs-diff-program
			(mapconcat 'cvs-quote-multiword-string
				   (nconc (copy-sequence cvs-diff-flags)
					  backup-temp-files)
				   " ")))
	(apply 'call-process cvs-diff-program nil t t
	       (nconc (copy-sequence cvs-diff-flags) backup-temp-files))
	(goto-char (point-max))
	(message "backup diff %s... Done."
		 (cvs-fileinfo->file-name fileinfo-to-diff))
	(setq marked-file-list (cdr marked-file-list)))))
  (message "backup diff... Done."))

;;----------
(defun cvs-mode-diff-vendor (&optional ignore-marks)
  "Diff the revisions merged into the current file.  I.e. show what changes
were merged in.

This command can be used on files that are marked with \"Merged\"
or \"Conflict\" in the *cvs* buffer.

If the variable cvs-diff-ignore-marks is non-nil any marked files will
not be considered to be selected.  An optional prefix argument will
invert the influence from cvs-diff-ignore-marks.

The flags in cvs-diff-flags will be passed to ``diff''.

The resulting diffs are placed in the cvs-fileinfo->vendor-diff-buffer."

  (interactive "P")
  (if (not (listp cvs-diff-flags))
      (error "cvs-diff-flags should be set using cvs-set-diff-flags."))
  (save-some-buffers)
  (let ((marked-file-list (cvs-filter
			   (function cvs-vendor-diffable)
			   (cvs-get-marked
			    (or
			     (and ignore-marks (not cvs-diff-ignore-marks))
			     (and (not ignore-marks) cvs-diff-ignore-marks))))))
    (if (null marked-file-list)
	(error "No ``Conflict'' or ``Merged'' file selected!"))
    (message "vendor diff...")
    (while marked-file-list
      (let ((fileinfo-to-diff (tin-cookie cvs-cookie-handle
					  (car marked-file-list)))
	    (local-def-directory (file-name-as-directory
				  (cvs-fileinfo->dir
				   (tin-cookie cvs-cookie-handle
					       (car marked-file-list)))))
	    (vendor-temp-files (cvs-diff-vendor-extractor
				(tin-cookie cvs-cookie-handle
					    (car marked-file-list)))))
	(message "vendor diff %s..."
		     (cvs-fileinfo->file-name fileinfo-to-diff))
	(if (not (cvs-fileinfo->vendor-diff-buffer fileinfo-to-diff))
	    (cvs-set-fileinfo->vendor-diff-buffer fileinfo-to-diff
						  (concat "*cvs-diff-"
							  (cvs-fileinfo->file-name
							   fileinfo-to-diff)
							  "-of-"
							  (cvs-fileinfo->base-revision
							   fileinfo-to-diff)
							  "-to-"
							  (cvs-fileinfo->head-revision
							   fileinfo-to-diff)
							  "-in-"
							  local-def-directory
							  "*")))
	(display-buffer (get-buffer-create
			 (cvs-fileinfo->vendor-diff-buffer fileinfo-to-diff)))
	(set-buffer (cvs-fileinfo->vendor-diff-buffer fileinfo-to-diff))
	(setq buffer-read-only nil)
	(setq default-directory local-def-directory)
	(erase-buffer)
	(insert (format "=== cd %s\n" default-directory))
	(insert (format "=== %s %s\n\n"
			cvs-diff-program
			(mapconcat 'cvs-quote-multiword-string
				   (nconc (copy-sequence cvs-diff-flags)
					  vendor-temp-files)
				   " ")))
	(apply 'call-process cvs-diff-program nil t t
	       (nconc (copy-sequence cvs-diff-flags) vendor-temp-files))
	(goto-char (point-max))
	(message "vendor diff %s... Done."
		     (cvs-fileinfo->file-name fileinfo-to-diff))
	(while vendor-temp-files
	  (cvs-kill-buffer-visiting (car vendor-temp-files))
	  (delete-file (car vendor-temp-files))
	  (setq vendor-temp-files (cdr vendor-temp-files)))
	(setq marked-file-list (cdr marked-file-list)))))
  (message "vendor diff... Done."))

;;----------
(defun cvs-backup-diffable (tin)
  "Check if the TIN is backup-diffable.
It must have a backup file to be diffable."

  (file-readable-p
   (cvs-fileinfo->backup-file (tin-cookie cvs-cookie-handle tin))))

;;----------
(defun cvs-vendor-diffable (tin)
  "Check if the TIN is vendor-diffable.
It must have head and base revision info to be diffable."

  (and
   (cvs-fileinfo->base-revision (tin-cookie cvs-cookie-handle tin))
   (cvs-fileinfo->head-revision (tin-cookie cvs-cookie-handle tin))))

;;----------
(defun cvs-diff-backup-extractor (fileinfo)
  "Return the filename and the name of the backup file as a list.
Signal an error if there is no backup file."

  (if (not (file-readable-p (cvs-fileinfo->backup-file fileinfo)))
      (error "%s has no backup file."
	     (concat
	      (file-name-as-directory (cvs-fileinfo->dir fileinfo))
	      (cvs-fileinfo->file-name fileinfo))))
  (list	(cvs-fileinfo->backup-file fileinfo)
	 (cvs-fileinfo->file-name fileinfo)))

;;----------
(defun cvs-diff-vendor-extractor (fileinfo)
  "Retrieve and return the filenames of the vendor branch revisions as a list.
Signal an error if there is no info for the vendor revisions."

  (list (cvs-retrieve-revision-to-tmpfile fileinfo
					  (cvs-fileinfo->base-revision
					   fileinfo))
	(cvs-retrieve-revision-to-tmpfile fileinfo
					  (cvs-fileinfo->head-revision
					   fileinfo))))

;;----------
(defun cvs-mode-find-file-other-window (pos)
  "Select a buffer containing the file in another window.
Args:  POS."

  (interactive "d")
  (let ((tin (tin-locate cvs-cookie-handle pos)))
    (if tin
	(let ((type (cvs-fileinfo->type (tin-cookie cvs-cookie-handle tin))))
	  (cond
	   ((or (eq type 'REMOVED)
		(eq type 'CVS-REMOVED))
	    (error "Can't visit a removed file."))
	   ((eq type 'DIRCHANGE)
	    (let ((obuf (current-buffer))
		  (odir default-directory))
	      (setq default-directory
		    (file-name-as-directory
		     (cvs-fileinfo->dir
		      (tin-cookie cvs-cookie-handle tin))))
	      (dired-other-window default-directory)
	      (set-buffer obuf)
	      (setq default-directory odir)))
	   (t
	    (find-file-other-window (cvs-full-path tin)))))
      (error "There is no file to find."))))

;;----------
(defun cvs-fileinfo->full-path (fileinfo)
  "Return the full path for the file that is described in FILEINFO."

  (concat
   (file-name-as-directory
    (cvs-fileinfo->dir fileinfo))
   (cvs-fileinfo->file-name fileinfo)))

;;----------
(defun cvs-full-path (tin)
  "Return the full path for the file that is described in TIN."

  (cvs-fileinfo->full-path (tin-cookie cvs-cookie-handle tin)))

;;----------
(defun cvs-mode-find-file (pos)
  "Select a buffer containing the file in another window.
Args:  POS."

  (interactive "d")
  (let* ((cvs-buf (current-buffer))
	 (tin (tin-locate cvs-cookie-handle pos)))
    (if tin
	(let* ((fileinfo (tin-cookie cvs-cookie-handle tin))
	       (type (cvs-fileinfo->type fileinfo)))
	  (cond
	   ((or (eq type 'REMOVED)
		(eq type 'CVS-REMOVED))
	    (error "Can't visit a removed file."))
	   ((eq type 'DIRCHANGE)
	    (let ((odir default-directory))
	      (setq default-directory
		    (file-name-as-directory (cvs-fileinfo->dir fileinfo)))
	      (dired default-directory)
	      (set-buffer cvs-buf)
	      (setq default-directory odir))) 
	   (t
	    (find-file (cvs-full-path tin)))))
      (error "There is no file to find."))))

;;----------
(defun cvs-mode-mark-all-files ()
  "Mark all files.
Directories are not marked."

  (interactive)
  (cookie-map (function (lambda (cookie)
			  (cond
			   ((not (eq (cvs-fileinfo->type cookie) 'DIRCHANGE))
			    (cvs-set-fileinfo->marked cookie t)
			    t))))
	      cvs-cookie-handle))

;;----------
(defun cvs-mode-unmark (pos)
  "Unmark a fileinfo.
Args:  POS."

  (interactive "d")
  (let* ((tin (tin-locate cvs-cookie-handle pos))
	 (sel (tin-cookie cvs-cookie-handle tin)))
    (cond
     ((eq (cvs-fileinfo->type sel) 'DIRCHANGE)
      (cookie-map
       (function (lambda (f dir)
		   (cond
		    ((cvs-dir-member-p f dir)
		     (cvs-set-fileinfo->marked f nil)
		     t))))
       cvs-cookie-handle
       (cvs-fileinfo->dir sel)))
     (t
      (cvs-set-fileinfo->marked sel nil)
      (tin-invalidate cvs-cookie-handle tin)
      (tin-goto-next cvs-cookie-handle pos 1)))))

;;----------
(defun cvs-mode-unmark-all-files ()
  "Unmark all files.
Directories are also unmarked, but that doesn't matter, since
they should always be unmarked."

  (interactive)
  (cookie-map (function (lambda (cookie)
			  (cvs-set-fileinfo->marked cookie nil)
			  t))
	      cvs-cookie-handle))

;;----------
(defun cvs-do-removal (tins)
  "Remove files.
Args:  TINS.

TINS is a list of tins that the user wants to delete.  The files are deleted.
If the type of the tin is 'UNKNOWN or 'UNKNOWN-DIR the tin is removed from the
buffer.  If it is anything else the file is added to a list that should be `cvs
remove'd and the tin is changed to be of type 'REMOVED.

Returns a list of tins files that should be `cvs remove'd."

  (cvs-use-temp-buffer)
  (mapcar 'cvs-insert-full-path tins)
  (cond
   ((and tins (yes-or-no-p (format "Delete %d files? " (length tins))))
    (let (files-to-remove)
      (while tins
	(let* ((tin (car tins))
	       (fileinfo (tin-cookie cvs-cookie-handle tin))
	       (filepath (cvs-full-path tin))
	       (type (cvs-fileinfo->type fileinfo)))
	  (if (or (eq type 'REMOVED)
		  (eq type 'CVS-REMOVED))
	      nil
	    ;; if it doesn't exist, as a file or directory, ignore it
	    (cond ((file-directory-p filepath)
		   (call-process cvs-rmdir-program nil nil nil filepath))
		  ((file-exists-p filepath)
		   (delete-file filepath)))
	    (if (or (eq type 'UNKNOWN)
		     (eq type 'UNKNOWN-DIR)
		     (eq type 'MOVE-AWAY))
		(tin-delete cvs-cookie-handle tin)
	      (setq files-to-remove (cons tin files-to-remove))
	      (cvs-set-fileinfo->type fileinfo 'REMOVED)
	      (cvs-set-fileinfo->handled fileinfo nil)
	      (tin-invalidate cvs-cookie-handle tin))))
	(setq tins (cdr tins)))
      files-to-remove))
   (t nil)))

;;----------
(defun cvs-mode-remove-file ()
  "Remove all marked files."

  (interactive)
  (let ((files-to-remove (cvs-do-removal (cvs-get-marked))))
    (if (null files-to-remove)
	nil
      (cvs-use-temp-buffer)
      (message "removing from repository...")
      (if (cvs-execute-list files-to-remove cvs-program
			    (if cvs-cvsroot
				(list "-d" cvs-cvsroot "remove")
			      '("remove"))
			    "removing %s from repository...")
	  (error "CVS exited with non-zero exit status.")
	(message "removing from repository... Done.")))))

;;----------
(defun cvs-mode-undo-local-changes ()
  "Undo local changes to all marked files.
The file is removed and `cvs update FILE' is run."

  (interactive)
  (let ((tins-to-undo (cvs-get-marked)))
    (cvs-use-temp-buffer)
    (mapcar 'cvs-insert-full-path tins-to-undo)
    (cond
     ((and tins-to-undo (yes-or-no-p (format "Undo changes to %d files? "
					     (length tins-to-undo))))
      (let (files-to-update)
	(while tins-to-undo
	  (let* ((tin (car tins-to-undo))
		 (fileinfo (tin-cookie cvs-cookie-handle tin))
		 (type (cvs-fileinfo->type fileinfo)))
	    (cond
	     ((or
	       (eq type 'UPDATED)
	       (eq type 'PATCHED)
	       (eq type 'MODIFIED)
	       (eq type 'MERGED)
	       (eq type 'CONFLICT)
	       (eq type 'CVS-REMOVED)
	       (eq type 'REM-CONFLICT)
	       (eq type 'MOVE-AWAY)
	       (eq type 'REMOVED))
	      (if (not (eq type 'REMOVED))
		  (delete-file (cvs-full-path tin)))
	      (setq files-to-update (cons tin files-to-update))
	      (cvs-set-fileinfo->type fileinfo 'UPDATED)
	      (cvs-set-fileinfo->handled fileinfo t)
	      (tin-invalidate cvs-cookie-handle tin))

	     ((eq type 'MOD-CONFLICT)
	      (error "Use cvs-mode-add instead on %s."
		     (cvs-fileinfo->file-name fileinfo)))

	     ((eq type 'REM-CONFLICT)
	      (error "Can't deal with a file you have removed and recreated."))

	     ((eq type 'DIRCHANGE)
	      (error "Undo on directories not supported (yet)."))

	     ((eq type 'ADDED)
	      (error "There is no old revision to get for %s"
		     (cvs-fileinfo->file-name fileinfo)))
	     (t (error "cvs-mode-undo-local-changes: can't handle an %s"
		       type)))

	    (setq tins-to-undo (cdr tins-to-undo))))
	(cvs-use-temp-buffer)
	(message "Re-getting files from repository...")
	(if (cvs-execute-list files-to-update cvs-program
			      (if cvs-cvsroot
				  (list "-d" cvs-cvsroot "update")
				'("update"))
			      "Re-getting %s from repository...")
	    (error "CVS exited with non-zero exit status.")
	  (message "Re-getting files from repository... Done.")))))))

;;----------
(defun cvs-mode-acknowledge ()
  "Remove all marked files from the buffer."

  (interactive)
  (mapcar (function (lambda (tin)
		      (tin-delete cvs-cookie-handle tin)))
	  (cvs-get-marked)))

;;----------
(defun cvs-mode-unmark-up (pos)
  "Unmark the file on the previous line.
Takes one argument POS, a buffer position."

  (interactive "d")
  (let ((tin (tin-goto-previous cvs-cookie-handle pos 1)))
    (cond
     (tin
      (cvs-set-fileinfo->marked (tin-cookie cvs-cookie-handle tin)
				nil)
      (tin-invalidate cvs-cookie-handle tin)))))

;;----------
(defun cvs-mode-previous-line (arg)
  "Go to the previous line.
If a prefix argument is given, move by that many lines."

  (interactive "p")
  (tin-goto-previous cvs-cookie-handle (point) arg))

;;----------
(defun cvs-mode-next-line (arg)
  "Go to the next line.
If a prefix argument is given, move by that many lines."

  (interactive "p")
  (tin-goto-next cvs-cookie-handle (point) arg))

;;----------
(defun cvs-add-file-update-buffer (tin)
  "Sub-function to cvs-mode-add.  Internal use only.  Update the display.  Return
non-nil if `cvs add' should be called on this file.
Args:  TIN.

Returns 'DIR, 'ADD, 'ADD-DIR, or 'RESURRECT."

  (let ((fileinfo (tin-cookie cvs-cookie-handle tin)))
    (cond
     ((eq (cvs-fileinfo->type fileinfo) 'UNKNOWN-DIR)
      (cvs-set-fileinfo->full-log fileinfo "new directory added with cvs-mode-add")
      'ADD-DIR)
     ((eq (cvs-fileinfo->type fileinfo) 'UNKNOWN)
      (cvs-set-fileinfo->type fileinfo 'ADDED)
      (cvs-set-fileinfo->full-log fileinfo "new file added with cvs-mode-add")
      (tin-invalidate cvs-cookie-handle tin)
      'ADD)
     ((eq (cvs-fileinfo->type fileinfo) 'REMOVED)
      (cvs-set-fileinfo->type fileinfo 'UPDATED)
      (cvs-set-fileinfo->full-log fileinfo "file resurrected with cvs-mode-add")
      (cvs-set-fileinfo->handled fileinfo t)
      (tin-invalidate cvs-cookie-handle tin)
      'RESURRECT))))

;;----------
(defun cvs-add-sub (cvs-buf candidates)
  "Internal use only.
Args:  CVS-BUF CANDIDATES.

CANDIDATES is a list of tins.  Updates the CVS-BUF and returns a list of lists.
The first list is unknown tins that shall be `cvs add -m msg'ed.
The second list is unknown directory tins that shall be `cvs add -m msg'ed.
The third list is removed files that shall be `cvs add'ed (resurrected)."

  (let (add add-dir resurrect)
    (while candidates
      (let ((type (cvs-add-file-update-buffer (car candidates))))
	(cond ((eq type 'ADD)
	       (setq add (cons (car candidates) add)))
	      ((eq type 'ADD-DIR)
	       (setq add-dir (cons (car candidates) add-dir)))
	      ((eq type 'RESURRECT)
	       (setq resurrect (cons (car candidates) resurrect)))))
      (setq candidates (cdr candidates)))
    (list add add-dir resurrect)))

;;----------
(defun cvs-mode-add ()
  "Add marked files to the cvs repository."

  (interactive)
  (let* ((buf (current-buffer))
	 (marked (cvs-get-marked))
	 (result (cvs-add-sub buf marked))
	 (added (car result))
	 (newdirs (car (cdr result)))
	 (resurrect (car (cdr (cdr result))))
	 (msg (if (or added newdirs)
		  (read-from-minibuffer "Enter description: "))))

    (if (or resurrect (or added newdirs))
	(cvs-use-temp-buffer))

    (cond (resurrect
	   (message "Resurrecting files from repository...")
	   (if (cvs-execute-list resurrect
				 cvs-program
				 (if cvs-cvsroot
				     (list "-d" cvs-cvsroot "add")
				   '("add"))
				 "Resurrecting %s from repository...")
	       (error "CVS exited with non-zero exit status.")
	     (message "Resurrecting files from repository... Done."))))

    (cond (added
	   (message "Adding new files to repository...")
	   (if (cvs-execute-list added
				 cvs-program
				 (if cvs-cvsroot
				     (list "-d" cvs-cvsroot "add" "-m" msg)
				   (list "add" "-m" msg))
				 "Adding %s to repository...")
	       (error "CVS exited with non-zero exit status.")
	     (message "Adding new files to repository... Done."))))

    (cond (newdirs
	   (message "Adding new directories to repository...")
	   (if (cvs-execute-list newdirs
				 cvs-program
				 (if cvs-cvsroot
				     (list "-d" cvs-cvsroot "add" "-m" msg)
				   (list "add" "-m" msg))
				 "Adding %s to repository...")
	       (error "CVS exited with non-zero exit status.")
	     (while newdirs
	       (let* ((tin (car newdirs))
		      (fileinfo (tin-cookie cvs-cookie-handle tin))
		      (newdir (cvs-fileinfo->file-name fileinfo)))
		 (cvs-set-fileinfo->dir fileinfo
					(concat (cvs-fileinfo->dir fileinfo)
						"/"
						newdir))
		 (cvs-set-fileinfo->type fileinfo 'DIRCHANGE)
		 (cvs-set-fileinfo->file-name fileinfo ".")
		 (tin-invalidate cvs-cookie-handle tin)
		 (setq newdirs (cdr newdirs))))
	     ;; FIXME: this should really run cvs-update-no-prompt on the
	     ;; subdir and insert everthing in the current list.
	     (message "You must re-update to visit the new directories."))))))

;;----------
(defun cvs-mode-ignore ()
  "Arrange so that CVS ignores the selected files and directories.
This command ignores files/dirs that are flagged as `Unknown'."

  (interactive)
  (mapcar (function (lambda (tin)
		      (let* ((fileinfo (tin-cookie cvs-cookie-handle tin))
			     (type (cvs-fileinfo->type fileinfo)))
			(cond ((or (eq type 'UNKNOWN)
				   (eq type 'UNKNOWN-DIR))
			       (cvs-append-to-ignore fileinfo)
			       (tin-delete cvs-cookie-handle tin))))))
	  (cvs-get-marked)))

;;----------
(defun cvs-append-to-ignore (fileinfo)
  "Append the file in fileinfo to the .cvsignore file"

  (save-window-excursion
    (set-buffer (find-file-noselect (concat (file-name-as-directory
					     (cvs-fileinfo->dir fileinfo))
					    ".cvsignore")))
    (goto-char (point-max))
    (if (not (zerop (current-column)))
	(insert "\n"))
    (insert (cvs-fileinfo->file-name fileinfo) "\n")
    (if cvs-sort-ignore-file
	(sort-lines nil (point-min) (point-max)))
    (save-buffer)))

;;----------
(defun cvs-mode-status ()
  "Show cvs status for all marked files."

  (interactive)
  (save-some-buffers)
  (if (not (listp cvs-status-flags))
      (error "cvs-status-flags should be set using cvs-set-status-flags."))
  (let ((marked (cvs-get-marked nil t)))
    (cvs-use-temp-buffer)
    (message "Running cvs status ...")
    (if (cvs-execute-list marked
			  cvs-program
			  (append (if cvs-cvsroot (list "-d" cvs-cvsroot))
				  (list "-Q" "status")
				  cvs-status-flags)
			  "Running cvs -Q status %s...")
	(error "CVS exited with non-zero exit status.")
      (message "Running cvs -Q status ... Done."))))

;;----------
(defun cvs-mode-log ()
  "Display the cvs log of all selected files."

  (interactive)
  (if (not (listp cvs-log-flags))
      (error "cvs-log-flags should be set using cvs-set-log-flags."))
  (let ((marked (cvs-get-marked nil t)))
    (cvs-use-temp-buffer)
    (message "Running cvs log ...")
    (if (cvs-execute-list marked
			  cvs-program
			  (append (if cvs-cvsroot (list "-d" cvs-cvsroot))
				  (list "log")
				  cvs-log-flags)
			  "Running cvs log %s...")
	(error "CVS exited with non-zero exit status.")
      (message "Running cvs log ... Done."))))

;;----------
(defun cvs-mode-tag ()
  "Run 'cvs tag' on all selected files."

  (interactive)
  (if (not (listp cvs-tag-flags))
      (error "cvs-tag-flags should be set using cvs-set-tag-flags."))
  (let ((marked (cvs-get-marked nil t))
	(tag-args (cvs-make-list (read-string "Tag name (and flags): "))))
    (cvs-use-temp-buffer)
    (message "Running cvs tag ...")
    (if (cvs-execute-list marked
			  cvs-program
			  (append (if cvs-cvsroot (list "-d" cvs-cvsroot))
				  (list "tag")
				  cvs-tag-flags
				  tag-args)
			  "Running cvs tag %s...")
	(error "CVS exited with non-zero exit status.")
      (message "Running cvs tag ... Done."))))

;;----------
(defun cvs-mode-rtag ()
  "Run 'cvs rtag' on all selected files."

  (interactive)
  (if (not (listp cvs-rtag-flags))
      (error "cvs-rtag-flags should be set using cvs-set-rtag-flags."))
  (let ((marked (cvs-get-marked nil t))
	;; FIXME:  should give selection from the modules file
	(module-name (read-string "Module name: "))
	;; FIXME:  should also ask for an existing tag *or* date
	(rtag-args (cvs-make-list (read-string "Tag name (and flags): "))))
    (cvs-use-temp-buffer)
    (message "Running cvs rtag ...")
    (if (cvs-execute-list marked
			  cvs-program
			  (append (if cvs-cvsroot (list "-d" cvs-cvsroot)) 
				  (list "rtag")
				  cvs-rtag-flags
				  rtag-args
				  (list module-name))
			  "Running cvs rtag %s...")
	(error "CVS rtag exited with non-zero exit status.")
      (message "Running cvs rtag ... Done."))))

;;----------
(defun cvs-mode-byte-compile-files ()
  "Run byte-compile-file on all selected files that end in '.el'."

  (interactive)
  (let ((marked (cvs-get-marked)))
    (while marked
      (let ((filename (cvs-full-path (car marked))))
	(if (string-match "\\.el$" filename)
	    (byte-compile-file filename)))
      (setq marked (cdr marked)))))

;;----------
(defun cvs-insert-full-path (tin)
  "Insert full path to the file described in TIN in the current buffer."

  (insert (format "%s\n" (cvs-full-path tin))))

;;----------
(defun cvs-mode-add-change-log-entry-other-window (pos)
  "Add a ChangeLog entry in the ChangeLog of the current directory.
Args:  POS."

  (interactive "d")
  (let* ((cvs-buf (current-buffer))
	 (odir default-directory)
	 (obfname buffer-file-name)
	 (tin (tin-locate cvs-cookie-handle pos))
	 (fileinfo (tin-cookie cvs-cookie-handle tin))
	 (fname (cvs-fileinfo->file-name fileinfo))
	 (dname (file-name-as-directory (cvs-fileinfo->dir fileinfo))))
    (setq change-log-default-name nil)	; this rarely correct in 19.28
    (setq buffer-file-name (cond (fname
				   fname)
				  (t
				   nil)))
    (setq default-directory (cond (dname
				    dname)
				   (t
				    odir)))
    (add-change-log-entry-other-window)
    (set-buffer cvs-buf)
    (setq default-directory odir)
    (setq buffer-file-name obfname)))

;;----------
(defun print-cvs-tin (foo)
  "Debug utility."

  (let ((cookie (tin-cookie cvs-cookie-handle foo))
	(stream (get-buffer-create "pcl-cvs-debug")))
    (princ "==============\n" stream)
    (princ (cvs-fileinfo->file-name cookie) stream)
    (princ "\n" stream)
    (princ (cvs-fileinfo->dir cookie) stream)
    (princ "\n" stream)
    (princ (cvs-fileinfo->full-log cookie) stream)
    (princ "\n" stream)
    (princ (cvs-fileinfo->marked cookie) stream)
    (princ "\n" stream)))

;;----------
;; NOTE: the variable cvs-emerge-tmp-head-file will be "free" when compiling
(defun cvs-mode-emerge (pos)
  "Emerge appropriate revisions of the selected file.
Args:  POS."

  (interactive "d")
  (let* ((cvs-buf (current-buffer))
	 (tin (tin-locate cvs-cookie-handle pos)))
    (if (boundp 'cvs-emerge-tmp-head-file)
	(error "There can only be one emerge session active at a time."))
    (if tin
	(let* ((fileinfo (tin-cookie cvs-cookie-handle tin))
	       (type (cvs-fileinfo->type fileinfo)))
	  (cond
	   ((eq type 'MODIFIED)		; merge repository head rev. with working file
	    (require 'emerge)
	    (setq cvs-emerge-tmp-head-file ; trick to prevent multiple runs
		  (cvs-retrieve-revision-to-tmpfile fileinfo))
	    (unwind-protect
		(if (not (emerge-files
			  t						; arg
			  (cvs-fileinfo->full-path fileinfo) 		; file-A
			  ;; this is an un-avoidable compiler reference to a free variable
			  cvs-emerge-tmp-head-file			; file-B
			  (cvs-fileinfo->full-path fileinfo)		; file-out
			  nil						; start-hooks
			  '(lambda ()					; quit-hooks
			     (delete-file cvs-emerge-tmp-head-file)
			     (makunbound 'cvs-emerge-tmp-head-file))))
		    (error "Emerge session failed"))))

	   ;; re-do the same merge rcsmerge supposedly just did....
	   ((or (eq type 'MERGED)
		(eq type 'CONFLICT))	; merge backup-working=A, head=B, base=ancestor
	    (require 'emerge)
	    (setq cvs-emerge-tmp-head-file ; trick to prevent multiple runs
		  (cvs-retrieve-revision-to-tmpfile fileinfo
						    (cvs-fileinfo->head-revision
						     fileinfo)))
	    (let ((cvs-emerge-tmp-backup-working-file
		   (cvs-fileinfo->backup-file fileinfo))
		  (cvs-emerge-tmp-ancestor-file
		   (cvs-retrieve-revision-to-tmpfile fileinfo
						     (cvs-fileinfo->base-revision
						      fileinfo))))
	      (unwind-protect
		  (if (not (emerge-files-with-ancestor
			    t						; arg
			    cvs-emerge-tmp-backup-working-file		; file-A
			    ;; this is an un-avoidable compiler reference to a free variable
			    cvs-emerge-tmp-head-file			; file-B
			    cvs-emerge-tmp-ancestor-file		; file-ancestor
			    (cvs-fileinfo->full-path fileinfo)		; file-out
			    nil						; start-hooks
			    '(lambda ()					; quit-hooks
			       (delete-file cvs-emerge-tmp-backup-file)
			       (delete-file cvs-emerge-tmp-ancestor-file)
			       (delete-file cvs-emerge-tmp-head-file)
			       (makunbound 'cvs-emerge-tmp-head-file))))
		      (error "Emerge session failed")))))
	   (t
	    (error "Can only e-merge \"Modified\", \"Merged\" or \"Conflict\" files"))))
      (error "There is no file to e-merge."))))

;;----------
;; NOTE: the variable ediff-version may be "free" when compiling
(defun cvs-mode-ediff (pos)
  "Ediff appropriate revisions of the selected file.
Args:  POS."

  (interactive "d")
  (if (boundp 'cvs-ediff-tmp-head-file)
      (error "There can only be one ediff session active at a time."))
  (require 'ediff)
  (if (and (boundp 'ediff-version)
	   (>= (string-to-number ediff-version) 2.0)) ; FIXME real number?
      (run-ediff-from-cvs-buffer pos)
    (cvs-old-ediff-interface pos)))

(defun cvs-old-ediff-interface (pos)
  "Emerge like interface for older ediffs.
Args:  POS"

  (let* ((cvs-buf (current-buffer))
	 (tin (tin-locate cvs-cookie-handle pos)))
    (if tin
	(let* ((fileinfo (tin-cookie cvs-cookie-handle tin))
	       (type (cvs-fileinfo->type fileinfo)))
	  (cond
	   ((eq type 'MODIFIED)		; diff repository head rev. with working file
	    ;; should this be inside the unwind-protect, and should the
	    ;; makeunbound be an unwindform?
	    (setq cvs-ediff-tmp-head-file ; trick to prevent multiple runs
		  (cvs-retrieve-revision-to-tmpfile fileinfo))
	    (unwind-protect
		(if (not (ediff-files	; check correct ordering of args
			  (cvs-fileinfo->full-path fileinfo) 		; file-A
			  ;; this is an un-avoidable compiler reference to a free variable
			  cvs-ediff-tmp-head-file			; file-B
			  '(lambda ()					; startup-hooks
			     (make-local-hook 'ediff-cleanup-hooks)
			     (add-hook 'ediff-cleanup-hooks
				       '(lambda ()
					  (ediff-janitor)
					  (delete-file cvs-ediff-tmp-head-file)
					  (makunbound 'cvs-ediff-tmp-head-file))
				       nil t))))
		    (error "Ediff session failed"))))

	   ;; look at the merge rcsmerge supposedly just did....
	   ((or (eq type 'MERGED)
		(eq type 'CONFLICT))	; diff backup-working=A, head=B, base=ancestor
	    (if (not (boundp 'ediff-version))
		(error "ediff version way too old for 3-way diff"))
	    (if (<= (string-to-number ediff-version) 1.9) ; FIXME real number?
		(error "ediff version %s too old for 3-way diff" ediff-version))
	    (setq cvs-ediff-tmp-head-file ; trick to prevent multiple runs
		  (cvs-retrieve-revision-to-tmpfile fileinfo
						    (cvs-fileinfo->head-revision
						     fileinfo)))
	    (let ((cvs-ediff-tmp-backup-working-file
		   (cvs-fileinfo->backup-file fileinfo))
		  (cvs-ediff-tmp-ancestor-file
		   (cvs-retrieve-revision-to-tmpfile fileinfo
						     (cvs-fileinfo->base-revision
						      fileinfo))))
	      (unwind-protect
		  (if (not (ediff-files3 ; check correct ordering of args
			    cvs-ediff-tmp-backup-working-file		; file-A
			    ;; this is an un-avoidable compiler reference to a free variable
			    cvs-ediff-tmp-head-file			; file-B
			    cvs-ediff-tmp-ancestor-file			; file-ancestor
			    '(lambda ()					; start-hooks
			       (make-local-hook 'ediff-cleanup-hooks)
			       (add-hook 'ediff-cleanup-hooks
					 '(lambda ()
					    (ediff-janitor)
					    (delete-file cvs-ediff-tmp-backup-file)
					    (delete-file cvs-ediff-tmp-ancestor-file)
					    (delete-file cvs-ediff-tmp-head-file)
					    (makunbound 'cvs-ediff-tmp-head-file))
					 nil t))))
		      (error "Ediff session failed")))))

	   ((not (or (eq type 'UNKNOWN)
		     (eq type 'UNKNOWN-DIR))) ; i.e. UPDATED or PATCHED ????
	    ;; this should really diff the current working file with the previous
	    ;; rev. on the current branch (i.e. not the head, since that's what
	    ;; the current file should be)
	    (setq cvs-ediff-tmp-head-file ; trick to prevent multiple runs
		  (cvs-retrieve-revision-to-tmpfile fileinfo
						    (read-string "Rev #/tag to diff against: "
								 (cvs-fileinfo->head-revision
								  fileinfo))))
	    (unwind-protect
		(if (not (ediff-files	; check correct ordering of args
			  (cvs-fileinfo->full-path fileinfo)	 	; file-A
			  ;; this is an un-avoidable compiler reference to a free variable
			  cvs-ediff-tmp-head-file			; file-B
			  '(lambda ()					; startup-hooks
			     (make-local-hook 'ediff-cleanup-hooks)
			     (add-hook 'ediff-cleanup-hooks
				       '(lambda ()
					  (ediff-janitor)
					  (delete-file cvs-ediff-tmp-head-file)
					  (makunbound 'cvs-ediff-tmp-head-file))
				       nil t))))
		    (error "Ediff session failed"))))
	   (t
	    (error "Can not ediff \"Unknown\" files"))))
      (error "There is no file to ediff."))))

;;----------
(defun cvs-retrieve-revision-to-tmpfile (fileinfo &optional revision)
  "Retrieve the latest revision of the file in FILEINFO to a temporary file.
If second optional argument REVISION is given, retrieve that revision instead."

  (let
      ((temp-name (make-temp-name
		   (concat (file-name-as-directory
			    (or (getenv "TMPDIR") "/tmp"))
			   "pcl-cvs." revision))))
    (cvs-kill-buffer-visiting temp-name)
    (if (and revision
	     (stringp revision)
	     (not (string= revision "")))
	(message "Retrieving revision %s..." revision)
      (message "Retrieving latest revision..."))
    (let ((res (call-process cvs-shell nil nil nil "-c"
			     (concat cvs-program " update -p "
				     (if (and revision
					      (stringp revision)
					      (not (string= revision "")))
					 (concat "-r " revision " ")
				       "")
				     (cvs-fileinfo->full-path fileinfo)
				     " > " temp-name))))
      (if (and res (not (and (integerp res) (zerop res))))
	  (error "Something went wrong retrieving revision %s: %s"
		 revision res))

      (if revision
	  (message "Retrieving revision %s... Done." revision)
	(message "Retrieving latest revision... Done."))
      (save-excursion
	(set-buffer (find-file-noselect temp-name))
	(rename-buffer (concat " " (file-name-nondirectory temp-name)) t))
      temp-name)))

;;----------
(defun cvs-kill-buffer-visiting (filename)
  "If there is any buffer visiting FILENAME, kill it (without confirmation)."

  (let ((l (buffer-list)))
    (while l
      (if (string= (buffer-file-name (car l)) filename)
	  (kill-buffer (car l)))
      (setq l (cdr l)))))

;;----------
(defun cvs-change-cvsroot ()
  "Ask for a new cvsroot."

  (interactive)
  (cvs-set-cvsroot (read-file-name "New CVSROOT: " cvs-cvsroot)))

;;----------
(defun cvs-set-cvsroot (newroot)
  "Change the cvsroot."

  (if (or (file-directory-p (expand-file-name "CVSROOT" newroot))
	  (y-or-n-p (concat "Warning:  no CVSROOT found inside repository."
			    " Change cvs-cvsroot anyhow?")))
      (setq cvs-cvsroot newroot)))

;;----------
(defun cvs-set-diff-flags ()
  "Ask for new setting of cvs-diff-flags."

  (interactive)
  (let ((old-value (mapconcat 'identity
			      (copy-sequence cvs-diff-flags) " ")))
    (setq cvs-diff-flags
	  (cvs-make-list (read-string "Diff flags: " old-value)))))

;;----------
(defun cvs-set-update-optional-flags ()
  "Ask for new setting of cvs-update-optional-flags."
  
  (interactive)
  (let ((old-value (mapconcat 'identity
			      (copy-sequence cvs-update-optional-flags) " ")))
    (setq cvs-update-optional-flags
	  (cvs-make-list (read-string "Update optional flags: " old-value)))))

;;----------
(defun cvs-set-status-flags ()
  "Ask for new setting of cvs-status-flags."

  (interactive)
  (let ((old-value (mapconcat 'identity
			      (copy-sequence cvs-status-flags) " ")))
    (setq cvs-status-flags
	  (cvs-make-list (read-string "Status flags: " old-value)))))

;;----------
(defun cvs-set-log-flags ()
  "Ask for new setting of cvs-log-flags."

  (interactive)
  (let ((old-value (mapconcat 'identity
			      (copy-sequence cvs-log-flags) " ")))
    (setq cvs-log-flags
	  (cvs-make-list (read-string "Log flags: " old-value)))))

;;----------
(defun cvs-set-tag-flags ()
  "Ask for new setting of cvs-tag-flags."

  (interactive)
  (let ((old-value (mapconcat 'identity
			      (copy-sequence cvs-tag-flags) " ")))
    (setq cvs-tag-flags
	  (cvs-make-list (read-string "Tag flags: " old-value)))))

;;----------
(defun cvs-set-rtag-flags ()
  "Ask for new setting of cvs-rtag-flags."

  (interactive)
  (let ((old-value (mapconcat 'identity
			      (copy-sequence cvs-rtag-flags) " ")))
    (setq cvs-rtag-flags
	  (cvs-make-list (read-string "Rtag flags: " old-value)))))

;;----------
(if (string-match "Lucid" emacs-version)
    (progn
      (autoload 'pcl-cvs-fontify "pcl-cvs-lucid")
      (add-hook 'cvs-mode-hook 'pcl-cvs-fontify)))

(defun cvs-changelog-name (directory)
  "Return the name of the ChangeLog file that handles DIRECTORY.
This is in DIRECTORY or one of its parents.
Signal an error if we can't find an appropriate ChangeLog file."
  (let ((dir (file-name-as-directory directory))
        file)
    (while (and dir
                (not (file-exists-p 
                      (setq file (expand-file-name "ChangeLog" dir)))))
      (let ((last dir))
        (setq dir (file-name-directory (directory-file-name dir)))
        (if (equal last dir)
            (setq dir nil))))
    (or dir
        (error "Can't find ChangeLog for %s" directory))
    file))

(defun cvs-narrow-changelog ()
  "Narrow to the top page of the current buffer, a ChangeLog file.
Actually, the narrowed region doesn't include the date line.
A \"page\" in a ChangeLog file is the area between two dates."
  (or (eq major-mode 'change-log-mode)
      (error "cvs-narrow-changelog: current buffer isn't a ChangeLog"))

  (goto-char (point-min))

  ;; Skip date line and subsequent blank lines.
  (forward-line 1)
  (if (looking-at "[ \t\n]*\n")
      (goto-char (match-end 0)))

  (let ((start (point)))
    (forward-page 1)
    (narrow-to-region start (point))
    (goto-char (point-min))))

(defun cvs-changelog-paragraph ()
  "Return the bounds of the ChangeLog paragraph containing point.
If we are between paragraphs, return the previous paragraph."
  (save-excursion
    (beginning-of-line)
    (if (looking-at "^[ \t]*$")
        (skip-chars-backward " \t\n" (point-min)))
    (list (progn
            (if (re-search-backward "^[ \t]*\n" nil 'or-to-limit)
                (goto-char (match-end 0)))
            (point))
          (if (re-search-forward "^[ \t\n]*$" nil t)
              (match-beginning 0)
            (point)))))

(defun cvs-changelog-subparagraph ()
  "Return the bounds of the ChangeLog subparagraph containing point.
A subparagraph is a block of non-blank lines beginning with an asterisk.
If we are between sub-paragraphs, return the previous subparagraph."
  (save-excursion
    (end-of-line)
    (if (search-backward "*" nil t)
        (list (progn (beginning-of-line) (point))
              (progn 
                (forward-line 1)
                (if (re-search-forward "^[ \t]*[\n*]" nil t)
                    (match-beginning 0)
                  (point-max))))
      (list (point) (point)))))

(defun cvs-changelog-entry ()
  "Return the bounds of the ChangeLog entry containing point.
The variable `cvs-changelog-full-paragraphs' decides whether an
\"entry\" is a paragraph or a subparagraph; see its documentation string
for more details."
  (if cvs-changelog-full-paragraphs
      (cvs-changelog-paragraph)
    (cvs-changelog-subparagraph)))

;; NOTE: the variable user-full-name may be "free" when compiling
(defun cvs-changelog-ours-p ()
  "See if ChangeLog entry at point is for the current user, today.
Return non-nil iff it is."
  ;; Code adapted from add-change-log-entry.
  (or (looking-at
       (regexp-quote (format "%s  %s  <%s>"
			     (format-time-string "%Y-%m-%d")
			     add-log-full-name
			     add-log-mailing-address)))
      (looking-at
       (concat (regexp-quote (substring (current-time-string)
                                               0 10))
                      ".* "
                      (regexp-quote (substring (current-time-string) -4))
                      "[ \t]+"
		      (regexp-quote (if (and (boundp 'add-log-full-name)
                                             add-log-full-name)
                                        add-log-full-name
                                      user-full-name))
                      "  <"
		      (regexp-quote (if (and (boundp 'add-log-mailing-address)
					     add-log-mailing-address)
					add-log-mailing-address
			       user-mail-address))))))

(defun cvs-relative-path (base child)
  "Return a directory path relative to BASE for CHILD.
If CHILD doesn't seem to be in a subdirectory of BASE, just return 
the full path to CHILD."
  (let ((base (file-name-as-directory (expand-file-name base)))
        (child (expand-file-name child)))
    (or (string= base (substring child 0 (length base)))
        (error "cvs-relative-path: %s isn't in %s" child base))
    (substring child (length base))))

(defun cvs-changelog-entries (file)
  "Return the ChangeLog entries for FILE, and the ChangeLog they came from.
The return value looks like this:
  (LOGBUFFER (ENTRYSTART . ENTRYEND) ...)
where LOGBUFFER is the name of the ChangeLog buffer, and each
\(ENTRYSTART . ENTRYEND\) pair is a buffer region."
  (save-excursion
    (set-buffer (find-file-noselect
                 (cvs-changelog-name
                  (file-name-directory
                   (expand-file-name file)))))
    (or (eq major-mode 'change-log-mode)
	(change-log-mode))
    (goto-char (point-min))
    (if (looking-at "[ \t\n]*\n")
        (goto-char (match-end 0)))
    (if (not (cvs-changelog-ours-p))
        (list (current-buffer))
      (save-restriction
        (cvs-narrow-changelog)
        (goto-char (point-min))

        ;; Search for the name of FILE relative to the ChangeLog.  If that
        ;; doesn't occur anywhere, they're not using full relative
        ;; filenames in the ChangeLog, so just look for FILE; we'll accept
        ;; some false positives.
        (let ((pattern (cvs-relative-path
                        (file-name-directory buffer-file-name) file)))
          (if (or (string= pattern "")
                  (not (save-excursion
                         (search-forward pattern nil t))))
              (setq pattern file))

          (let (texts)
            (while (search-forward pattern nil t)
              (let ((entry (cvs-changelog-entry)))
                (setq texts (cons entry texts))
                (goto-char (elt entry 1))))

            (cons (current-buffer) texts)))))))

(defun cvs-changelog-insert-entries (buffer regions)
  "Insert those regions in BUFFER specified in REGIONS.
Sort REGIONS front-to-back first."
  (let ((regions (sort regions 'car-less-than-car))
        (last))
    (while regions
      (if (and last (< last (car (car regions))))
          (newline))
      (setq last (elt (car regions) 1))
      (apply 'insert-buffer-substring buffer (car regions))
      (setq regions (cdr regions)))))

(defun cvs-union (set1 set2)
  "Return the union of SET1 and SET2, according to `equal'."
  (while set2
    (or (member (car set2) set1)
        (setq set1 (cons (car set2) set1)))
    (setq set2 (cdr set2)))
  set1)

(defun cvs-insert-changelog-entries (files)
  "Given a list of files FILES, insert the ChangeLog entries for them."
  (let ((buffer-entries nil))

    ;; Add each buffer to buffer-entries, and associate it with the list
    ;; of entries we want from that file.
    (while files
      (let* ((entries (cvs-changelog-entries (car files)))
             (pair (assq (car entries) buffer-entries)))
        (if pair
            (setcdr pair (cvs-union (cdr pair) (cdr entries)))
          (setq buffer-entries (cons entries buffer-entries))))
      (setq files (cdr files)))

    ;; Now map over each buffer in buffer-entries, sort the entries for
    ;; each buffer, and extract them as strings.
    (while buffer-entries
      (cvs-changelog-insert-entries (car (car buffer-entries))
                                    (cdr (car buffer-entries)))
      (if (and (cdr buffer-entries) (cdr (car buffer-entries)))
          (newline))
      (setq buffer-entries (cdr buffer-entries)))))

(defun cvs-edit-delete-common-indentation ()
  "Unindent the current buffer rigidly until at least one line is flush left."
  (save-excursion
    (let ((common 100000))
      (goto-char (point-min))
      (while (< (point) (point-max))
        (if (not (looking-at "^[ \t]*$"))
            (setq common (min common (current-indentation))))
        (forward-line 1))
      (indent-rigidly (point-min) (point-max) (- common)))))

(defun cvs-mode-changelog-commit ()
  "Check in all marked files, or the current file.
Ask the user for a log message in a buffer.

This is just like `\\[cvs-mode-commit]', except that it tries to provide
appropriate default log messages by looking at the ChangeLog.  The
idea is to write your ChangeLog entries first, and then use this
command to commit your changes.

To select default log text, we:
- find the ChangeLog entries for the files to be checked in,
- verify that the top entry in the ChangeLog is on the current date
  and by the current user; if not, we don't provide any default text,
- search the ChangeLog entry for paragraphs containing the names of
  the files we're checking in, and finally
- use those paragraphs as the log text."

  (interactive)

  (let* ((cvs-buf (current-buffer))
         (marked (cvs-filter (function cvs-committable)
                             (cvs-get-marked))))
    (if (null marked)
        (error "Nothing to commit!")
      (pop-to-buffer (get-buffer-create cvs-commit-prompt-buffer))
      (goto-char (point-min))

      (erase-buffer)
      (cvs-insert-changelog-entries
       (mapcar (lambda (tin)
                 (let ((cookie (tin-cookie cvs-cookie-handle tin)))
                   (expand-file-name 
                    (cvs-fileinfo->file-name cookie)
                    (cvs-fileinfo->dir cookie))))
               marked))
      (cvs-edit-delete-common-indentation)

      (cvs-edit-mode)
      (make-local-variable 'cvs-commit-list)
      (setq cvs-commit-list marked)
      (message "Press C-c C-c when you are done editing."))))

(provide 'pcl-cvs)

;;;; end of file pcl-cvs.el
