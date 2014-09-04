/*
 *  shatest.c
 *
 *  Copyright (C) 1998, 2009
 *  Paul E. Jones <paulej@packetizer.com>
 *  All Rights Reserved
 *
 *****************************************************************************
 *  $Id: shatest.c 12 2009-06-22 19:34:25Z paulej $
 *****************************************************************************
 *
 *  Description:
 *      This file will exercise the SHA1 class and perform the three
 *      tests documented in FIPS PUB 180-1.
 *
 *  Portability Issues:
 *      None.
 *
 */

#include <stdio.h>
#include <string.h>
#include "sha1.h"
#include "minunit.h"
#include "catcierge_test_helpers.h"

/*
 *  Define patterns for testing
 */
#define TESTA   "abc"
#define TESTB_1 "abcdbcdecdefdefgefghfghighij"
#define TESTB_2 "hijkijkljklmklmnlmnomnopnopq"
#define TESTB   TESTB_1 TESTB_2
#define TESTC   "a"

static char *run_tests()
{
    SHA1Context sha;
    int i;

    /*
     *  Perform test A
     */
    catcierge_test_STATUS("\nTest A: 'abc'");

    SHA1Reset(&sha);
    SHA1Input(&sha, (const unsigned char *) TESTA, strlen(TESTA));

    if (!SHA1Result(&sha))
    {
        return "Could not compute message digest";
    }
    else
    {
        catcierge_test_STATUS_ex("\t");
        for(i = 0; i < 5 ; i++)
        {
            catcierge_test_STATUS_ex("%X ", sha.Message_Digest[i]);
        }
        catcierge_test_STATUS("");
        catcierge_test_STATUS("Should match:");
        catcierge_test_STATUS("\tA9993E36 4706816A BA3E2571 7850C26C 9CD0D89D");
        mu_assert("Unexpected digest",
            sha.Message_Digest[0] == 0xA9993E36 &&
            sha.Message_Digest[1] == 0x4706816A &&
            sha.Message_Digest[2] == 0xBA3E2571 &&
            sha.Message_Digest[3] == 0x7850C26C &&
            sha.Message_Digest[4] == 0x9CD0D89D);
    }

    /*
     *  Perform test B
     */
    catcierge_test_STATUS("\nTest B:");

    SHA1Reset(&sha);
    SHA1Input(&sha, (const unsigned char *) TESTB, strlen(TESTB));

    if (!SHA1Result(&sha))
    {
        return "Could not compute message digest";
    }
    else
    {
        catcierge_test_STATUS_ex("\t");
        for(i = 0; i < 5 ; i++)
        {
            catcierge_test_STATUS_ex("%X ", sha.Message_Digest[i]);
        }
        catcierge_test_STATUS("");
        catcierge_test_STATUS("Should match:");
        catcierge_test_STATUS("\t84983E44 1C3BD26E BAAE4AA1 F95129E5 E54670F1");
        mu_assert("Unexpected digest",
            sha.Message_Digest[0] == 0x84983E44 &&
            sha.Message_Digest[1] == 0x1C3BD26E &&
            sha.Message_Digest[2] == 0xBAAE4AA1 &&
            sha.Message_Digest[3] == 0xF95129E5 &&
            sha.Message_Digest[4] == 0xE54670F1);
    }

    /*
     *  Perform test C
     */
    catcierge_test_STATUS("\nTest C: One million 'a' characters");

    SHA1Reset(&sha);
    for(i = 1; i <= 1000000; i++) 
    {
        SHA1Input(&sha, (const unsigned char *) TESTC, 1);
    }

    if (!SHA1Result(&sha))
    {
        return "Could not compute message digest";
    }
    else
    {
        catcierge_test_STATUS_ex("\t");
        for(i = 0; i < 5 ; i++)
        {
            catcierge_test_STATUS_ex("%X ", sha.Message_Digest[i]);
        }
        catcierge_test_STATUS("");
        catcierge_test_STATUS("Should match:");
        catcierge_test_STATUS("\t34AA973C D4C4DAA4 F61EEB2B DBAD2731 6534016F\n");
        mu_assert("Unexpected digest",
            sha.Message_Digest[0] == 0x34AA973C &&
            sha.Message_Digest[1] == 0xD4C4DAA4 &&
            sha.Message_Digest[2] == 0xF61EEB2B &&
            sha.Message_Digest[3] == 0xDBAD2731 &&
            sha.Message_Digest[4] == 0x6534016F);
    }

    return 0;
}

int TEST_sha1(int argc, char **argv)
{
    int ret = 0;
    char *e = NULL;

    CATCIERGE_RUN_TEST((e = run_tests()),
        "SHA1 tests",
        "SHA1 tests", &ret);    

    return ret;
}
