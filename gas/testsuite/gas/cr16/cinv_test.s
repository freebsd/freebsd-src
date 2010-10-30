        .text
        .global main
main:
	##############################
	# cin [i/i,u/d/d,u/d,i/d,i,u]
	##############################
	cinv   [i]
	cinv   [i,u]
	cinv   [d]
	cinv   [d,u]
	cinv   [d,i]
	cinv   [d,i,u]
