# err.s
# some data pointer error conditions

#NOT YET INCLUDED 



	MOVE @++DP[0], @DP[0]++
	MOVE @++DP[1], @DP[1]++
	MOVE @BP[++Offs], @BP[Offs++]
	MOVE @--DP[0], @DP[0]--
	MOVE @--DP[1], @DP[1]--
	MOVE @BP[--Offs], @BP[Offs--]	
	MOVE @++DP[0], @DP[0]--
	MOVE @++DP[1], @DP[1]--	
	MOVE @BP[++Offs], @BP[Offs--]		
	MOVE @--DP[0], @DP[0]++
	MOVE @--DP[1], @DP[1]++
	MOVE @BP[--Offs], @BP[Offs++]
	MOVE @DP[0], @DP[0]++
	MOVE @DP[1], @DP[1]++
	MOVE @BP[Offs], @BP[Offs++]
	MOVE @DP[0], @DP[0]--
	MOVE @DP[1], @DP[1]--
	MOVE @BP[Offs], @BP[Offs--]
	MOVE DP[0], @DP[0]++
	MOVE DP[0], @DP[0]--
	MOVE DP[1], @DP[1]++
	MOVE DP[1], @DP[1]--
	MOVE Offs, @BP[Offs--]	
	MOVE Offs, @BP[Offs++]
