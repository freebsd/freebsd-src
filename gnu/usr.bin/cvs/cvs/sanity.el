;;;; -*- lisp-interaction -*-
;;;; Time-stamp: <29 Nov 93 14:25:28, by rich@sendai.cygnus.com>

(defun reset-fail-counter (arg)
  (interactive "p")
  (setq fail-counter arg)
  (message (concat "fail-counter = " (int-to-string arg))))


(defun inc-next-fail-counter nil
  (interactive)
  (search-forward "failed test ")
  (kill-word 1)
  (insert-string fail-counter)
  (setq fail-counter (+ 1 fail-counter)))

(global-set-key [f15] 'reset-fail-counter)
(global-set-key [f16] 'inc-next-fail-counter)
