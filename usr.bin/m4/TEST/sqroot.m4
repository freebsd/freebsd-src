define(square_root, 
	`ifelse(eval($1<0),1,negative-square-root,
			     `square_root_aux($1, 1, eval(($1+1)/2))')')
define(square_root_aux,
	`ifelse($3, $2, $3,
		$3, eval($1/$2), $3,
		`square_root_aux($1, $3, eval(($3+($1/$3))/2))')')
