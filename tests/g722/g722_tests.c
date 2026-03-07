/*
 * SpanDSP - a series of DSP components for telephony
 *
 * g722_tests.c - Test G.722 encode and decode (ITU-T compliance only).
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*!
 * \file g722_tests.c
 * \brief G.722 tests – ITU-T compliance tests only.
 *
 * This module implements the tests defined in the G.722 specification,
 * using the test data files supplied with the specification.
 *
 * To perform the tests you need to obtain the test data files from the
 * specification. These are copyright material, and so cannot be distributed
 * with this test software.
 *
 * The files, containing test vectors, which are supplied with the G.722
 * specification, should be copied to itutests/g722. The ITU tests can then
 * be run by executing this program without any parameters.
 *
 * \note This file has been adapted for use as a library on microcontrollers.
 *       File I/O functions are replaced by macros defined in config.h.
 *       The function itu_compliance_tests() returns 0 on success or a
 *       negative error code on failure.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <g722.h>

#include "g722_tests.h"

/* ------------------------------------------------------------------------- */
/* I/O macros – to be provided by the user in config.h                       */
/* ------------------------------------------------------------------------- */
#ifndef TEST_OPEN
#define TEST_OPEN(filename, mode)   fopen(filename, mode)
#endif

#ifndef TEST_CLOSE
#define TEST_CLOSE(file)             fclose(file)
#endif

#ifndef TEST_GETS
#define TEST_GETS(buf, size, file)   fgets(buf, size, file)
#endif

#ifndef TEST_PRINTF
#define TEST_PRINTF                  printf
#endif

/* The original code used exit() on fatal errors.  In a library we return
   a negative error code instead.  The macro is kept for clarity, but it
   is never used in this version. */
#ifndef TEST_EXIT
#define TEST_EXIT(code)              return (code)
#endif

/* ------------------------------------------------------------------------- */
/* Test data directory – may be overridden in config.h                       */
/* ------------------------------------------------------------------------- */
#ifndef TESTDATA_DIR
#define TESTDATA_DIR        "../test-data/itu/g722/g722-ascii/"
#endif

#define MAX_TEST_VECTOR_LEN 16500

/*We don't have much RAM on MCU so we must use aliasing...*/
static union {
    /*! Input data buffer (16-bit PCM samples) */
    int16_t data[MAX_TEST_VECTOR_LEN];

    /*! Reference output data (upper band) */
    uint16_t ref_upper[MAX_TEST_VECTOR_LEN];
} itu;
/*! Reference output data (lower band) */
static uint16_t itu_ref[MAX_TEST_VECTOR_LEN];
/*! Compressed G.722 data */
static uint8_t compressed[MAX_TEST_VECTOR_LEN];
/*! Decompressed PCM data */
static int16_t decompressed[2 * MAX_TEST_VECTOR_LEN];

/*!
 * List of encode test file pairs (input -> reference output).
 * Each pair consists of:
 *   - input file  (PCM, 16 bits/sample, 16 kHz, in hex format)
 *   - reference encoded file (G.722 compressed, in hex format)
 */
static const char *encode_test_files[] =
{
    TESTDATA_DIR "T1C1.XMT",
    TESTDATA_DIR "T2R1.COD",
    TESTDATA_DIR "T1C2.XMT",
    TESTDATA_DIR "T2R2.COD",
    NULL
};

/*!
 * List of decode test file groups.
 * For each group (5 files) we have:
 *   - compressed input file
 *   - lower band reference output for mode 1 (48 kbit/s)
 *   - lower band reference output for mode 2 (56 kbit/s)
 *   - lower band reference output for mode 3 (64 kbit/s)
 *   - upper band reference output (all modes use the same upper band)
 */
static const char *decode_test_files[] =
{
    TESTDATA_DIR "T2R1.COD",
    TESTDATA_DIR "T3L1.RC1",
    TESTDATA_DIR "T3L1.RC2",
    TESTDATA_DIR "T3L1.RC3",
    TESTDATA_DIR "T3H1.RC0",

    TESTDATA_DIR "T2R2.COD",
    TESTDATA_DIR "T3L2.RC1",
    TESTDATA_DIR "T3L2.RC2",
    TESTDATA_DIR "T3L2.RC3",
    TESTDATA_DIR "T3H2.RC0",

    TESTDATA_DIR "T1D3.COD",
    TESTDATA_DIR "T3L3.RC1",
    TESTDATA_DIR "T3L3.RC2",
    TESTDATA_DIR "T3L3.RC3",
    TESTDATA_DIR "T3H3.RC0",

    NULL
};

