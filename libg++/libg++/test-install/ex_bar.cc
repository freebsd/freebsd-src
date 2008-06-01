// bar.cc - test the runtime support for GNU C++ exception handling.
//          inspired by a PROLOG toplevel loop by Heinz Seidl (hgs@cygnus.com)

#define S(s) SS(s)
#define SS(s) # s

extern "C" {
    int printf (const char*, ...);
    void abort ();
    int exit (int);
};

enum TERM { ATOM, INTEGER, DOUBLE, STRUCTURE, LIST, REF, EOF };

exception {
    int number;
    char * message;
} EX_IMPL;

exception {
    TERM Term;
    char * message;
} EX_CALL;

exception {} EX_EOF;


void pread (TERM & Term)
{
    // simulate an input buffer
    //
    static int p = 0;
    TERM buffer[] = 
      { REF,
	LIST,
	STRUCTURE,
	DOUBLE,
	INTEGER,
	ATOM
      };
    const int blen = sizeof (buffer) / sizeof (TERM);
    
    if ( p < blen)
      Term = buffer[p++];
    else
      Term = EOF;
}

void call (const TERM & Term) raises EX_CALL, EX_EOF, EX_IMPL
{
    switch (Term) 
      {
	case REF:
	  raise EX_CALL (REF,
			 "Sorry - dereferencing not implemented (REF).");
	  break;
	case LIST:
	  raise EX_CALL (LIST,   
			 "LISTs are not callable (LIST).");
	  break;
	case STRUCTURE:
	  raise EX_CALL (STRUCTURE,
			 "Undefined predicate (STRUCTURE).");
	  break;
	case DOUBLE:
	  raise EX_CALL (DOUBLE,
			 "DOUBLEs are not callable (DOUBLE).");
	  break;
	case INTEGER:
	  raise EX_CALL (INTEGER,
			 "INTEGERs are not callable (INTEGER).");
	case ATOM:
	   raise EX_CALL (ATOM,
			  "Undefined predicate (ATOM).");
	  break;
	case EOF:
	  raise EX_EOF ();
	  break;
	default:
	  raise EX_IMPL ( Term,
			 "Implementation error in file " __FILE__ 
			 " at line "  S(__LINE__) " .");
      }
}

void main()
{
    try {
	while (1) {
	    try {
		while (1) 
		  {
		      TERM Term;
		      pread (Term);
		      call (Term);
		  }
	    } except ep {
		EX_CALL {
		    printf ("EXCEPTION(%d) : %s\n", ep.Term, ep.message);
		}
		EX_EOF {
		    printf ("EOF encountered.\n");
		    raise ep;
		}
		default {
		    raise ep;
		}
	    }
	}
    } except ep {
	EX_IMPL {
	    printf ("FATAL(%d): %s\n", ep.number, ep.message);
	    abort();
	}
	EX_EOF {
	    printf ("Good bye.\n");
	}
	default {
	    raise ep;
	}
    }
    return 0;
}
