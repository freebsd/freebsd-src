define(ack, `ifelse($1,0,incr($2),$2,0,`ack(DECR($1),1)',
`ack(DECR($1), ack($1,DECR($2)))')')
