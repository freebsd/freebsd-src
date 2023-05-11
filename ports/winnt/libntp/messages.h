 /*
 Boilerplate Microsoft copyright statement previously here removed because
 the code does not posess "at least some minimal degree of creativity".
 
 https://www.copyright.gov/comp3/chap300/ch300-copyrightable-authorship.pdf
 Section 308 The Originality Requirement
 */
//
//  Values are 32 bit values laid out as follows:
//
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  +---+-+-+-----------------------+-------------------------------+
//  |Sev|C|R|     Facility          |               Code            |
//  +---+-+-+-----------------------+-------------------------------+
//
//  where
//
//      Sev - is the severity code
//
//          00 - Success
//          01 - Informational
//          10 - Warning
//          11 - Error
//
//      C - is the Customer code flag
//
//      R - is a reserved bit
//
//      Facility - is the facility code
//
//      Code - is the facility's status code
//
//
// Define the facility codes
//


//
// Define the severity codes
//


//
// MessageId: NTP_ERROR
//
// MessageText:
//
// %1
//
#define NTP_ERROR                        0xC0000001L

//
// MessageId: NTP_WARNING
//
// MessageText:
//
// %1
// 
//
#define NTP_WARNING                      0x80000002L

//
// MessageId: NTP_INFO
//
// MessageText:
//
// %1
// 
// 
// 
//
#define NTP_INFO                         0x40000003L

