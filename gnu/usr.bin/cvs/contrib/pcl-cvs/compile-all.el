;;;; @(#) Id: compile-all.el,v 1.11 1993/05/31 18:40:25 ceder Exp 
;;;; This file byte-compiles all .el files in pcl-cvs release 1.05.
;;;;
;;;; Copyright (C) 1991 Inge Wallin
;;;;
;;;; This file was once upon a time part of Elib, but have since been
;;;; modified by Per Cederqvist.
;;;;
;;;; GNU Elib is free software; you can redistribute it and/or modify
;;;; it under the terms of the GNU General Public License as published by
;;;; the Free Software Foundation; either version 1, or (at your option)
;;;; any later version.
;;;;
;;;; GNU Elib is distributed in the hope that it will be useful,
;;;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;;; GNU General Public License for more details.
;;;;
;;;; You should have received a copy of the GNU General Public License
;;;; along with GNU Emacs; see the file COPYING.  If not, write to
;;;; the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
;;;;


(setq files-to-compile '("pcl-cvs" "pcl-cvs-lucid"))


(defun compile-file-if-necessary (file)
  "Compile FILE if necessary.

This is done if FILE.el is newer than FILE.elc or if FILE.elc doesn't exist."
  (let ((el-name (concat file ".el"))
	(elc-name (concat file ".elc")))
    (if (or (not (file-exists-p elc-name))
	    (file-newer-than-file-p el-name elc-name))
	(progn
	  (message (format "Byte-compiling %s..." el-name))
	  (byte-compile-file el-name)))))


(defun compile-pcl-cvs ()
  "Byte-compile all uncompiled files of pcl-cvs."

  (interactive)

  ;; Be sure to have . in load-path since a number of files
  ;; depend on other files and we always want the newer one even if
  ;; a previous version of pcl-cvs exists.
  (let ((load-path (append '(".") load-path)))

    (mapcar (function compile-file-if-necessary)
	    files-to-compile)))
