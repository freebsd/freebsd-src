	;; test float numbers and constants
	.text
        ;; Standard GAS syntax
start:  ldf     0e0, f0
        ldf     0e2.7, f0
        ldf     0e2.7e1, f0
        ldf     0e2.7e-1, f0
        ldf     0e-2.7e1, f0
        ldf     0e-2.7e-1, f0
        ldf     -0e1.0, f0

        ;; Standard TI syntax
        ldf     0, f0
        ldf     0.0, f0
        ldf     0.5, f0
        ldf     -0.5, f0
        ldf     2.7, f0
        ldf     2.7e-1, f0
        ldf     -2.7e1, f0
        ldf     -2.7e-1, f0

FLOAT:   .float   0f0, 0f1.0, 0f0.5, 0f-1.0, 0e-1.0e25, 3, 123, 0f3.141592654
SINGLE:  .single  0f0, 0f1.0, 0f0.5, 0f-1.0, 0e-1.0e25, 3, 123, 0f3.141592654
DOUBLE:  .double  0f0, 0f1.0, 0f0.5, 0f-1.0, 0e-1.0e25, 3, 123, 0f3.141592654
LDOUBLE: .ldouble 0f0, 0f1.0, 0f0.5, 0f-1.0, 0e-1.0e25, 3, 123, 0f3.141592654
IEEE:    .ieee    0f0, 0f1.0, 0f0.5, 0f-1,0, 0e-1.0e25, 3, 123, 0f3.141592654

      	.end