/*!
 * Convert a 4‑character hex string to an integer.
 * \param s Pointer to a string containing exactly 4 hex digits.
 * \return The integer value, or -1 on error.
 */
static int hex_get(char *s)
{
    int i;
    int value;
    int x;

    for (value = i = 0;  i < 4;  i++)
    {
        x = *s++ - 0x30;
        if (x > 9)
            x -= 0x07;
        if (x > 15)
            x -= 0x20;
        if (x < 0  ||  x > 15)
            return -1;
        value <<= 4;
        value |= x;
    }
    return value;
}

/*!
 * Read one vector (a line of hex numbers) from a test vector file.
 * \param file Open file handle.
 * \param vec  Array to store the decoded numbers.
 * \return The number of values read, or 0 at end of file.
 */
static int get_vector(void *file, uint16_t vec[])
{
    char buf[132 + 1];
    char *s;
    int i;
    int value;

    while (TEST_GETS(buf, sizeof(buf), (FILE *)file))
    {
        if (buf[0] == '/'  &&  buf[1] == '*')
            continue;
        s = buf;
        i = 0;
        while ((value = hex_get(s)) >= 0)
        {
            vec[i++] = value;
            s += 4;
        }
        return i;
    }
    return 0;
}

/*!
 * Load a complete test vector file into a buffer.
 * \param file   Name of the file to read.
 * \param buf    Buffer to store the data.
 * \param max_len Maximum number of values to store.
 * \return The total number of values read, or -1 on error.
 */
static int get_test_vector(const char *file, uint16_t buf[], int max_len)
{
    int octets;
    int i;
    void *infile;

    (void)max_len;

    if ((infile = TEST_OPEN(file, "r")) == NULL)
    {
        TEST_PRINTF("    Failed to open '%s'\n", file);
        return -1;
    }
    octets = 0;
    while ((i = get_vector(infile, buf + octets)) > 0)
        octets += i;
    TEST_CLOSE(infile);
    return octets;
}

/*!
 * Perform all ITU-T G.722 compliance tests.
 * These tests use the codec in a special mode where the QMFs are disabled.
 * \return 0 on success, negative error code on failure.
 */
