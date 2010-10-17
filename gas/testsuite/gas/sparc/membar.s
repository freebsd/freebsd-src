# Test membar args
	.text
	membar 0
	membar 127
	membar #Sync|#MemIssue|#Lookaside|#StoreStore|#LoadStore|#StoreLoad|#LoadLoad
	membar #Sync
	membar #MemIssue
	membar #Lookaside
	membar #StoreStore
	membar #LoadStore
	membar #StoreLoad
	membar #LoadLoad
