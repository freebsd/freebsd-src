/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/****************************************************************************
 *                                                                           *
 *                               SHS Test Code                               *
 *                                                                           *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "shs.h"

/* Test the SHS implementation */

#ifdef NEW_SHS

static SHS_LONG shsTestResults[][ 5 ] = {
    { 0xA9993E36L, 0x4706816AL, 0xBA3E2571L, 0x7850C26CL, 0x9CD0D89DL, },
    { 0x84983E44L, 0x1C3BD26EL, 0xBAAE4AA1L, 0xF95129E5L, 0xE54670F1L, },
    { 0x34AA973CL, 0xD4C4DAA4L, 0xF61EEB2BL, 0xDBAD2731L, 0x6534016FL, }
};

#else

static SHS_LONG shsTestResults[][ 5 ] = {
    { 0x0164B8A9L, 0x14CD2A5EL, 0x74C4F7FFL, 0x082C4D97L, 0xF1EDF880L },
    { 0xD2516EE1L, 0xACFA5BAFL, 0x33DFC1C4L, 0x71E43844L, 0x9EF134C8L },
    { 0x3232AFFAL, 0x48628A26L, 0x653B5AAAL, 0x44541FD9L, 0x0D690603L }
};
#endif /* NEW_SHS */

static int compareSHSresults(shsInfo, shsTestLevel)
    SHS_INFO *shsInfo;
    int shsTestLevel;
{
    int i, fail = 0;

    /* Compare the returned digest and required values */
    for( i = 0; i < 5; i++ )
        if( shsInfo->digest[ i ] != shsTestResults[ shsTestLevel ][ i ] )
            fail = 1;
    if (fail) {
        printf("\nExpected: ");
        for (i = 0; i < 5; i++) {
            printf("%8.8lx ", (unsigned long) shsTestResults[shsTestLevel][i]);
        }
        printf("\nGot:      ");
        for (i = 0; i < 5; i++) {
            printf("%8.8lx ", (unsigned long) shsInfo->digest[i]);
        }
        printf("\n");
        return( -1 );
    }
    return( 0 );
}

int
main()
{
    SHS_INFO shsInfo;
    unsigned int i;

    /* Make sure we've got the endianness set right.  If the machine is
       big-endian (up to 64 bits) the following value will be signed,
       otherwise it will be unsigned.  Unfortunately we can't test for odd
       things like middle-endianness without knowing the size of the data
       types */

    /* Test SHS against values given in SHS standards document */
    printf( "Running SHS test 1 ... " );
    shsInit( &shsInfo );
    shsUpdate( &shsInfo, ( SHS_BYTE * ) "abc", 3 );
    shsFinal( &shsInfo );
    if( compareSHSresults( &shsInfo, 0 ) == -1 )
    {
        putchar( '\n' );
        puts( "SHS test 1 failed" );
        exit( -1 );
    }
#ifdef NEW_SHS
    puts( "passed, result= A9993E364706816ABA3E25717850C26C9CD0D89D" );
#else
    puts( "passed, result= 0164B8A914CD2A5E74C4F7FF082C4D97F1EDF880" );
#endif /* NEW_SHS */

    printf( "Running SHS test 2 ... " );
    shsInit( &shsInfo );
    shsUpdate( &shsInfo, ( SHS_BYTE * ) "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56 );
    shsFinal( &shsInfo );
    if( compareSHSresults( &shsInfo, 1 ) == -1 )
    {
        putchar( '\n' );
        puts( "SHS test 2 failed" );
        exit( -1 );
    }
#ifdef NEW_SHS
    puts( "passed, result= 84983E441C3BD26EBAAE4AA1F95129E5E54670F1" );
#else
    puts( "passed, result= D2516EE1ACFA5BAF33DFC1C471E438449EF134C8" );
#endif /* NEW_SHS */

    printf( "Running SHS test 3 ... " );
    shsInit( &shsInfo );
    for( i = 0; i < 15625; i++ )
        shsUpdate( &shsInfo, ( SHS_BYTE * ) "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 64 );
    shsFinal( &shsInfo );
    if( compareSHSresults( &shsInfo, 2 ) == -1 )
    {
        putchar( '\n' );
        puts( "SHS test 3 failed" );
        exit( -1 );
    }
#ifdef NEW_SHS
    puts( "passed, result= 34AA973CD4C4DAA4F61EEB2BDBAD27316534016F" );
#else
    puts( "passed, result= 3232AFFA48628A26653B5AAA44541FD90D690603" );
#endif /* NEW_SHS */

    puts( "\nAll SHS tests passed" );
    exit( 0 );
}
