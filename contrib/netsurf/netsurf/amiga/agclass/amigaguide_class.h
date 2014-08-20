/*
 *  AmigaGuide Class
 *
 */

#ifndef AMIGAGUIDE_CLASS_H
#define AMIGAGUIDE_CLASS_H

#include <exec/types.h>
#include <intuition/classes.h>

#include <classes/window.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/amigaguide.h>
#include <proto/utility.h>



// tag definitions
#define AMIGAGUIDE_Dummy        (TAG_USER+0x05000000)

#define AMIGAGUIDE_Name         (AMIGAGUIDE_Dummy + 1) // Name of the AmigaGuide database.
#define AMIGAGUIDE_Screen       (AMIGAGUIDE_Dummy + 2) // Pointer of the screen to open on.
#define AMIGAGUIDE_PubScreen    (AMIGAGUIDE_Dummy + 3) // Name of the public screen to open on.
#define AMIGAGUIDE_BaseName     (AMIGAGUIDE_Dummy + 4) // Basename of the application that opens the help file.
#define AMIGAGUIDE_ContextArray (AMIGAGUIDE_Dummy + 5) // Context node array (must be NULL-terminated).
#define AMIGAGUIDE_ContextID    (AMIGAGUIDE_Dummy + 6) // Index value of the node to display.
#define AMIGAGUIDE_Signal       (AMIGAGUIDE_Dummy + 7) // Signal mask to wait on

// method definition
#define AGM_Dummy   AMIGAGUIDE_Dummy + 100
#define AGM_OPEN    AGM_Dummy + 1
#define AGM_CLOSE   AGM_Dummy + 2
#define AGM_PROCESS AGM_Dummy + 3

// function prototypes
Class *initAGClass(void);
BOOL   freeAGClass(Class *);

#endif   // AMIGAGUIDE_CLASS_H