int itu_compliance_tests(void)
{
    g722_encode_state_t *enc_state;
    g722_decode_state_t *dec_state;
    int i;
    int j;
    int k;
    int len_comp;
    int len_comp_lower;
    int len_comp_upper;
    int len_data;
    int len;
    int len2;
    int mode;
    int file;

    /* ---- ENCODE TESTS (Configuration 1) ---- */
    for (file = 0;  encode_test_files[file];  file += 2)
    {
        TEST_PRINTF("Testing %s -> %s\n", encode_test_files[file], encode_test_files[file + 1]);

        /* Load input PCM data */
        len_data = get_test_vector(encode_test_files[file], (uint16_t *) itu.data, MAX_TEST_VECTOR_LEN);
        if (len_data < 0)
            return -1;
        /* Load reference encoded data */
        len_comp = get_test_vector(encode_test_files[file + 1], itu_ref, MAX_TEST_VECTOR_LEN);
        if (len_comp < 0)
            return -1;

        if (len_data != len_comp)
        {
            TEST_PRINTF("Test data length mismatch\n");
            return -2;
        }

        /* Find the active region (between start/stop markers) */
        for (i = 0;  i < len_data;  i++)
        {
            if ((itu.data[i] & 1) == 0)
                break;
        }
        for (j = i;  j < len_data;  j++)
        {
            if ((itu.data[j] & 1))
                break;
        }
        len = j - i;

        /* Encode using the special ITU test mode (QMF bypassed) */
        enc_state = g722_encode_init(NULL, 64000, 0);
        enc_state->itu_test_mode = true;
        len2 = g722_encode(enc_state, compressed, itu.data + i, len);

        /* Compare with the reference */
        j = 0;
        for (k = 0;  k < len2;  k++)
        {
            if ((compressed[k] & 0xFF) != ((itu_ref[k + i] >> 8) & 0xFF))
            {
                TEST_PRINTF(">>> %6d %4x %4x\n", k, compressed[k] & 0xFF, itu_ref[k + i] & 0xFFFF);
                j++;
            }
        }
        TEST_PRINTF("%d bad samples, out of %d/%d samples\n", j, len, len_data);
        if (j)
        {
            TEST_PRINTF("Test failed\n");
            g722_encode_release(enc_state);
            return -3;
        }
        TEST_PRINTF("Test passed\n");
        g722_encode_release(enc_state);
    }

    /* ---- DECODE TESTS (Configuration 2) ---- */
    /* Run for each mode: 1 = 48 kbps, 2 = 56 kbps, 3 = 64 kbps */
    for (mode = 1;  mode <= 3;  mode++)
    {
        for (file = 0;  decode_test_files[file];  file += 5)
        {
            TEST_PRINTF("Testing mode %d, %s -> %s + %s\n",
                   mode,
                   decode_test_files[file],
                   decode_test_files[file + mode],
                   decode_test_files[file + 4]);

            /* Load compressed input data */
            len_data = get_test_vector(decode_test_files[file], (uint16_t *) itu.data, MAX_TEST_VECTOR_LEN);
            if (len_data < 0)
                return -1;

            /*===============================================================================================*/
            /* We must do it here as itu.data is alias of itu.ref_upper!!!*/
            /*===============================================================================================*/
            /* Find the active region */
            for (i = 0;  i < len_data;  i++)
            {
                if ((itu.data[i] & 1) == 0)
                    break;
            }
            for (j = i;  j < len_data;  j++)
            {
                if ((itu.data[j] & 1))
                    break;
            }
            len = j - i;

            /* Extract the compressed bytes from the 16‑bit words.
               The reference files store compressed data in 16‑bit words,
               but the actual G.722 bytes are placed differently per mode. */
            for (k = 0;  k < len;  k++)
                compressed[k] = itu.data[k + i] >> ((mode == 3)  ?  10  :  (mode == 2)  ?  9  :  8);
            /*===============================================================================================*/

            /* Load lower band reference (mode dependent) */
            len_comp_lower = get_test_vector(decode_test_files[file + mode], itu_ref, MAX_TEST_VECTOR_LEN);
            if (len_comp_lower < 0)
                return -1;
            /* Load upper band reference (same for all modes) */
            len_comp_upper = get_test_vector(decode_test_files[file + 4], itu.ref_upper, MAX_TEST_VECTOR_LEN);
            if (len_comp_upper < 0)
                return -1;

            if (len_data != len_comp_lower  ||  len_data != len_comp_upper)
            {
                TEST_PRINTF("Test data length mismatch\n");
                return -2;
            }

            /* Decode using the appropriate bit rate and ITU test mode */
            dec_state = g722_decode_init(NULL,
                                         (mode == 3)  ?  48000  :  (mode == 2)  ?  56000  :  64000,
                                         0);
            dec_state->itu_test_mode = true;
            len2 = g722_decode(dec_state, decompressed, compressed, len);

            /* Compare each stereo (lower+upper) sample with the two references */
            j = 0;
            for (k = 0;  k < len2;  k += 2)
            {
                if ((decompressed[k] & 0xFFFF) != (itu_ref[(k >> 1) + i] & 0xFFFF)
                    ||
                    (decompressed[k + 1] & 0xFFFF) != (itu.ref_upper[(k >> 1) + i] & 0xFFFF))
                {
                    TEST_PRINTF(">>> %6d %4x %4x %4x %4x\n",
                           k >> 1,
                           decompressed[k] & 0xFFFF,
                           decompressed[k + 1] & 0xFFFF,
                           itu_ref[(k >> 1) + i] & 0xFFFF,
                           itu.ref_upper[(k >> 1) + i] & 0xFFFF);
                    j++;
                }
            }
            TEST_PRINTF("%d bad samples, out of %d/%d samples\n", j, len, len_data);
            if (j)
            {
                TEST_PRINTF("Test failed\n");
                g722_decode_release(dec_state);
                return -3;
            }
            TEST_PRINTF("Test passed\n");
            g722_decode_release(dec_state);
        }
    }

    TEST_PRINTF("All ITU-T compliance tests passed.\n");
    return 0;
}
/*- End of file ------------------------------------------------------------*/
