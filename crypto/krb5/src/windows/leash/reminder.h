#ifndef __REMINDER_H__
#define __REMINDER_H__

#define Stringize( L )        #L
#define MakeString( M, L )    M(L)
#define $LINE                 MakeString( Stringize, __LINE__ )
#define Reminder              __FILE__ "(" $LINE ") : Reminder: "

#endif

//Put this in your .cpp file where ever you need it (NOTE: Don't end this statement with a ';' char)
//i.e. -->> #pragma message(Reminder "Your message reminder here!!!")
