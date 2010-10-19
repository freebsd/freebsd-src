#source: sort_n_a-a.s
#source: sort_n_a-b.s
#ld: -T sort_b_n_a.t --sort-section name
#name: SORT_BY_NAME(SORT_BY_ALIGNMENT()) --sort-section alignment
#nm: -n

0[0-9a-f]* t texta
0[0-9a-f]* t textb
0[0-9a-f]* t text1a
0[0-9a-f]* t text1b
0[0-9a-f]* t text2a
0[0-9a-f]* t text2b
0[0-9a-f]* t text3b
0[0-9a-f]* t text3a
