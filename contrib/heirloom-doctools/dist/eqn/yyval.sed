#
# Sccsid @(#)yyval.sed	1.2 (gritter) 10/2/07
#
# bison has a yacc-compatible yyval, but it is a local variable inside
# yyparse(). Making the variable global is necessary to make bc work
# with a bison-generated parser.
1,2 {
	/Bison/ {
	:look
		/second part of user declarations/ {
			i\
			YYSTYPE yyval;
		:repl
			s/^[ 	]*YYSTYPE[ 	]*yyval;//
			n
			t
			b repl
		}
		n
		b look
	}
}
