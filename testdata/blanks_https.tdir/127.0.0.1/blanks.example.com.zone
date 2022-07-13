; Test if the zone parser accepts various blank lines
@		IN	SOA	ns1.example.com dnsmaster.example.com. (
				1	; Serial
				      7200	; Refresh 2 hours
				      3600	; Retry   1 hour
				   2419200	; expire - 4 weeks
				      3600	; Minimum 1 hour     
)
		7200	IN	NS      ns1
ns1			IN	A	192.0.2.1
			IN	AAAA	2001:dbb::1
; completely blank line

; line with one space
 
; line with one tab
	
; line with spaces followed by comment
  ; test comment
; line with tabs followed by comment
		; test comment
; Final line with spaces, tabs and comment
 	 	; test comment
