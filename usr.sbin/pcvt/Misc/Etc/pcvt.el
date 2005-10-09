;; pcvt25.el, by J"org Wunsch <joerg_wunsch@uriah.sax.de>
;;
;; pcvt is a good VT emulator

(load "term/vt220")

;; ...but i don't like `find' and `select' on `home' and `end' keys
(global-set-key [find] 'beginning-of-line)
(global-set-key [select] 'end-of-line)

;; and the f1 thru f8 keys are designated as f6 thru f13
(define-key function-key-map "\e[17~" [f1])
(define-key function-key-map "\e[18~" [f2])
(define-key function-key-map "\e[19~" [f3])
(define-key function-key-map "\e[20~" [f4])
(define-key function-key-map "\e[21~" [f5])
(define-key function-key-map "\e[23~" [f6])
(define-key function-key-map "\e[24~" [f7])
(define-key function-key-map "\e[25~" [f8])
