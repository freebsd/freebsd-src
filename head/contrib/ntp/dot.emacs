;; This is how Dave Mills likes to see the code formatted.

(defconst ntp-c-style
  '((c-basic-offset . 8)
    (c-offsets-alist . ((arglist-intro	      . +)
			(case-label	      . *)
			(statement-case-intro . *)
			(statement-cont	      . *)
			(substatement-open    . 0))))
  "Dave L. Mills; programming style for use with ntp")

(defun ntp-c-mode-common-hook ()
  ;; add ntp c style
  (c-add-style "ntp" ntp-c-style nil))

(add-hook 'c-mode-common-hook 'ntp-c-mode-common-hook)

;; 1997112600
