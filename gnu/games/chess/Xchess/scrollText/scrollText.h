/*
 * Scrollable Text Window Header File
 *
 * David Harrison
 * University of California,  Berkeley
 * 1986
 *
 * This file contains definitions for a scrollable text window
 * with scroll bar support.
 */

int TxtGrab();
   /* Take hold of a previously created window */

#define TXT_NO_COLOR	-1

int TxtAddFont();
   /* Loads a new font for use later */
int TxtWinP();
   /* Returns non-zero value if the window is text window */
int TxtClear();
   /* Clears text window and resets text buffer */

int TxtWriteStr();
   /* Writes a string to window with immediate update */
int TxtJamStr();
   /* Write a string without causing update to screen */

int TxtRepaint();
   /* Repaints entire scrollable text window */
int TxtFilter();
   /* Handles events related to text window */
