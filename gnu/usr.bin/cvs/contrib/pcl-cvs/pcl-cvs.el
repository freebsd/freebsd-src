;;; @(#) Id: pcl-cvs.el,v 1.93 1993/05/31 22:44:00 ceder Exp 
;;; pcl-cvs.el -- A Front-end to CVS 1.3 or later.  Release 1.05.
;;; Copyright (C) 1991, 1992, 1993  Per Cederqvist
;;;
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

;;;; See below for installation instructions.
;;;;
;;;; There is an TeXinfo file that describes this package.  The GNU
;;;; General Public License is included in that file.  You should read
;;;; it to get the most from this package.

;;;; Send bug reports and improvements to ceder@lysator.liu.se or
;;;; ceder@signum.se.  Talk some about Signum Support here. +++FIXME

;;; Don't try to use this with CVS 1.2 or earlier. It won't work. Get
;;; CVS 1.3. This package works together with RCS 5.6 and probably 5.5
;;; as well.

;;; Mail questions and bug reports to ceder@lysator.liu.se.

(require 'cookie)
(provide 'pcl-cvs)

;;; -------------------------------------------------------
;;;	    START OF THINGS TO CHECK WHEN INSTALLING

(defvar cvs-program "/usr/local/bin/cvs"
  "*Full path to the cvs executable.")

(defvar cvs-diff-program "/usr/local/bin/diff"
  "*Full path to the diff program.")

(defvar cvs-rmdir-program "/bin/rmdir"
  "*Full path to the rmdir program. Typically /bin/rmdir.")

;; Uncomment the following line if you are running on 18.57 or earlier.
;(setq delete-exited-processes nil)
;; Emacs version 18.57 and earlier is likely to crash if
;; delete-exited-processes is t, since the sentinel uses lots of
;; memory, and 18.57 forgets to GCPROT a variable if
;; delete-exited-processes is t.

(defvar cvs-shell "/bin/sh"
  "*Full path to a shell that can do redirection on stdout.")

;;;	     END OF THINGS TO CHECK WHEN INSTALLING
;;; --------------------------------------------------------

(defvar cvs-cvsroot nil
  "*Specifies where the (current) cvs master repository is.
Overrides the $CVSROOT variable by sending \" -d dir\" to all cvs commands.
This switch is useful if you have multiple CVS repositories.")

(defvar cvs-cvsroot-required t
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
If this is set to any non-nil value entries that does not need to be
checked in will be removed from the *cvs* buffer after every cvs-mode-commit
command.")

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

(defvar cvs-status-flags nil
  "*List of strings to pass to ``cvs status''.")

(defvar cvs-log-flags nil
  "*List of strings to pass to ``cvs log''.")

(defvar cvs-diff-flags nil
  "*List of strings to use as flags to pass to ``diff'' and ``cvs diff''.
Used by cvs-mode-diff-cvs and cvs-mode-diff-backup.
Set this to '(\"-u\") to get a Unidiff format, or '(\"-c\") to get context diffs.")

(defvar cvs-update-prog-output-skip-regexp "$"
  "*A regexp that matches the end of the output from all cvs update programs.
That is, output from any programs that are run by CVS (by the flag -u
in the `modules' file - see cvs(5)) when `cvs update' is performed should
terminate with a line that this regexp matches.  It is enough that
some part of the line is matched.

The default (a single $) fits programs without output.")

;; The variables below are used internally by pcl-cvs. You should
;; never change them.

(defvar cvs-buffers-to-delete nil
  "List of temporary buffers that should be discarded as soon as possible.
Due to a bug in emacs 18.57 the sentinel can't discard them reliably.")

;; You are NOT allowed to disable this message by default.  However, you
;; are encouraged to inform your users that by adding
;;	(setq cvs-inhibit-copyright-message t)
;; to their .emacs they can get rid of it.  Just don't add that line
;; to your default.el!
(defvar cvs-inhibit-copyright-message nil
  "*Non-nil means don't display a Copyright message in the ``*cvs*'' buffer.")

(defconst pcl-cvs-version "1.05"
  "A string denoting the current release version of pcl-cvs.")

(defconst cvs-startup-message
  (if cvs-inhibit-copyright-message
      "PCL-CVS release 1.05"
    "PCL-CVS release 1.05.  Copyright (C) 1992, 1993 Per Cederqvist
Pcl-cvs comes with absolutely no warranty; for details consult the manual.
This is free software, and you are welcome to redistribute it under certain
conditions; again, consult the TeXinfo manual for details.")
  "*Startup message for CVS.")

(defvar cvs-update-running nil
  "This is set to nil when no process is running, and to
the process when a cvs update process is running.")

(defvar cvs-cookie-handle nil
  "Handle for the cookie structure that is displayed in the *cvs* buffer.")

(defvar cvs-mode-commit nil
  "Used internally by pcl-cvs.")

;;; The cvs data structure:
;;;
;;; When the `cvs update' is ready we parse the output. Every file
;;; that is affected in some way is added as a cookie of fileinfo
;;; (as defined below).
;;;

;;; cvs-fileinfo
;;;
;;;  marked		 t/nil
;;;  type		 One of
;;;			   UPDATED    - file copied from repository
;;;			   MODIFIED   - modified by you, unchanged in
;;;					repository
;;;			   ADDED      - added by you, not yet committed
;;;			   REMOVED    - removed by you, not yet committed
;;;			   CVS-REMOVED- removed, since file no longer exists
;;;					in the repository.
;;;			   MERGED     - successful merge
;;;			   CONFLICT   - conflict when merging
;;;			   REM-CONFLICT-removed in repository, changed locally.
;;;			   MOD-CONFLICT-removed locally, changed in repository.
;;;                        REM-EXIST  -removed locally, but still exists.
;;;			   DIRCHANGE  - A change of directory.
;;;			   UNKNOWN    - An unknown file.
;;;			   MOVE-AWAY  - A file that is in the way.
;;;			   REPOS-MISSING- The directory is removed from the
;;;					  repository. Go fetch a backup.
;;;                        MESSAGE    - This is a special fileinfo that is used
;;;  				          to display a text that should be in
;;;                                       full-log.
;;;  dir		 Directory the file resides in. Should not end with
;;;			 slash.
;;;  file-name		 The file name.
;;;  base-revision       The revision that the working file was based on.
;;;                      Only valid for MERGED and CONFLICT files.
;;;  cvs-diff-buffer	 A buffer that contains a 'cvs diff file'.
;;;  backup-diff-buffer	 A buffer that contains a 'diff file backup-file'.
;;;  full-log		 The output from cvs, unparsed.
;;;  mod-time		 Modification time of file used for *-diff-buffer.
;;;  handled		 True if this file doesn't require further action.
;;; 
;;; Constructor:

;;; cvs-fileinfo

;;; Constructor:

(defun cvs-create-fileinfo (type
			    dir
			    file-name
			    full-log)
  "Create a fileinfo from all parameters.
Arguments: TYPE DIR FILE-NAME FULL-LOG.
A fileinfo has the following fields:

  marked	    t/nil
  type		      One of
			UPDATED	   - file copied from repository
			MODIFIED   - modified by you, unchanged in
				     repository
			ADDED	   - added by you, not yet committed
			REMOVED	   - removed by you, not yet committed
			CVS-REMOVED- removed, since file no longer exists
				     in the repository.
			MERGED	   - successful merge
			CONFLICT   - conflict when merging
			REM-CONFLICT-removed in repository, but altered
				     locally.
			MOD-CONFLICT-removed locally, changed in repository.
                        REM-EXIST  - removed locally, but still exists.
			DIRCHANGE  - A change of directory.
			UNKNOWN	   - An unknown file.
			MOVE-AWAY  - A file that is in the way.
			REPOS-MISSING- The directory has vanished from the
				       repository.
                        MESSAGE    - This is a special fileinfo that is used
  				       to display a text that should be in
                                       full-log.
  dir		      Directory the file resides in. Should not end with slash.
  file-name	      The file name.
  backup-file	      Name of the backup file if MERGED or CONFLICT.
  cvs-diff-buffer     A buffer that contains a 'cvs diff file'.
  backup-diff-buffer  A buffer that contains a 'diff file backup-file'.
  full-log	      The output from cvs, unparsed.
  mod-time	      Modification time of file used for *-diff-buffer.
  handled	      True if this file doesn't require further action."
  (cons
   'CVS-FILEINFO
   (vector nil nil type dir file-name nil nil nil full-log nil)))


;;; Selectors:

(defun cvs-fileinfo->handled (cvs-fileinfo)
  "Get the  `handled' field from CVS-FILEINFO."
  (elt (cdr cvs-fileinfo) 0))

(defun cvs-fileinfo->marked (cvs-fileinfo)
  "Check if CVS-FILEINFO is marked."
  (elt (cdr cvs-fileinfo) 1))

(defun cvs-fileinfo->type (cvs-fileinfo)
  "Get type from CVS-FILEINFO.
Type is one of UPDATED, MODIFIED, ADDED, REMOVED, CVS-REMOVED, MERGED,
CONFLICT, REM-CONFLICT, MOD-CONFLICT, REM-EXIST, DIRCHANGE, UNKNOWN, MOVE-AWAY,
REPOS-MISSING or MESSAGE."
  (elt (cdr cvs-fileinfo) 2))

(defun cvs-fileinfo->dir (cvs-fileinfo)
  "Get dir from CVS-FILEINFO.
The directory name does not end with a slash. "
  (elt (cdr cvs-fileinfo) 3))

(defun cvs-fileinfo->file-name (cvs-fileinfo)
  "Get file-name from CVS-FILEINFO."
  (elt (cdr cvs-fileinfo) 4))

(defun cvs-fileinfo->base-revision (cvs-fileinfo)
  "Get the base revision from CVS-FILEINFO."
  (elt (cdr cvs-fileinfo) 5))

(defun cvs-fileinfo->cvs-diff-buffer (cvs-fileinfo)
  "Get cvs-diff-buffer from CVS-FILEINFO."
  (elt (cdr cvs-fileinfo) 6))

(defun cvs-fileinfo->backup-diff-buffer (cvs-fileinfo)
  "Get backup-diff-buffer from CVS-FILEINFO."
  (elt (cdr cvs-fileinfo) 7))

(defun cvs-fileinfo->full-log (cvs-fileinfo)
  "Get full-log from CVS-FILEINFO."
  (elt (cdr cvs-fileinfo) 8))

(defun cvs-fileinfo->mod-time (cvs-fileinfo)
  "Get mod-time from CVS-FILEINFO."
  (elt (cdr cvs-fileinfo) 9))

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

(defun cvs-set-fileinfo->base-revision (cvs-fileinfo newval)
  "Set base-revision in CVS-FILEINFO to NEWVAL."
  (aset (cdr cvs-fileinfo) 5 newval))

(defun cvs-set-fileinfo->cvs-diff-buffer (cvs-fileinfo newval)
  "Set cvs-diff-buffer in CVS-FILEINFO to NEWVAL."
  (aset (cdr cvs-fileinfo) 6 newval))

(defun cvs-set-fileinfo->backup-diff-buffer (cvs-fileinfo newval)
  "Set backup-diff-buffer in CVS-FILEINFO to NEWVAL."
  (aset (cdr cvs-fileinfo) 7 newval))

(defun cvs-set-fileinfo->full-log (cvs-fileinfo newval)
  "Set full-log in CVS-FILEINFO to NEWVAL."
  (aset (cdr cvs-fileinfo) 8 newval))

(defun cvs-set-fileinfo->mod-time (cvs-fileinfo newval)
  "Set full-log in CVS-FILEINFO to NEWVAL."
  (aset (cdr cvs-fileinfo) 9 newval))



;;; Predicate:

(defun cvs-fileinfo-p (object)
  "Return t if OBJECT is a cvs-fileinfo."
  (eq (car-safe object) 'CVS-FILEINFO))

;;;; End of types.

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

; Too complicated to handle all the cases that are generated.
; Maybe later.
;(defun cvs-examine (directory &optional local)
;  "Run a 'cvs -n update' in the current working directory.
;That is, check what needs to be done, but don't change the disc.
;Feed the output to a *cvs* buffer and run cvs-mode on it.
;If optional prefix argument LOCAL is non-nil, 'cvs update -l' is run."
;  (interactive (list (read-file-name "CVS Update (directory): "
;				     nil default-directory nil)
;		     current-prefix-arg))
;  (cvs-do-update directory local 'noupdate))

(defun cvs-update (directory &optional local)
  "Run a 'cvs update' in the current working directory. Feed the
output to a *cvs* buffer and run cvs-mode on it.
If optional prefix argument LOCAL is non-nil, 'cvs update -l' is run."
  (interactive (list (read-file-name "CVS Update (directory): "
				     nil default-directory nil)
		     current-prefix-arg))
  (cvs-do-update directory local nil)
  (switch-to-buffer cvs-buffer-name))

(defun cvs-update-other-window (directory &optional local)
  "Run a 'cvs update' in the current working directory. Feed the
output to a *cvs* buffer, display it in the other window, and run
cvs-mode on it.

If optional prefix argument LOCAL is non-nil, 'cvs update -l' is run."
  (interactive (list (read-file-name "CVS Update other window (directory): "
				     nil default-directory nil)
		     current-prefix-arg))
  (cvs-do-update directory local nil)
  (switch-to-buffer-other-window cvs-buffer-name))

(defun cvs-filter (predicate list &rest extra-args)
  "Apply PREDICATE to each element on LIST.
Args: PREDICATE LIST &rest EXTRA-ARGS.
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

(defun cvs-mode-update-no-prompt ()
  "Run cvs update in current directory."
  (interactive)
  (cvs-do-update default-directory nil nil))

(defun cvs-do-update (directory local dont-change-disc)
  "Do a 'cvs update' in DIRECTORY.
Args: DIRECTORY LOCAL DONT-CHANGE-DISC &optional NOTTHISWINDOW.
If LOCAL is non-nil 'cvs update -l' is executed.
If DONT-CHANGE-DISC is non-nil 'cvs -n update' is executed.
Both LOCAL and DONT-CHANGE-DISC may be non-nil simultaneously.

*Note*: DONT-CHANGE-DISC does not yet work. The parser gets confused."
  (save-some-buffers)
  (if (not (file-exists-p cvs-program))
      (error "%s: file not found (check setting of cvs-program)"
	     cvs-program))
  (if (and cvs-cvsroot-required
	   (not (or (getenv "CVSROOT") cvs-cvsroot)))
      (error "Both cvs-cvsroot and environment variable CVSROOT unset."))
  (let* ((this-dir (file-name-as-directory (expand-file-name directory)))
	 (update-buffer (generate-new-buffer
			 (concat (file-name-nondirectory
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

    ;; Check that at most one `cvs update' is run at any time.

    (if (and cvs-update-running (process-status cvs-update-running)
	     (or (eq (process-status cvs-update-running) 'run)
		 (eq (process-status cvs-update-running) 'stop)))
	(error "Can't run two `cvs update' simultaneously."))

    ;; Generate "-d /master -n update -l".
    (setq args (concat (if cvs-cvsroot (concat " -d " cvs-cvsroot))
		       (if dont-change-disc " -n ")
		       " update "
		       (if local " -l ")))

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
	   cvs-startup-message		;Se comment above cvs-startup-message.
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
  ;; emacses. It shouldn't be needed, but it does no harm.
  (sit-for 0))


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


(put 'cvs-mode 'mode-class 'special)

(defun cvs-mode ()
  "\\<cvs-mode-map>Mode used for pcl-cvs, a frontend to CVS.

To get the *cvs* buffer you should use ``\\[cvs-update]''.

Full documentation is in the Texinfo file.  These are the most useful commands:

\\[cvs-mode-previous-line] Move up.                    \\[cvs-mode-next-line] Move down.
\\[cvs-mode-commit]   Commit file.                \\[cvs-mode-update-no-prompt]   Reupdate directory.
\\[cvs-mode-mark]   Mark file/dir.              \\[cvs-mode-unmark]   Unmark file/dir.
\\[cvs-mode-mark-all-files]   Mark all files.             \\[cvs-mode-unmark-all-files]   Unmark all files.
\\[cvs-mode-find-file]   Edit file/run Dired.        \\[cvs-mode-find-file-other-window]   Find file or run Dired in other window.
\\[cvs-mode-remove-handled]   Remove processed entries.   \\[cvs-mode-add-change-log-entry-other-window]   Write ChangeLog in other window.
\\[cvs-mode-add]   Add to repository.          \\[cvs-mode-remove-file]   Remove file.
\\[cvs-mode-diff-cvs]   Diff between base revision. \\[cvs-mode-diff-backup]   Diff backup file.
\\[cvs-mode-emerge]   Run emerge on base revision/backup file.
\\[cvs-mode-acknowledge] Delete line from buffer.    \\[cvs-mode-ignore]   Add file to the .cvsignore file.
\\[cvs-mode-log]   Run ``cvs log''.            \\[cvs-mode-status]   Run ``cvs status''.
\\[cvs-mode-changelog-commit]  Like \\[cvs-mode-commit], but get default log text from ChangeLog.
\\[cvs-mode-undo-local-changes]  Revert the last checked in version - discard your changes to the file.

Entry to this mode runs cvs-mode-hook.
This description is updated for release 1.05 of pcl-cvs.

All bindings:
\\{cvs-mode-map}"
  (interactive)
  (setq major-mode 'cvs-mode)
  (setq mode-name "CVS")
  (setq mode-line-process nil)
  (buffer-flush-undo (current-buffer))
  (make-local-variable 'goal-column)
  (setq goal-column cvs-cursor-column)
  (use-local-map cvs-mode-map)
  (run-hooks 'cvs-mode-hook))

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

(defun cvs-update-filter (proc string)
  "Filter function for pcl-cvs.
This function gets the output that CVS sends to stderr. It inserts it
into (process-buffer proc) but it also checks if CVS is waiting for a
lock file. If so, it inserts a message cookie in the *cvs* buffer."
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
	       "^cvs update: \\[..:..:..\\] waiting \
for \\(.*\\)lock in \\(.*\\)$")
	      (setq cvs-lock-file (buffer-substring (match-beginning 2)
						    (match-end 2)))
	      (cookie-enter-last
	       cvs-cookie-handle
	       (cvs-create-fileinfo
		'MESSAGE nil nil
		(concat "\tWaiting for "
			(buffer-substring (match-beginning 1)
					  (match-end 1))
			"lock in " cvs-lock-file
			".\n\t (type M-x cvs-delete-lock to delete it)")))))))
      (store-match-data data)
      (set-buffer old-buffer))))

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

(defun cvs-remove-directory (dir)
  "Remove a directory."
  (if (file-directory-p dir)
      (call-process cvs-rmdir-program nil nil nil dir)
    (error "Not a directory: %s" dir))
  (if (file-exists-p dir)
      (error "Could not remove directory %s" dir)))

(defun cvs-lock-file-p (file)
  "Return true if FILE looks like a CVS lock file."
  (or
   (string-match "^#cvs.tfl.[0-9]+$" file)
   (string-match "^#cvs.rfl.[0-9]+$" file)
   (string-match "^#cvs.wfl.[0-9]+$" file)))

(defun cvs-skip-line (stdout stderr regexp &optional arg)
  "Like forward-line, but check that the skipped line matches REGEXP.
Args: STDOUT STDERR REGEXP &optional ARG.

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
    (cvs-parse-error stdout stderr
		     (if (eq (current-buffer) stdout) 'STDOUT 'STDERR)
		     (point)))))

(defun cvs-get-current-dir (root-dir dirname)
  "Return current working directory, suitable for cvs-parse-update.
Args: ROOT-DIR DIRNAME.
Concatenates ROOT-DIR and DIRNAME to form an absolute path."
  (if (string= "." dirname)
      (substring root-dir 0 -1)
    (concat root-dir dirname)))

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

(defun cvs-parse-error (stdout-buffer stderr-buffer err-buf pos)
  "Handle a parse error when parsing the output from cvs.
Args: STDOUT-BUFFER STDERR-BUFFER ERR-BUF POS.
ERR-BUF should be 'STDOUT or 'STDERR."
  (setq pos (1- pos))
  (set-buffer cvs-buffer-name)
  (setq buffer-read-only nil)
  (erase-buffer)
  (insert "To: ceder@lysator.liu.se\n")
  (insert "Subject: pcl-cvs " pcl-cvs-version " parse error.\n")
  (insert "--text follows this line--\n\n")
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
  (insert "Please state the version of these programs you are using:\n")
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
    (insert "Emacs-version: " (emacs-version) "\n")
    (insert "Pcl-cvs $" "Id:" "$" ": " "Id: pcl-cvs.el,v 1.93 1993/05/31 22:44:00 ceder Exp \n")
    (insert "\n")
    (insert (format "--- Contents of stdout buffer (%d chars) ---\n"
		    (length stdout)))
    (insert stdout)
    (insert "--- End of stdout buffer ---\n")
    (insert (format "--- Contents of stderr buffer (%d chars) ---\n"
		    (length stderr)))
    (insert stderr)
    (insert "--- End of stderr buffer ---\n")
    (insert "End of bug report.\n")
    (require 'sendmail)
    (mail-mode)
    (error "CVS parse error - please report this bug.")))
      
(defun cvs-parse-update (stdout-buffer stderr-buffer)
  "Parse the output from `cvs update'.

Args: STDOUT-BUFFER STDERR-BUFFER.

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
    (cvs-remove-empty-directories)
    (set-buffer cvs-buffer-name)
    (cvs-mode)
    (goto-char (point-min))
    (tin-goto-previous cvs-cookie-handle (point-min) 1)
    (setq default-directory root-dir)))

(defun cvs-remove-stdout-shadows ()
  "Remove entries in the *cvs* buffer that comes from both stdout and stderr.
If there is two entries for a single file the second one should be
deleted. (Remember that sort uses a stable sort algorithm, so one can
be sure that the stderr entry is always first)."
  (collection-filter-tins cvs-cookie-handle
	      (function
	       (lambda (tin)
		 (not (cvs-shadow-entry-p tin))))))

(defun cvs-shadow-entry-p (tin)
  "Return non-nil if TIN is a shadow entry.
Args: TIN.
A TIN is a shadow entry if the previous tin contains the same file."
  (let* ((previous-tin (tin-previous cvs-cookie-handle tin))
	 (curr (tin-cookie cvs-cookie-handle tin))
	 (prev (and previous-tin
		    (tin-cookie cvs-cookie-handle previous-tin))))
    (and
     prev curr
     (string= (cvs-fileinfo->file-name prev) (cvs-fileinfo->file-name curr))
     (string= (cvs-fileinfo->dir prev)       (cvs-fileinfo->dir curr))
     (or
      (and (eq (cvs-fileinfo->type prev) 'CONFLICT)
	   (eq (cvs-fileinfo->type curr) 'CONFLICT))
      (and (eq (cvs-fileinfo->type prev) 'MERGED)
	   (eq (cvs-fileinfo->type curr) 'MODIFIED))
      (and (eq (cvs-fileinfo->type prev) 'REM-EXIST)
	   (eq (cvs-fileinfo->type curr) 'REMOVED))))))


(defun cvs-parse-stderr (stdout-buffer stderr-buffer head dir)
  "Parse the output from CVS that is written to stderr.
Args: STDOUT-BUFFER STDERR-BUFFER HEAD DIR
STDOUT-BUFFER holds the output that cvs sent to stdout. It is only
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

	 ;; RCVS support (for now, we simply ignore any output from
	 ;; RCVS, including error messages!)

	 ((looking-at "updating of .* finished$")
	  (forward-line 1))

	 ((looking-at "REMOTE FOLDER:.*")
	  (forward-line 1)
	  (while (and (< (point) (point-max)) (not (looking-at "phase 2.*")))
	    (forward-line 1))
	  (forward-line 2))

	 ((looking-at "turn on remote mode$")
	  (forward-line 1)
	  (while (and (< (point) (point-max)) (not (looking-at "phase 2.*")))
	    (forward-line 1))
	  (forward-line 2))

	 ((looking-at "phase 3.*")
	  (goto-char (point-max)))

	 ;; End of RCVS stuff.

	 ;; CVS is descending a subdirectory.
	 ;; (The "server" case is there to support Cyclic CVS.)
	 ((looking-at "cvs \\(update\\|server\\): Updating \\(.*\\)$")
	  (setq current-dir
		(cvs-get-current-dir
		 root-dir
		 (buffer-substring (match-beginning 2) (match-end 2))))
	  (setcdr head (list (cvs-create-fileinfo
			      'DIRCHANGE current-dir
			      nil (buffer-substring (match-beginning 0)
						    (match-end 0)))))
	  (setq head (cdr head))
	  (forward-line 1))

	 ;; File removed, since it is removed (by third party) in repository.
       
	 ((or (looking-at
	       "cvs update: warning: \\(.*\\) is not (any longer) pertinent")
	      (looking-at
	       "cvs update: \\(.*\\) is no longer in the repository"))

	  (setcdr head (list (cvs-create-fileinfo
			      'CVS-REMOVED current-dir
			      (file-name-nondirectory
			       (buffer-substring (match-beginning 1)
						 (match-end 1)))
			      (buffer-substring (match-beginning 0)
						(match-end 0)))))
	  (setq head (cdr head))
	  (forward-line 1))

	 ;; File removed by you, but recreated by cvs. Ignored.

	 ((looking-at "cvs update: warning: .* was lost$")
	  (forward-line 1))

	 ;; A file that has been created by you, but added to the cvs
	 ;; repository by another.

	 ((looking-at "^cvs update: move away \\(.*\\); it is in the way$")
	  (setcdr head (list (cvs-create-fileinfo
			      'MOVE-AWAY current-dir
			      (file-name-nondirectory
			       (buffer-substring (match-beginning 1)
						 (match-end 1)))
			      (buffer-substring (match-beginning 0)
						(match-end 0)))))
	  (setq head (cdr head))
	  (forward-line 1))

	 ;; Empty line. Probably inserted by mistake by user (or developer :-)
	 ;; Ignore.

	 ((looking-at "^$")
	  (forward-line 1))

	 ;; Cvs waits for a lock. Ignore.

	 ((looking-at
	   "^cvs update: \\[..:..:..\\] waiting for .*lock in ")
	  (forward-line 1))

	 ;; File removed in repository, but edited by you.

	 ((looking-at
	   "cvs update: conflict: \\(.*\\) is modified but no longer \
in the repository$")
	  (setcdr head (list
			(cvs-create-fileinfo
			 'REM-CONFLICT current-dir
			 (file-name-nondirectory
			  (buffer-substring (match-beginning 1) (match-end 1)))
			 (buffer-substring (match-beginning 0)
					   (match-end 0)))))
	  (setq head (cdr head))
	  (forward-line 1))

	 ((looking-at
	   "cvs update: conflict: removed \\(.*\\) was modified by \
second party")
	  (setcdr head
		  (list
		   (cvs-create-fileinfo
		    'MOD-CONFLICT current-dir
		    (buffer-substring (match-beginning 1) (match-end 1))
		    (buffer-substring (match-beginning 0) (match-end 0)))))
	  (setq head (cdr head))
	  (forward-line 1))

	 ((looking-at
	   "cvs update: \\(.*\\) should be removed and is still there")
	  (setcdr head
		  (list
		   (cvs-create-fileinfo
		    'REM-EXIST current-dir
		    (buffer-substring (match-beginning 1) (match-end 1))
		    (buffer-substring (match-beginning 0) (match-end 0)))))
	  (setq head (cdr head))
	  (forward-line 1))

	 ((looking-at "cvs update: in directory ")
	  (let ((start (point)))
	    (forward-line 1)
	    (cvs-skip-line
	     stdout-buffer stderr-buffer
	     (regexp-quote "cvs [update aborted]: there is no repository "))
	    (setcdr head (list
			  (cvs-create-fileinfo
			   'REPOS-MISSING current-dir
			   nil
			   (buffer-substring start (point)))))
	    (setq head (cdr head))))

	 ;; Ignore other messages from Cyclic CVS.
	 ((looking-at "cvs server:")
	  (forward-line 1))

	 (t

	  ;; CVS has decided to merge someone elses changes into this
	  ;; document. This leads to a lot of garbage being printed.
	  ;; First there is two lines that contains no information
	  ;; that we skip (but we check that we recognize them).

	  (let ((complex-start (point))
		initial-revision filename)

	    (cvs-skip-line stdout-buffer stderr-buffer "^RCS file: .*$")
	    (setq initial-revision
		  (cvs-skip-line stdout-buffer stderr-buffer
				 "^retrieving revision \\(.*\\)$" 1))
	    (cvs-skip-line stdout-buffer stderr-buffer
			   "^retrieving revision .*$")

	    ;; Get the file name from the next line.

	    (setq
	     filename
	     (cvs-skip-line
	      stdout-buffer stderr-buffer
	      "^Merging differences between [0-9.]+ and [0-9.]+ into \\(.*\\)$"
	      1))

	    (cond
	     ;; Was it a conflict?
	     ((looking-at
	       ;; Allow both RCS 5.5 and 5.6. (5.6 prints "rcs" and " warning").
	       "^\\(rcs\\)?merge:?\\( warning\\)?: \\(overlaps\\|conflicts\\) during merge$")

	      ;; Yes, this is a conflict.
	      (cvs-skip-line
	       stdout-buffer stderr-buffer
	       "^\\(rcs\\)?merge:?\\( warning\\)?: \\(overlaps\\|conflicts\\) during merge$")

	      (cvs-skip-line stdout-buffer stderr-buffer
			     "^cvs \\(update\\|server\\): conflicts found in ")

	      (let ((fileinfo
		     (cvs-create-fileinfo
		      'CONFLICT current-dir
		      filename
		      (buffer-substring complex-start (point)))))

		(cvs-set-fileinfo->base-revision fileinfo initial-revision)

		(setcdr head (list fileinfo))
		(setq head (cdr head))))

	     ;; Was it a conflict, and was RCS compiled without DIFF3_BIN?

	     ((looking-at
	       ;; Allow both RCS 5.5 and 5.6. (5.6 prints "rcs" and " warning").
	       "^\\(rcs\\)?merge\\( warning\\)?: overlaps or other probl\
ems during merge$")

	      ;; Yes, this is a conflict.
	      (cvs-skip-line
	       stdout-buffer stderr-buffer
	       "^\\(rcs\\)?merge\\( warning\\)?: overlaps .*during merge$")

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

	     (t
	      ;; Not a conflict; it must be a succesful merge.
	      (let ((fileinfo
		     (cvs-create-fileinfo
		      'MERGED current-dir
		      filename
		      (buffer-substring complex-start (point)))))
		(cvs-set-fileinfo->base-revision fileinfo initial-revision)
		(setcdr head (list fileinfo))
		(setq head (cdr head)))))))))))
  head)


(defun cvs-parse-stdout (stdout-buffer stderr-buffer head root-dir)
  "Parse the output from CVS that is written to stdout.
Args: STDOUT-BUFFER STDERR-BUFFER HEAD ROOT-DIR
STDOUT-BUFFER is the buffer that holds the output to parse.
STDERR-BUFFER holds the output that cvs sent to stderr. It is only
used to create a bug report in case there is a parse error.

HEAD is a cons-cell, the head of the list that is built.
ROOT-DIR is the directory the `cvs update' was run in.

This function doesn't return anything particular."
  (save-window-excursion
    (set-buffer stdout-buffer)
    (goto-char (point-min))
    (while (< (point) (point-max))
      (cond

       ;; M: The file is modified by the user, and untouched in the repository.
       ;; A: The file is "cvs add"ed, but not "cvs ci"ed.
       ;; R: The file is "cvs remove"ed, but not "cvs ci"ed.
       ;; C: Conflict
       ;; U, P: The file is copied from the repository.
       ;; ?: Unknown file.


       ((looking-at "\\([MARCUP?]\\) \\(.*\\)$")
	(let*
	    ((c         (char-after (match-beginning 1)))
	     (full-path
	      (concat (file-name-as-directory root-dir)
		      (buffer-substring (match-beginning 2) (match-end 2))))
	     (fileinfo (cvs-create-fileinfo
			(cond ((eq c ?M) 'MODIFIED)
			      ((eq c ?A) 'ADDED)
			      ((eq c ?R) 'REMOVED)
			      ((eq c ?C) 'CONFLICT)
			      ((eq c ?U) 'UPDATED)
			      ;; generated when Cyclic CVS sends a
			      ;; patch instead of the full file:
			      ((eq c ?P) 'UPDATED)
			      ((eq c ??) 'UNKNOWN))
			(substring (file-name-directory full-path) 0 -1)
			(file-name-nondirectory full-path)
			(buffer-substring (match-beginning 0) (match-end 0)))))
	  ;; Updated files require no further action.
	  (if (memq c '(?U ?P))
	      (cvs-set-fileinfo->handled fileinfo t))

	  ;; Link this last on the list.
	  (setcdr head (list fileinfo))
	  (setq head (cdr head))
	  (forward-line 1)))

       ;; Executing a program because of the -u option in modules.
       ((looking-at "cvs update: Executing")
	;; Skip by any output the program may generate to stdout.
	;; Note that pcl-cvs will get seriously confused if the
	;; program prints anything to stderr.
	(re-search-forward cvs-update-prog-output-skip-regexp)
	(forward-line 1))

       (t (cvs-parse-error stdout-buffer stderr-buffer 'STDOUT (point)))))))

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
      ((eq a 'CVS-REMOVED)
       (format "%s Removed from repository:  %s" s f))
      ((eq a 'REM-CONFLICT)
       (format "%s Conflict: Removed from repository, changed by you: %s" s f))
      ((eq a 'MOD-CONFLICT)
       (format "%s Conflict: Removed by you, changed in repository: %s" s f))
      ((eq a 'REM-EXIST)
       (format "%s Conflict: Removed by you, but still exists: %s" s f))
      ((eq a 'DIRCHANGE)
       (format "\nIn directory %s:"
	       (cvs-fileinfo->dir fileinfo)))
      ((eq a 'MOVE-AWAY)
       (format "%s Move away %s - it is in the way" s f))
      ((eq a 'REPOS-MISSING)
       (format "  This repository is missing! Remove this dir manually."))
      ((eq a 'MESSAGE)
       (cvs-fileinfo->full-log fileinfo))
      (t
       (format "%s Internal error! %s" s f))))))


;;; You can define your own keymap in .emacs. pcl-cvs.el won't overwrite it.

(if cvs-mode-map
    nil
  (setq cvs-mode-map (make-keymap))
  (suppress-keymap cvs-mode-map)
  (define-key cvs-mode-map " "	'cvs-mode-next-line)
  (define-key cvs-mode-map "?"	'describe-mode)
  (define-key cvs-mode-map "A"	'cvs-mode-add-change-log-entry-other-window)
  (define-key cvs-mode-map "M"	'cvs-mode-mark-all-files)
  (define-key cvs-mode-map "R"	'cvs-mode-revert-updated-buffers)
  (define-key cvs-mode-map "U"	'cvs-mode-undo-local-changes)
  (define-key cvs-mode-map "\C-?"  'cvs-mode-unmark-up)
  (define-key cvs-mode-map "\C-k"  'cvs-mode-acknowledge)
  (define-key cvs-mode-map "\C-n"  'cvs-mode-next-line)
  (define-key cvs-mode-map "\C-p"  'cvs-mode-previous-line)
  (define-key cvs-mode-map "\M-\C-?" 'cvs-mode-unmark-all-files)
  (define-key cvs-mode-map "a"	'cvs-mode-add)
  (define-key cvs-mode-map "b"	'cvs-mode-diff-backup)
  (define-key cvs-mode-map "c"	'cvs-mode-commit)
  (define-key cvs-mode-map "C"  'cvs-mode-changelog-commit)
  (define-key cvs-mode-map "d"	'cvs-mode-diff-cvs)
  (define-key cvs-mode-map "e"	'cvs-mode-emerge)
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
  (define-key cvs-mode-map "x"	'cvs-mode-remove-handled)
  (define-key cvs-mode-map "u"	'cvs-mode-unmark))


(defun cvs-get-marked (&optional ignore-marks)
  "Return a list of all selected tins.
If there are any marked tins, and IGNORE-MARKS is nil, return them.
Otherwise, if the cursor selects a directory, return all files in it.
Otherwise return (a list containing) the file the cursor points to, or
an empty list if it doesn't point to a file at all.

Args: &optional IGNORE-MARKS."
    
  (cond
   ;; Any marked cookies?
   ((and (not ignore-marks)
	 (collection-collect-tin cvs-cookie-handle 'cvs-fileinfo->marked)))
   ;; Nope.
   (t
    (let ((sel (tin-locate cvs-cookie-handle (point))))
      (cond
       ;; If a directory is selected, all it members are returned.
       ((and sel (eq (cvs-fileinfo->type
			(tin-cookie cvs-cookie-handle sel))
		       'DIRCHANGE))
	(collection-collect-tin
	 cvs-cookie-handle 'cvs-dir-member-p
	 (cvs-fileinfo->dir (tin-cookie cvs-cookie-handle sel))))
       (t
	(list sel)))))))


(defun cvs-dir-member-p (fileinfo dir)
  "Return true if FILEINFO represents a file in directory DIR."
  (and (not (eq (cvs-fileinfo->type fileinfo) 'DIRCHANGE))
       (string= (cvs-fileinfo->dir fileinfo) dir)))

(defun cvs-dir-empty-p (tin)
  "Return non-nil if TIN is a directory that is empty.
Args: CVS-BUF TIN."
  (and (eq (cvs-fileinfo->type (tin-cookie cvs-cookie-handle tin)) 'DIRCHANGE)
       (or (not (tin-next cvs-cookie-handle tin))
	   (eq (cvs-fileinfo->type
		(tin-cookie cvs-cookie-handle
				    (tin-next cvs-cookie-handle tin)))
	       'DIRCHANGE))))

(defun cvs-mode-revert-updated-buffers ()
  "Revert any buffers that are UPDATED, MERGED or CONFLICT."
  (interactive)
  (cookie-map (function cvs-revert-fileinfo) cvs-cookie-handle))

(defun cvs-revert-fileinfo (fileinfo)
  "Revert the buffer that holds the file in FILEINFO if it has changed,
and if the type is UPDATED, MERGED or CONFLICT."
  (let* ((type (cvs-fileinfo->type fileinfo))
	 (file (cvs-fileinfo->full-path fileinfo))
	 (buffer (get-file-buffer file)))
    ;; For a revert to happen...
    (cond
     ((and
       ;; ...the type must be one that justifies a revert...
       (or (eq type 'UPDATED)
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


(defun cvs-mode-remove-handled ()
  "Remove all lines that are handled.
Empty directories are removed."
  (interactive)
  ;; Pass one: remove files that are handled.
  (collection-filter-cookies cvs-cookie-handle
		 (function
		  (lambda (fileinfo) (not (cvs-fileinfo->handled fileinfo)))))
  ;; Pass two: remove empty directories.
  (cvs-remove-empty-directories))


(defun cvs-remove-empty-directories ()
  "Remove empty directories in the *cvs* buffer."
  (collection-filter-tins cvs-cookie-handle
		     (function
		      (lambda (tin)
			(not (cvs-dir-empty-p tin))))))

(defun cvs-mode-mark (pos)
  "Mark a fileinfo. Args: POS.
If the fileinfo is a directory, all the contents of that directory are
marked instead. A directory can never be marked.
POS is a buffer position."

  (interactive "d")

  (let* ((tin (tin-locate cvs-cookie-handle pos))
	 (sel (tin-cookie cvs-cookie-handle tin)))

    (cond
     ;; Does POS point to a directory? If so, mark all files in that directory.
     ((eq (cvs-fileinfo->type sel) 'DIRCHANGE)
      (cookie-map
       (function (lambda (f dir)
		   (cond
		    ((cvs-dir-member-p f dir)
		     (cvs-set-fileinfo->marked f t)
		     t))))		;Tell cookie to redisplay this cookie.
       cvs-cookie-handle
       (cvs-fileinfo->dir sel)))
     (t
      (cvs-set-fileinfo->marked sel t)
      (tin-invalidate cvs-cookie-handle tin)
      (tin-goto-next cvs-cookie-handle pos 1)))))
  

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


(defun cvs-edit-done ()
  "Commit the files to the repository."
  (interactive)
  (if (null cvs-commit-list)
      (error "You have already commited the files"))
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
			    (list "commit" "-m" msg)))
	(error "Something went wrong. Check the %s buffer carefully."
	       cvs-temp-buffer-name))
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

(defun cvs-after-commit-function (fileinfo)
  "Do everything that needs to be done when FILEINFO has been commited.
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


(defun cvs-execute-list (tin-list program constant-args)
  "Run PROGRAM on all elements on TIN-LIST.
Args: TIN-LIST PROGRAM CONSTANT-ARGS
The PROGRAM will be called with pwd set to the directory the
files reside in. CONSTANT-ARGS should be a list of strings. The
arguments given to the program will be CONSTANT-ARGS followed by all
the files (from TIN-LIST) that resides in that directory. If the files
in TIN-LIST resides in different directories the PROGRAM will be run
once for each directory (if all files in the same directory appears
after each other).

Any output from PROGRAM will be inserted in the current buffer.

This function return nil if all went well, or the numerical exit
status or a signal name as a string. Note that PROGRAM might be called
several times. This will return non-nil if something goes wrong, but
there is no way to know which process that failed."

  (let ((exitstatus nil))
    (while tin-list
      (let ((current-dir (cvs-fileinfo->dir
			  (tin-cookie cvs-cookie-handle
					 (car tin-list))))
	    arg-list arg-str)

	;; Collect all marked files in this directory.

	(while (and tin-list
		    (string=
		     current-dir
		     (cvs-fileinfo->dir
		      (tin-cookie cvs-cookie-handle (car tin-list)))))
	  (setq arg-list
		(cons (cvs-fileinfo->file-name
		       (tin-cookie cvs-cookie-handle (car tin-list)))
		      arg-list))
	  (setq tin-list (cdr tin-list)))

	(setq arg-list (nreverse arg-list))

	;; Execute the command on all the files that were collected.

	(setq default-directory (file-name-as-directory current-dir))
	(insert (format "=== cd %s\n" default-directory))
	(insert (format "=== %s %s\n\n"
			program
			(mapconcat '(lambda (foo) foo)
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
	(goto-char (point-max))))
    exitstatus))


(defun cvs-execute-single-file-list (tin-list extractor program constant-args)
  "Run PROGRAM on all elements on TIN-LIST.

Args: TIN-LIST EXTRACTOR PROGRAM CONSTANT-ARGS

The PROGRAM will be called with pwd set to the directory the files
reside in.  CONSTANT-ARGS is a list of strings to pass as arguments to
PROGRAM.  The arguments given to the program will be CONSTANT-ARGS
followed by the list that EXTRACTOR returns.

EXTRACTOR will be called once for each file on TIN-LIST.  It is given
one argument, the cvs-fileinfo.  It can return t, which means ignore
this file, or a list of arguments to send to the program."

    (while tin-list
      (let ((default-directory (file-name-as-directory
				(cvs-fileinfo->dir
				 (tin-cookie cvs-cookie-handle
						(car tin-list)))))
	    (arg-list
	     (funcall extractor
		      (tin-cookie cvs-cookie-handle (car tin-list)))))

	;; Execute the command unless extractor returned t.

	(if (eq arg-list t)
	    nil
	  (insert (format "=== cd %s\n" default-directory))
	  (insert (format "=== %s %s\n\n"
			  program
			  (mapconcat '(lambda (foo) foo)
				     (nconc (copy-sequence constant-args)
					    arg-list)
				     " ")))
	  (apply 'call-process program nil t t
		 (nconc (copy-sequence constant-args) arg-list))
	  (goto-char (point-max))))
      (setq tin-list (cdr tin-list))))


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


(if cvs-edit-mode-map
    nil
  (setq cvs-edit-mode-map (make-sparse-keymap))
  (define-prefix-command 'cvs-control-c-prefix)
  (define-key cvs-edit-mode-map "\C-c" 'cvs-control-c-prefix)
  (define-key cvs-edit-mode-map "\C-c\C-c" 'cvs-edit-done))


(defun cvs-diffable (tins)
  "Return a list of all tins on TINS that it makes sense to run
``cvs diff'' on."
  ;; +++ There is an unnecessary (nreverse) here. Get the list the
  ;; other way around instead!
  (let ((result nil))
    (while tins
      (let ((type (cvs-fileinfo->type
		   (tin-cookie cvs-cookie-handle (car tins)))))
	(if (or (eq type 'MODIFIED)
		(eq type 'UPDATED)
		(eq type 'MERGED)
		(eq type 'CONFLICT)
		(eq type 'REMOVED)	;+++Does this line make sense?
		(eq type 'ADDED))	;+++Does this line make sense?
	    (setq result (cons (car tins) result)))
	(setq tins (cdr tins))))
    (nreverse result)))
	  
				     
(defun cvs-mode-diff-cvs (&optional ignore-marks)
  "Diff the selected files against the repository.
The flags in the variable cvs-diff-flags (which should be a list
of strings) will be passed to ``cvs diff''.  If the variable
cvs-diff-ignore-marks is non-nil any marked files will not be
considered to be selected.  An optional prefix argument will invert
the influence from cvs-diff-ignore-marks."

  (interactive "P")

  (if (not (listp cvs-diff-flags))
      (error "cvs-diff-flags should be a list of strings"))

  (save-some-buffers)
  (let ((marked (cvs-diffable
		 (cvs-get-marked
		  (or (and ignore-marks (not cvs-diff-ignore-marks))
		      (and (not ignore-marks) cvs-diff-ignore-marks))))))
    (cvs-use-temp-buffer)
    (message "cvsdiffing...")
    ;; Don't care much about the exit status since it is the _sum_ of
    ;; the status codes from the different files (not the _max_ as it
    ;; should be).
    (if (cvs-execute-list marked cvs-program
			  (if cvs-cvsroot
			      (cons "-d" (cons cvs-cvsroot
					       (cons "diff" cvs-diff-flags)))
			    (cons "diff" cvs-diff-flags)))
	(message "cvsdiffing... Done.")
      (message "cvsdiffing... No differences found."))))


(defun cvs-backup-diffable (tin)
  "Check if the TIN is backup-diffable.
It must have a backup file to be diffable."
  (file-readable-p
   (cvs-fileinfo->backup-file (tin-cookie cvs-cookie-handle tin))))

(defun cvs-mode-diff-backup (&optional ignore-marks)
  "Diff the files against the backup file.
This command can be used on files that are marked with \"Merged\"
or \"Conflict\" in the *cvs* buffer.

If the variable cvs-diff-ignore-marks is non-nil any marked files will
not be considered to be selected.  An optional prefix argument will
invert the influence from cvs-diff-ignore-marks.

The flags in cvs-diff-flags will be passed to ``diff''."

  (interactive "P")

  (if (not (listp cvs-diff-flags))
      (error "cvs-diff-flags should be a list of strings."))

  (save-some-buffers)
  (let ((marked (cvs-filter
		 (function cvs-backup-diffable)
		 (cvs-get-marked
		  (or
		   (and ignore-marks (not cvs-diff-ignore-marks))
		   (and (not ignore-marks) cvs-diff-ignore-marks))))))
    (if (null marked)
	(error "No ``Conflict'' or ``Merged'' file selected!"))
    (cvs-use-temp-buffer)
    (message "diffing...")
    (cvs-execute-single-file-list
     marked 'cvs-diff-backup-extractor cvs-diff-program cvs-diff-flags))
  (message "diffing... Done."))


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

(defun cvs-mode-find-file-other-window (pos)
  "Select a buffer containing the file in another window.
Args: POS"
  (interactive "d")
  (let ((tin (tin-locate cvs-cookie-handle pos)))
    (if tin
	(let ((type (cvs-fileinfo->type (tin-cookie cvs-cookie-handle
							    tin))))
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

(defun cvs-fileinfo->full-path (fileinfo)
  "Return the full path for the file that is described in FILEINFO."
  (concat
   (file-name-as-directory
    (cvs-fileinfo->dir fileinfo))
   (cvs-fileinfo->file-name fileinfo)))

(defun cvs-full-path (tin)
  "Return the full path for the file that is described in TIN."
  (cvs-fileinfo->full-path (tin-cookie cvs-cookie-handle tin)))

(defun cvs-mode-find-file (pos)
  "Select a buffer containing the file in another window.
Args: POS"
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


(defun cvs-mode-unmark (pos)
  "Unmark a fileinfo. Args: POS."
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

(defun cvs-mode-unmark-all-files ()
  "Unmark all files.
Directories are also unmarked, but that doesn't matter, since
they should always be unmarked."
  (interactive)
  (cookie-map (function (lambda (cookie)
			  (cvs-set-fileinfo->marked cookie nil)
			  t))
	      cvs-cookie-handle))


(defun cvs-do-removal (tins)
  "Remove files.
Args: TINS.
TINS is a list of tins that the
user wants to delete. The files are deleted. If the type of
the tin is 'UNKNOWN the tin is removed from the buffer. If it
is anything else the file is added to a list that should be
`cvs remove'd and the tin is changed to be of type 'REMOVED.

Returns a list of tins files that should be `cvs remove'd."
  (cvs-use-temp-buffer)
  (mapcar 'cvs-insert-full-path tins)
  (cond
   ((and tins (yes-or-no-p (format "Delete %d files? " (length tins))))
    (let (files-to-remove)
      (while tins
	(let* ((tin (car tins))
	       (fileinfo (tin-cookie cvs-cookie-handle tin))
	       (type (cvs-fileinfo->type fileinfo)))
	  (if (not (or (eq type 'REMOVED) (eq type 'CVS-REMOVED)))
	      (progn
		(delete-file (cvs-full-path tin))
		(cond
		 ((or (eq type 'UNKNOWN) (eq type 'MOVE-AWAY))
		  (tin-delete cvs-cookie-handle tin))
		 (t
		  (setq files-to-remove (cons tin files-to-remove))
		  (cvs-set-fileinfo->type fileinfo 'REMOVED)
		  (cvs-set-fileinfo->handled fileinfo nil)
		  (tin-invalidate cvs-cookie-handle tin))))))
	(setq tins (cdr tins)))
      files-to-remove))
   (t nil)))



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
			      '("remove")))
	  (error "CVS exited with non-zero exit status.")
	(message "removing from repository... done.")))))

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
	       (eq type 'UPDATED) (eq type 'MODIFIED) (eq type 'MERGED)
	       (eq type 'CONFLICT) (eq type 'CVS-REMOVED)
	       (eq type 'REM-CONFLICT) (eq type 'MOVE-AWAY)
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
	(message "Regetting files from repository...")
	(if (cvs-execute-list files-to-update cvs-program
			      (if cvs-cvsroot
				  (list "-d" cvs-cvsroot "update")
				'("update")))
	    (error "CVS exited with non-zero exit status.")
	  (message "Regetting files from repository... done.")))))))

(defun cvs-mode-acknowledge ()
  "Remove all marked files from the buffer."
  (interactive)

  (mapcar (function (lambda (tin)
		      (tin-delete cvs-cookie-handle tin)))
	  (cvs-get-marked)))


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

(defun cvs-mode-previous-line (arg)
  "Go to the previous line.
If a prefix argument is given, move by that many lines."
  (interactive "p")
  (tin-goto-previous cvs-cookie-handle (point) arg))

(defun cvs-mode-next-line (arg)
  "Go to the next line.
If a prefix argument is given, move by that many lines."
  (interactive "p")
  (tin-goto-next cvs-cookie-handle (point) arg))

(defun cvs-add-file-update-buffer (tin)
  "Subfunction to cvs-mode-add. Internal use only.
Update the display. Return non-nil if `cvs add' should be called on this
file. Args: TIN.
Returns 'ADD or 'RESURRECT."
  (let ((fileinfo (tin-cookie cvs-cookie-handle tin)))
    (cond
     ((eq (cvs-fileinfo->type fileinfo) 'UNKNOWN)
      (cvs-set-fileinfo->type fileinfo 'ADDED)
      (tin-invalidate cvs-cookie-handle tin)
      'ADD)
     ((eq (cvs-fileinfo->type fileinfo) 'REMOVED)
      (cvs-set-fileinfo->type fileinfo 'UPDATED)
      (cvs-set-fileinfo->handled fileinfo t)
      (tin-invalidate cvs-cookie-handle tin)
      'RESURRECT))))

(defun cvs-add-sub (cvs-buf candidates)
  "Internal use only.
Args: CVS-BUF CANDIDATES.
CANDIDATES is a list of tins. Updates the CVS-BUF and returns a pair of lists.
The first list is unknown tins that shall be `cvs add -m msg'ed.
The second list is removed files that shall be `cvs add'ed (resurrected)."
  (let (add resurrect)
    (while candidates
      (let ((type (cvs-add-file-update-buffer (car candidates))))
	(cond ((eq type 'ADD)
	       (setq add (cons (car candidates) add)))
	      ((eq type 'RESURRECT)
	       (setq resurrect (cons (car candidates) resurrect)))))
      (setq candidates (cdr candidates)))
    (cons add resurrect)))

(defun cvs-mode-add ()
  "Add marked files to the cvs repository."
  (interactive)

  (let* ((buf (current-buffer))
	 (result (cvs-add-sub buf (cvs-get-marked)))
	 (added (car result))
	 (resurrect (cdr result))
	 (msg (if added (read-from-minibuffer "Enter description: "))))

    (if (or resurrect added)
	(cvs-use-temp-buffer))

    (cond (resurrect
	   (message "Resurrecting files from repository...")
	   (if (cvs-execute-list resurrect cvs-program
				 (if cvs-cvsroot
				     (list "-d" cvs-cvsroot "add")
				   '("add")))
	       (error "CVS exited with non-zero exit status.")
	     (message "Done."))))

    (cond (added
	   (message "Adding new files to repository...")
	   (if (cvs-execute-list added cvs-program
				 (if cvs-cvsroot
				     (list "-d" cvs-cvsroot "add" "-m" msg)
				   (list "add" "-m" msg)))
	       (error "CVS exited with non-zero exit status.")
	     (message "Done."))))))

(defun cvs-mode-ignore ()
  "Arrange so that CVS ignores the selected files.
This command ignores files that are not flagged as `Unknown'."
  (interactive)

  (mapcar (function (lambda (tin)
		      (cond
		       ((eq (cvs-fileinfo->type
			     (tin-cookie cvs-cookie-handle tin))
			    'UNKNOWN)
			(cvs-append-to-ignore
			 (tin-cookie cvs-cookie-handle tin))
			(tin-delete cvs-cookie-handle tin)))))
	  (cvs-get-marked)))

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

(defun cvs-mode-status ()
  "Show cvs status for all marked files."
  (interactive)

  (save-some-buffers)
  (let ((marked (cvs-get-marked)))
    (cvs-use-temp-buffer)
    (message "Running cvs status ...")
    (if (cvs-execute-list
	 marked cvs-program
	 (if cvs-cvsroot
	     (cons "-d" (cons cvs-cvsroot (cons "status" cvs-status-flags)))
	   (cons "status" cvs-status-flags)))
	(error "CVS exited with non-zero exit status.")
      (message "Running cvs status ... Done."))))

(defun cvs-mode-log ()
  "Display the cvs log of all selected files."
  (interactive)

  (let ((marked (cvs-get-marked)))
    (cvs-use-temp-buffer)
    (message "Running cvs log ...")
    (if (cvs-execute-list marked cvs-program
			  (if cvs-cvsroot
			      (cons "-d" (cons cvs-cvsroot
					       (cons "log" cvs-log-flags)))
			    (cons "log" cvs-log-flags)))
	(error "CVS exited with non-zero exit status.")
      (message "Running cvs log ... Done."))))

(defun cvs-byte-compile-files ()
  "Run byte-compile-file on all selected files that end in '.el'."
  (interactive)
  (let ((marked (cvs-get-marked)))
    (while marked
      (let ((filename (cvs-full-path (car marked))))
	(if (string-match "\\.el$" filename)
	    (byte-compile-file filename)))
      (setq marked (cdr marked)))))

(defun cvs-insert-full-path (tin)
  "Insert full path to the file described in TIN in the current buffer."
  (insert (format "%s\n" (cvs-full-path tin))))


(defun cvs-mode-add-change-log-entry-other-window (pos)
  "Add a ChangeLog entry in the ChangeLog of the current directory.
Args: POS."
  (interactive "d")
  (let* ((cvs-buf (current-buffer))
	 (odir default-directory))
    (setq default-directory
	  (file-name-as-directory
	   (cvs-fileinfo->dir
	    (tin-cookie
	     cvs-cookie-handle
	     (tin-locate cvs-cookie-handle pos)))))
    (if (not default-directory)		;In case there was no entries.
	(setq default-directory odir))
    (add-change-log-entry-other-window)
    (set-buffer cvs-buf)
    (setq default-directory odir)))


(defun print-cvs-tin (foo)
  "Debug utility."
  (let ((cookie (tin-cookie cvs-cookie-handle foo))
	(stream (get-buffer-create "debug")))
    (princ "==============\n" stream)
    (princ (cvs-fileinfo->file-name cookie) stream)
    (princ "\n" stream)
    (princ (cvs-fileinfo->dir cookie) stream)
    (princ "\n" stream)
    (princ (cvs-fileinfo->full-log cookie) stream)
    (princ "\n" stream)
    (princ (cvs-fileinfo->marked cookie) stream)
    (princ "\n" stream)))

(defun cvs-mode-emerge (pos)
  "Emerge appropriate revisions of the selected file.
Args: POS"
  (interactive "d")
  (let* ((cvs-buf (current-buffer))
	 (tin (tin-locate cvs-cookie-handle pos)))
    (if tin
	(let* ((fileinfo (tin-cookie cvs-cookie-handle tin))
	       (type (cvs-fileinfo->type fileinfo)))
	  (cond
	   ((eq type 'MODIFIED)
	    (require 'emerge)
	    (let ((tmp-file
		   (cvs-retrieve-revision-to-tmpfile fileinfo)))
	      (unwind-protect
		  (if (not (emerge-files
			    t
			    (cvs-fileinfo->full-path fileinfo)
			    tmp-file
			    (cvs-fileinfo->full-path fileinfo)))
		      (error "Emerge session failed"))
		(delete-file tmp-file))))

	   ((or (eq type 'MERGED)
		(eq type 'CONFLICT))
	    (require 'emerge)
	    (let ((tmp-file
		   (cvs-retrieve-revision-to-tmpfile
		    fileinfo))
		  (ancestor-file
		   (cvs-retrieve-revision-to-tmpfile
		    fileinfo
		    (cvs-fileinfo->base-revision fileinfo))))
	      (unwind-protect
		  (if (not (emerge-files-with-ancestor
			    t
			    (cvs-fileinfo->backup-file fileinfo)
			    tmp-file
			    ancestor-file
			    (cvs-fileinfo->full-path fileinfo)))
		      (error "Emerge session failed"))
		(delete-file tmp-file)
		(delete-file ancestor-file))))
	   (t
	    (error "Can only emerge \"Modified\", \"Merged\" or \"Conflict\"%s"
		   " files"))))
      (error "There is no file to emerge."))))

(defun cvs-retrieve-revision-to-tmpfile (fileinfo &optional revision)
  "Retrieve the latest revision of the file in FILEINFO to a temporary file.
If second optional argument REVISION is given, retrieve that revision instead."
  (let
      ((temp-name (make-temp-name
		   (concat (file-name-as-directory
			    (or (getenv "TMPDIR") "/tmp"))
			   "pcl-cvs." revision))))
    (cvs-kill-buffer-visiting temp-name)
    (if revision
	(message "Retrieving revision %s..." revision)
      (message "Retrieving latest revision..."))
    (let ((res (call-process cvs-shell nil nil nil "-c"
			     (concat cvs-program " update -p "
				     (if revision
					 (concat "-r " revision " ")
				       "")
				     (cvs-fileinfo->full-path fileinfo)
				     " > " temp-name))))
      (if (and res (not (and (integerp res) (zerop res))))
	  (error "Something went wrong: %s" res))

      (if revision
	  (message "Retrieving revision %s... Done." revision)
	(message "Retrieving latest revision... Done."))
      (find-file-noselect temp-name)
      temp-name)))

(defun cvs-fileinfo->backup-file (fileinfo)
  "Construct the file name of the backup file for FILEINFO."
  (if (cvs-fileinfo->base-revision fileinfo)
      (concat cvs-bakprefix (cvs-fileinfo->file-name fileinfo)
	      "." (cvs-fileinfo->base-revision fileinfo))))

(defun cvs-kill-buffer-visiting (filename)
  "If there is any buffer visiting FILENAME, kill it (without confirmation)."
  (let ((l (buffer-list)))
    (while l
      (if (string= (buffer-file-name (car l)) filename)
	  (kill-buffer (car l)))
      (setq l (cdr l)))))

(defun cvs-change-cvsroot (newroot)
  "Change the cvsroot."
  (interactive "DNew repository: ")
  (if (or (file-directory-p (expand-file-name "CVSROOT" newroot))
	  (y-or-n-p (concat "Warning: no CVSROOT found inside repository."
			    " Change cvs-cvsroot anyhow?")))
      (setq cvs-cvsroot newroot)))

(if (string-match "Lucid" emacs-version)
    (progn
      (autoload 'pcl-cvs-fontify "pcl-cvs-lucid")
      (add-hook 'cvs-mode-hook 'pcl-cvs-fontify)))


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
need to understand the change.  This is the behavior you get when this
variable is set to t.

On the other hand, you could argue that the CVS log entry for a change
should contain only the text for the changes which occurred in that
file, because the CVS log is per-file.  This is the behavior you get
when this variable is set to nil.")

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
If we are between subparagraphs, return the previous subparagraph."
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

(defun cvs-changelog-ours-p ()
  "See if ChangeLog entry at point is for the current user, today.
Return non-nil iff it is."
  ;; Code adapted from add-change-log-entry.
  (looking-at (concat (regexp-quote (substring (current-time-string)
					       0 10))
		      ".* "
		      (regexp-quote (substring (current-time-string) -4))
		      "[ \t]+"
		      (regexp-quote (if (boundp 'add-log-full-name)
                                        add-log-full-name
                                      user-full-name))
		      "  <"
                      (regexp-quote
                       (if (boundp 'add-log-mailing-address)
                           add-log-mailing-address
                         user-mail-address)))))

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
appropriate default log messages by looking at the ChangeLogs.  The
idea is to write your ChangeLog entries first, and then use this
command to commit your changes.

To select default log text, we:
- find the ChangeLogs for the files to be checked in,
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
