define(hanoi, `trans(A, B, C, $1)')

define(moved,`move disk from $1 to $2
')

define(trans, `ifelse($4,1,`moved($1,$2)',
	`trans($1,$3,$2,DECR($4))moved($1,$2)trans($3,$2,$1,DECR($4))')')
