*
* Constants initialization
* See also long.s, space.s, field.s
*
	.global binary, octal, hex, field
binary:	.word	11b, 0100B
octal:	.word	011q, 12q, 013Q
hex:	.word	0Fh, 10H	
field:	.field	3, 3
	.field	8, 6
	.field	16, 5
	.field	01234h,20
	.field	01234h,32
	.global byte, word, xlong, long, int
byte:	.byte	0AAh, 0BBh
word:	.word	0CCCh
xlong:	.xlong	0EEEEFFFh	
long:	.long	0EEEEFFFFh
int:	.int	0DDDDh		
	.global xfloat, float
xfloat:	.xfloat	1.99999		
float:	.float	1.99999
	.global string, pstring
string	.string "abcd"
	.string	"abc","defg"
	.string	36 + 12
pstring	.pstring "abcd"
	.pstring "abc","defg"
	
	.global DAT1, DAT2, DAT3, DAT4
DAT1:	.long 0ABCDh, 'A' + 100h, 'g', 'o'		
xlong?:	.xlong DAT1, 0AABBCCDDh				
DAT2:	.word 0						
DAT3:	.long 012345678h				
	.word 0						
	.xlong 0AABBCCDDh				
DAT4:							
	.end
