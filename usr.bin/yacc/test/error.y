%%
S: error
%%
main(){printf("yyparse() = %d\n",yyparse());}
yylex(){return-1;}
yyerror(s)char*s;{printf("%s\n",s);}
