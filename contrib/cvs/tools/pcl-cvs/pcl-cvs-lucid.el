;;; Mouse and font support for PCL-CVS 1.3 running in Lucid GNU Emacs
;; @(#) Id: pcl-cvs-lucid.el,v 1.2 1993/05/31 19:37:34 ceder Exp 
;; Copyright (C) 1992-1993 Free Software Foundation, Inc.

;; This file is part of GNU Emacs.

;; GNU Emacs is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.

;; GNU Emacs is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with GNU Emacs; see the file COPYING.  If not, write to
;; the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.


;; This simply adds a menu of the common CVS commands to the menubar and to
;; the right mouse button.  Clicking right moves point, and then pops up a
;; menu from which commands can be executed.
;; 
;; This could stand to be a lot more clever: for example, the "Commit Changes"
;; command should only be active on files for which there is something to
;; commit.  Also, some indication of which files the command applies to
;; (especially in the presence of multiple marked files) would be nice.
;;
;; Middle-click runs find-file.


;(require 'pcl-cvs)
(load "pcl-cvs.el")

(defvar cvs-menu
  '("CVS"
    ["Find File"			cvs-mode-find-file		t]
    ["Find File Other Window"		cvs-mode-find-file-other-window	t]
    ["Interactively Merge (emerge)"	cvs-mode-emerge			t]
    ["Diff against Repository"		cvs-mode-diff-cvs		t]
    ["Diff against Backup Version"	cvs-mode-diff-backup		t]
    "----"
    ["Commit Changes to Repository"	cvs-mode-commit			t]
    ["Revert File from Repository"	cvs-mode-undo-local-changes	t]
    ["Add File to Repository"		cvs-mode-add			t]
    ["Remove File from Repository"	cvs-mode-remove-file		t]
    ["Ignore File"			cvs-mode-ignore			t]
    ["Hide File"			cvs-mode-acknowledge		t]
    ["Hide Handled Files"		cvs-mode-remove-handled		t]
    "----"
    ["Add ChangeLog Entry"	cvs-mode-add-change-log-entry-other-window t]
    ["Show CVS Log"			cvs-mode-log			t]
    ["Show CVS Status"			cvs-mode-status			t]
    "----"
    ["Mark File"			cvs-mode-mark			t]
    ["Unmark File"			cvs-mode-unmark			t]
    ["Mark All Files"			cvs-mode-mark-all-files		t]
    ["Unmark All Files"			cvs-mode-unmark-all-files	t]
    "----"
    ["Quit"				bury-buffer			t]
    ))

(defun cvs-menu (e)
  (interactive "e")
  (mouse-set-point e)
  (beginning-of-line)
  (or (looking-at "^[* ] ") (error "No CVS file line here"))
  (popup-menu cvs-menu))

(defun cvs-mouse-find-file (e)
  (interactive "e")
  (mouse-set-point e)
  (beginning-of-line)
  (or (looking-at "^[* ] ") (error "No CVS file line here"))
  (cvs-mode-find-file (point)))

(define-key cvs-mode-map 'button3 'cvs-menu)
(define-key cvs-mode-map 'button2 'cvs-mouse-find-file)

(make-face 'cvs-header-face)
(make-face 'cvs-filename-face)
(make-face 'cvs-status-face)

(or (face-differs-from-default-p 'cvs-header-face)
    (copy-face 'italic 'cvs-header-face))

(or (face-differs-from-default-p 'cvs-filename-face)
    (copy-face 'bold 'cvs-filename-face))

(or (face-differs-from-default-p 'cvs-status-face)
    (copy-face 'bold-italic 'cvs-status-face))


(defun pcl-mode-motion-highlight-line (event)
  (if (save-excursion
	(let* ((window (event-window event))
	       (buffer (and window (window-buffer window)))
	       (point (and buffer (event-point event))))
	  (and point
	       (progn
		 (set-buffer buffer)
		 (goto-char point)
		 (beginning-of-line)
		 (looking-at "^[* ] ")))))
      (mode-motion-highlight-line event)))

(defconst pcl-cvs-font-lock-keywords
  '(("^In directory \\(.+\\)$" 1 cvs-header-face)
    ("^[* ] \\w+ +\\(ci\\)" 1 cvs-status-face)
    ("^[* ] \\(Conflict\\|Merged\\)" 1 cvs-status-face)
    ("^[* ] \\w+ +\\(ci +\\)?\\(.+\\)$" 2 cvs-filename-face)
    )
  "Patterns to highlight in the *cvs* buffer.")

(defun pcl-cvs-fontify ()
  ;;
  ;; set up line highlighting
  (require 'mode-motion)
  (setq mode-motion-hook 'pcl-mode-motion-highlight-line)
  ;;
  ;; set up menubar
  (if (and current-menubar (not (assoc "CVS" current-menubar)))
      (progn
	(set-buffer-menubar (copy-sequence current-menubar))
	(add-menu nil "CVS" (cdr cvs-menu))))
  ;;
  ;; fontify mousable lines
  (set (make-local-variable 'font-lock-keywords) pcl-cvs-font-lock-keywords)
  (font-lock-mode 1)
  )

(add-hook 'cvs-mode-hook 'pcl-cvs-fontify)
