	.text
	prefetch [%g1],0
	prefetch [%g1],31
	prefetch [%g1],#n_reads
	prefetch [%g1],#one_read
	prefetch [%g1],#n_writes
	prefetch [%g1],#one_write
	prefetcha [%g1]#ASI_AIUP,0
	prefetcha [%g1]%asi,31
	prefetcha [%g1]#ASI_AIUS,#n_reads
	prefetcha [%g1]%asi,#one_read
