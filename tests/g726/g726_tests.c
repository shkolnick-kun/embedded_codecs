/*
 * SpanDSP - a series of DSP components for telephony
 *
 * g726_tests.c - Test G.726 encode and decode.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Steve Underwood
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

/*! \file g726_tests.c
 * \brief G.726 tests – ITU-T compliance tests only.
 *
 * This module implements the tests defined in the G.726 specification,
 * using the test data files supplied with the specification.
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
#include <string.h>

#include <g726.h>

#include "g726_tests.h"

#ifndef TESTDATA_DIR
#define TESTDATA_DIR        "../test-data/itu/g726/"
#endif

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

#define MAX_TEST_VECTOR_LEN 20000

static int16_t outdata[MAX_TEST_VECTOR_LEN];
static uint8_t adpcmdata[MAX_TEST_VECTOR_LEN];

static int16_t itudata[MAX_TEST_VECTOR_LEN];
static uint8_t itu_ref[MAX_TEST_VECTOR_LEN];
static uint8_t unpacked[MAX_TEST_VECTOR_LEN];
static uint8_t xlaw[MAX_TEST_VECTOR_LEN];

/*
Table 4 - Reset and homing sequences for u-law
            Normal                              I-input     Overload
Algorithm   Input   Intermediate    Output      Input       Output      Input   Intermediate    Output
            (PCM)   (ADPCM)         (PCM)       (ADPCM)     (PCM)       (PCM)   (ADPCM)         (PCM)

16F         NRM.M   RN16FM.I        RN16FM.O    I16         RI16FM.O    OVR.M   RV16FM.I        RV16FM.O
                    HN16FM.I        HN16FM.O                HI16FM.O            HV16FM.I        HV16FM.O

24F         NRM.M   RN24FM.I        RN24FM.O    I24         RI24FM.O    OVR.M   RV24FM.I        RV24FM.O
                    HN24FM.I        HN24FM.O                HI24FM.O            HV24FM.I        HV24FM.O

32F         NRM.M   RN32FM.I        RN32FM.O    I32         RI32FM.O    OVR.M   RV32FM.I        RV32FM.O
                    HN32FM.I        HN32FM.O                HI32FM.O            HV32FM.I        HV32FM.O

40F         NRM.M   RN40FM.I        RN40FM.O    I40         RI40FM.O    OVR.M   RV40FM.I        RV40FM.O
                    HN40FM.I        HN40FM.O                HI40FM.O            HV40FM.I        HV40FM.O


Table 5 - Reset and homing sequences for A-law
            Normal                              I-input     Overload
Algorithm   Input   Intermediate    Output      Input       Output      Input   Intermediate    Output
            (PCM)   (ADPCM)         (PCM)       (ADPCM)     (PCM)       (PCM)   (ADPCM)         (PCM)
16F         NRM.A   RN16FA.I        RN16FA.O    I16         RI16FA.O    OVR.A   RV16FA.I        RV16FA.O
                    HN16FA.I        HN16FA.O                HI16FA.O            HV16FA.I        HV16FA.O

24F         NRM.A   RN24FA.I        RN24FA.O    I24         RI24FA.O    OVR.A   RV24FA.I        RV24FA.O
                    HN24FA.I        HN24FA.O                HI24FA.O            HV24FA.I        HV24FA.O

32F         NRM.A   RN32FA.I        RN32FA.O    I32         RI32FA.O    OVR.A   RV32FA.I        RV32FA.O
                    HN32FA.I        HN32FA.O                HI32FA.O            HV32FA.I        HV32FA.O

40F         NRM.A   RN40FA.I        RN40FA.O    I40         RI40FA.O    OVR.A   RV40FA.I        RV40FA.O
                    HN40FA.I        HN40FA.O                HI40FA.O            HV40FA.I        HV40FA.O

Table 6 - Reset and homing cross sequences for u-law -> A-law
            Normal                              Overload
Algorithm   Input   Intermediate    Output      Input   Intermediate    Output
            (PCM)   (ADPCM)         (PCM)       (PCM)   (ADPCM)         (PCM)
16F         NRM.M   RN16FM.I        RN16FC.O    OVR.M   RV16FM.I        RV16FC.O
                    HN16FM.I        HN16FC.O            HV16FM.I        HV16FC.O

24F         NRM.M   RN24FM.I        RN24FC.O    OVR.M   RV24FM.I        RV24FC.O
                    HN24FM.I        HN24FC.O            HV24FM.I        HV24FC.O

32F         NRM.M   RN32FM.I        RN32FC.O    OVR.M   RV32FM.I        RV32FC.O
                    HN32FM.I        HN32FC.O            HV32FM.I        HV32FC.O

40F         NRM.M   RN40FM.I        RN40FC.O    OVR.M   RV40FM.I        RV40FC.O
                    HN40FM.I        HN40FC.O            HV40FM.I        HV40FC.O

Table 7 - Reset and homing cross sequences for A-law -> u-law
            Normal                              Overload
Algorithm   Input   Intermediate    Output      Input   Intermediate    Output
            (PCM)   (ADPCM)         (PCM)       (PCM)   (ADPCM)         (PCM)
16F         NRM.A   RN16FA.I        RN16FX.O    OVR.A   RV16FA.I        RV16FX.O
                    HN16FA.I        HN16FX.O            HV16FA.I        HV16FX.O

24F         NRM.A   RN24FA.I        RN24FX.O    OVR.A   RV24FA.I        RV24FX.O
                    HN24FA.I        HN24FX.O            HV24FA.I        HV24FX.O

32F         NRM.A   RN32FA.I        RN32FX.O    OVR.A   RV32FA.I        RV32FX.O
                    HN32FA.I        HN32FX.O            HV32FA.I        HV32FX.O

40F         NRM.A   RN40FA.I        RN40FX.O    OVR.A   RV40FA.I        RV40FX.O
                    HN40FA.I        HN40FX.O            HV40FA.I        HV40FX.O
*/

#define G726_ENCODING_NONE          9999

typedef struct
{
    const char *conditioning_pcm_file;
    const char *pcm_file;
    const char *conditioning_adpcm_file;
    const char *adpcm_file;
    const char *output_file;
    int rate;
    int compression_law;
    int decompression_law;
} test_set_t;

static test_set_t itu_test_sets[] =
{
    /* u-law to u-law tests */
    {
        "",
        TESTDATA_DIR "DISK1/INPUT/NRM.M",
        "",
        TESTDATA_DIR "DISK1/RESET/16/RN16FM.I",
        TESTDATA_DIR "DISK1/RESET/16/RN16FM.O",
        16000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ULAW
    },
    {
        "",
        "",
        "",
        TESTDATA_DIR "DISK1/INPUT/I16",
        TESTDATA_DIR "DISK1/RESET/16/RI16FM.O",
        16000,
        G726_ENCODING_NONE,
        G726_ENCODING_ULAW
    },
    {
        "",
        TESTDATA_DIR "DISK1/INPUT/OVR.M",
        "",
        TESTDATA_DIR "DISK1/RESET/16/RV16FM.I",
        TESTDATA_DIR "DISK1/RESET/16/RV16FM.O",
        16000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ULAW
    },
    {
        "",
        TESTDATA_DIR "DISK1/INPUT/NRM.M",
        "",
        TESTDATA_DIR "DISK1/RESET/24/RN24FM.I",
        TESTDATA_DIR "DISK1/RESET/24/RN24FM.O",
        24000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ULAW
    },
    {
        "",
        "",
        "",
        TESTDATA_DIR "DISK1/INPUT/I24",
        TESTDATA_DIR "DISK1/RESET/24/RI24FM.O",
        24000,
        G726_ENCODING_NONE,
        G726_ENCODING_ULAW
    },
    {
        "",
        TESTDATA_DIR "DISK1/INPUT/OVR.M",
        "",
        TESTDATA_DIR "DISK1/RESET/24/RV24FM.I",
        TESTDATA_DIR "DISK1/RESET/24/RV24FM.O",
        24000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ULAW
    },
    {
        "",
        TESTDATA_DIR "DISK1/INPUT/NRM.M",
        "",
        TESTDATA_DIR "DISK1/RESET/32/RN32FM.I",
        TESTDATA_DIR "DISK1/RESET/32/RN32FM.O",
        32000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ULAW
    },
    {
        "",
        "",
        "",
        TESTDATA_DIR "DISK1/INPUT/I32",
        TESTDATA_DIR "DISK1/RESET/32/RI32FM.O",
        32000,
        G726_ENCODING_NONE,
        G726_ENCODING_ULAW
    },
    {
        "",
        TESTDATA_DIR "DISK1/INPUT/OVR.M",
        "",
        TESTDATA_DIR "DISK1/RESET/32/RV32FM.I",
        TESTDATA_DIR "DISK1/RESET/32/RV32FM.O",
        32000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ULAW
    },
    {
        "",
        TESTDATA_DIR "DISK1/INPUT/NRM.M",
        "",
        TESTDATA_DIR "DISK1/RESET/40/RN40FM.I",
        TESTDATA_DIR "DISK1/RESET/40/RN40FM.O",
        40000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ULAW
    },
    {
        "",
        "",
        "",
        TESTDATA_DIR "DISK1/INPUT/I40",
        TESTDATA_DIR "DISK1/RESET/40/RI40FM.O",
        40000,
        G726_ENCODING_NONE,
        G726_ENCODING_ULAW
    },
    {
        "",
        TESTDATA_DIR "DISK1/INPUT/OVR.M",
        "",
        TESTDATA_DIR "DISK1/RESET/40/RV40FM.I",
        TESTDATA_DIR "DISK1/RESET/40/RV40FM.O",
        40000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ULAW
    },
    /* A-law to A-law tests */
    {
        "",
        TESTDATA_DIR "DISK2/INPUT/NRM.A",
        "",
        TESTDATA_DIR "DISK2/RESET/16/RN16FA.I",
        TESTDATA_DIR "DISK2/RESET/16/RN16FA.O",
        16000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ALAW
    },
    {
        "",
        "",
        "",
        TESTDATA_DIR "DISK2/INPUT/I16",
        TESTDATA_DIR "DISK2/RESET/16/RI16FA.O",
        16000,
        G726_ENCODING_NONE,
        G726_ENCODING_ALAW
    },
    {
        "",
        TESTDATA_DIR "DISK2/INPUT/OVR.A",
        "",
        TESTDATA_DIR "DISK2/RESET/16/RV16FA.I",
        TESTDATA_DIR "DISK2/RESET/16/RV16FA.O",
        16000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ALAW
    },
    {
        "",
        TESTDATA_DIR "DISK2/INPUT/NRM.A",
        "",
        TESTDATA_DIR "DISK2/RESET/24/RN24FA.I",
        TESTDATA_DIR "DISK2/RESET/24/RN24FA.O",
        24000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ALAW
    },
    {
        "",
        "",
        "",
        TESTDATA_DIR "DISK2/INPUT/I24",
        TESTDATA_DIR "DISK2/RESET/24/RI24FA.O",
        24000,
        G726_ENCODING_NONE,
        G726_ENCODING_ALAW
    },
    {
        "",
        TESTDATA_DIR "DISK2/INPUT/OVR.A",
        "",
        TESTDATA_DIR "DISK2/RESET/24/RV24FA.I",
        TESTDATA_DIR "DISK2/RESET/24/RV24FA.O",
        24000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ALAW
    },
    {
        "",
        TESTDATA_DIR "DISK2/INPUT/NRM.A",
        "",
        TESTDATA_DIR "DISK2/RESET/32/RN32FA.I",
        TESTDATA_DIR "DISK2/RESET/32/RN32FA.O",
        32000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ALAW
    },
    {
        "",
        "",
        "",
        TESTDATA_DIR "DISK2/INPUT/I32",
        TESTDATA_DIR "DISK2/RESET/32/RI32FA.O",
        32000,
        G726_ENCODING_NONE,
        G726_ENCODING_ALAW
    },
    {
        "",
        TESTDATA_DIR "DISK2/INPUT/OVR.A",
        "",
        TESTDATA_DIR "DISK2/RESET/32/RV32FA.I",
        TESTDATA_DIR "DISK2/RESET/32/RV32FA.O",
        32000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ALAW
    },
    {
        "",
        TESTDATA_DIR "DISK2/INPUT/NRM.A",
        "",
        TESTDATA_DIR "DISK2/RESET/40/RN40FA.I",
        TESTDATA_DIR "DISK2/RESET/40/RN40FA.O",
        40000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ALAW
    },
    {
        "",
        "",
        "",
        TESTDATA_DIR "DISK2/INPUT/I40",
        TESTDATA_DIR "DISK2/RESET/40/RI40FA.O",
        40000,
        G726_ENCODING_NONE,
        G726_ENCODING_ALAW
    },
    {
        "",
        TESTDATA_DIR "DISK2/INPUT/OVR.A",
        "",
        TESTDATA_DIR "DISK2/RESET/40/RV40FA.I",
        TESTDATA_DIR "DISK2/RESET/40/RV40FA.O",
        40000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ALAW
    },
    /* u-law to A-law tests */
    {
        "",
        TESTDATA_DIR "DISK1/INPUT/NRM.M",
        "",
        TESTDATA_DIR "DISK1/RESET/16/RN16FM.I",
        TESTDATA_DIR "DISK1/RESET/16/RN16FC.O",
        16000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ALAW
    },
    {
        "",
        TESTDATA_DIR "DISK1/INPUT/OVR.M",
        "",
        TESTDATA_DIR "DISK1/RESET/16/RV16FM.I",
        TESTDATA_DIR "DISK1/RESET/16/RV16FC.O",
        16000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ALAW
    },
    {
        "",
        TESTDATA_DIR "DISK1/INPUT/NRM.M",
        "",
        TESTDATA_DIR "DISK1/RESET/24/RN24FM.I",
        TESTDATA_DIR "DISK1/RESET/24/RN24FC.O",
        24000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ALAW
    },
    {
        "",
        TESTDATA_DIR "DISK1/INPUT/OVR.M",
        "",
        TESTDATA_DIR "DISK1/RESET/24/RV24FM.I",
        TESTDATA_DIR "DISK1/RESET/24/RV24FC.O",
        24000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ALAW
    },
    {
        "",
        TESTDATA_DIR "DISK1/INPUT/NRM.M",
        "",
        TESTDATA_DIR "DISK1/RESET/32/RN32FM.I",
        TESTDATA_DIR "DISK1/RESET/32/RN32FC.O",
        32000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ALAW
    },
    {
        "",
        TESTDATA_DIR "DISK1/INPUT/OVR.M",
        "",
        TESTDATA_DIR "DISK1/RESET/32/RV32FM.I",
        TESTDATA_DIR "DISK1/RESET/32/RV32FC.O",
        32000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ALAW
    },
    {
        "",
        TESTDATA_DIR "DISK1/INPUT/NRM.M",
        "",
        TESTDATA_DIR "DISK1/RESET/40/RN40FM.I",
        TESTDATA_DIR "DISK1/RESET/40/RN40FC.O",
        40000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ALAW
    },
    {
        "",
        TESTDATA_DIR "DISK1/INPUT/OVR.M",
        "",
        TESTDATA_DIR "DISK1/RESET/40/RV40FM.I",
        TESTDATA_DIR "DISK1/RESET/40/RV40FC.O",
        40000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ALAW
    },
    /* A-law to u-law tests */
    {
        "",
        TESTDATA_DIR "DISK2/INPUT/NRM.A",
        "",
        TESTDATA_DIR "DISK2/RESET/16/RN16FA.I",
        TESTDATA_DIR "DISK2/RESET/16/RN16FX.O",
        16000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ULAW
    },
    {
        "",
        TESTDATA_DIR "DISK2/INPUT/OVR.A",
        "",
        TESTDATA_DIR "DISK2/RESET/16/RV16FA.I",
        TESTDATA_DIR "DISK2/RESET/16/RV16FX.O",
        16000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ULAW
    },
    {
        "",
        TESTDATA_DIR "DISK2/INPUT/NRM.A",
        "",
        TESTDATA_DIR "DISK2/RESET/24/RN24FA.I",
        TESTDATA_DIR "DISK2/RESET/24/RN24FX.O",
        24000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ULAW
    },
    {
        "",
        TESTDATA_DIR "DISK2/INPUT/OVR.A",
        "",
        TESTDATA_DIR "DISK2/RESET/24/RV24FA.I",
        TESTDATA_DIR "DISK2/RESET/24/RV24FX.O",
        24000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ULAW
    },
    {
        "",
        TESTDATA_DIR "DISK2/INPUT/NRM.A",
        "",
        TESTDATA_DIR "DISK2/RESET/32/RN32FA.I",
        TESTDATA_DIR "DISK2/RESET/32/RN32FX.O",
        32000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ULAW
    },
    {
        "",
        TESTDATA_DIR "DISK2/INPUT/OVR.A",
        "",
        TESTDATA_DIR "DISK2/RESET/32/RV32FA.I",
        TESTDATA_DIR "DISK2/RESET/32/RV32FX.O",
        32000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ULAW
    },
    {
        "",
        TESTDATA_DIR "DISK2/INPUT/NRM.A",
        "",
        TESTDATA_DIR "DISK2/RESET/40/RN40FA.I",
        TESTDATA_DIR "DISK2/RESET/40/RN40FX.O",
        40000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ULAW
    },
    {
        "",
        TESTDATA_DIR "DISK2/INPUT/OVR.A",
        "",
        TESTDATA_DIR "DISK2/RESET/40/RV40FA.I",
        TESTDATA_DIR "DISK2/RESET/40/RV40FX.O",
        40000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ULAW
    },
    /* u-law to u-law tests */
    {
        TESTDATA_DIR "DISK1/PCM_INIT.M",
        TESTDATA_DIR "DISK1/INPUT/NRM.M",
        TESTDATA_DIR "DISK1/HOMING/16/I_INI_16.M",
        TESTDATA_DIR "DISK1/HOMING/16/HN16FM.I",
        TESTDATA_DIR "DISK1/HOMING/16/HN16FM.O",
        16000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ULAW
    },
    {
        "",
        "",
        TESTDATA_DIR "DISK1/HOMING/16/I_INI_16.M",
        TESTDATA_DIR "DISK1/INPUT/I16",
        TESTDATA_DIR "DISK1/HOMING/16/HI16FM.O",
        16000,
        G726_ENCODING_NONE,
        G726_ENCODING_ULAW
    },
    {
        TESTDATA_DIR "DISK1/PCM_INIT.M",
        TESTDATA_DIR "DISK1/INPUT/OVR.M",
        TESTDATA_DIR "DISK1/HOMING/16/I_INI_16.M",
        TESTDATA_DIR "DISK1/HOMING/16/HV16FM.I",
        TESTDATA_DIR "DISK1/HOMING/16/HV16FM.O",
        16000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ULAW
    },
    {
        TESTDATA_DIR "DISK1/PCM_INIT.M",
        TESTDATA_DIR "DISK1/INPUT/NRM.M",
        TESTDATA_DIR "DISK1/HOMING/24/I_INI_24.M",
        TESTDATA_DIR "DISK1/HOMING/24/HN24FM.I",
        TESTDATA_DIR "DISK1/HOMING/24/HN24FM.O",
        24000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ULAW
    },
    {
        "",
        "",
        TESTDATA_DIR "DISK1/HOMING/24/I_INI_24.M",
        TESTDATA_DIR "DISK1/INPUT/I24",
        TESTDATA_DIR "DISK1/HOMING/24/HI24FM.O",
        24000,
        G726_ENCODING_NONE,
        G726_ENCODING_ULAW
    },
    {
        TESTDATA_DIR "DISK1/PCM_INIT.M",
        TESTDATA_DIR "DISK1/INPUT/OVR.M",
        TESTDATA_DIR "DISK1/HOMING/24/I_INI_24.M",
        TESTDATA_DIR "DISK1/HOMING/24/HV24FM.I",
        TESTDATA_DIR "DISK1/HOMING/24/HV24FM.O",
        24000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ULAW
    },
    {
        TESTDATA_DIR "DISK1/PCM_INIT.M",
        TESTDATA_DIR "DISK1/INPUT/NRM.M",
        TESTDATA_DIR "DISK1/HOMING/32/I_INI_32.M",
        TESTDATA_DIR "DISK1/HOMING/32/HN32FM.I",
        TESTDATA_DIR "DISK1/HOMING/32/HN32FM.O",
        32000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ULAW
    },
    {
        "",
        "",
        TESTDATA_DIR "DISK1/HOMING/32/I_INI_32.M",
        TESTDATA_DIR "DISK1/INPUT/I32",
        TESTDATA_DIR "DISK1/HOMING/32/HI32FM.O",
        32000,
        G726_ENCODING_NONE,
        G726_ENCODING_ULAW
    },
    {
        TESTDATA_DIR "DISK1/PCM_INIT.M",
        TESTDATA_DIR "DISK1/INPUT/OVR.M",
        TESTDATA_DIR "DISK1/HOMING/32/I_INI_32.M",
        TESTDATA_DIR "DISK1/HOMING/32/HV32FM.I",
        TESTDATA_DIR "DISK1/HOMING/32/HV32FM.O",
        32000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ULAW
    },
    {
        TESTDATA_DIR "DISK1/PCM_INIT.M",
        TESTDATA_DIR "DISK1/INPUT/NRM.M",
        TESTDATA_DIR "DISK1/HOMING/40/I_INI_40.M",
        TESTDATA_DIR "DISK1/HOMING/40/HN40FM.I",
        TESTDATA_DIR "DISK1/HOMING/40/HN40FM.O",
        40000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ULAW
    },
    {
        "",
        "",
        TESTDATA_DIR "DISK1/HOMING/40/I_INI_40.M",
        TESTDATA_DIR "DISK1/INPUT/I40",
        TESTDATA_DIR "DISK1/HOMING/40/HI40FM.O",
        40000,
        G726_ENCODING_NONE,
        G726_ENCODING_ULAW
    },
    {
        TESTDATA_DIR "DISK1/PCM_INIT.M",
        TESTDATA_DIR "DISK1/INPUT/OVR.M",
        TESTDATA_DIR "DISK1/HOMING/40/I_INI_40.M",
        TESTDATA_DIR "DISK1/HOMING/40/HV40FM.I",
        TESTDATA_DIR "DISK1/HOMING/40/HV40FM.O",
        40000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ULAW
    },
    /* A-law to A-law tests */
    {
        TESTDATA_DIR "DISK2/PCM_INIT.A",
        TESTDATA_DIR "DISK2/INPUT/NRM.A",
        TESTDATA_DIR "DISK2/HOMING/16/I_INI_16.A",
        TESTDATA_DIR "DISK2/HOMING/16/HN16FA.I",
        TESTDATA_DIR "DISK2/HOMING/16/HN16FA.O",
        16000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ALAW
    },
    {
        "",
        "",
        TESTDATA_DIR "DISK2/HOMING/16/I_INI_16.A",
        TESTDATA_DIR "DISK2/INPUT/I16",
        TESTDATA_DIR "DISK2/HOMING/16/HI16FA.O",
        16000,
        G726_ENCODING_NONE,
        G726_ENCODING_ALAW
    },
    {
        TESTDATA_DIR "DISK2/PCM_INIT.A",
        TESTDATA_DIR "DISK2/INPUT/OVR.A",
        TESTDATA_DIR "DISK2/HOMING/16/I_INI_16.A",
        TESTDATA_DIR "DISK2/HOMING/16/HV16FA.I",
        TESTDATA_DIR "DISK2/HOMING/16/HV16FA.O",
        16000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ALAW
    },
    {
        TESTDATA_DIR "DISK2/PCM_INIT.A",
        TESTDATA_DIR "DISK2/INPUT/NRM.A",
        TESTDATA_DIR "DISK2/HOMING/24/I_INI_24.A",
        TESTDATA_DIR "DISK2/HOMING/24/HN24FA.I",
        TESTDATA_DIR "DISK2/HOMING/24/HN24FA.O",
        24000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ALAW
    },
    {
        "",
        "",
        TESTDATA_DIR "DISK2/HOMING/24/I_INI_24.A",
        TESTDATA_DIR "DISK2/INPUT/I24",
        TESTDATA_DIR "DISK2/HOMING/24/HI24FA.O",
        24000,
        G726_ENCODING_NONE,
        G726_ENCODING_ALAW
    },
    {
        TESTDATA_DIR "DISK2/PCM_INIT.A",
        TESTDATA_DIR "DISK2/INPUT/OVR.A",
        TESTDATA_DIR "DISK2/HOMING/24/I_INI_24.A",
        TESTDATA_DIR "DISK2/HOMING/24/HV24FA.I",
        TESTDATA_DIR "DISK2/HOMING/24/HV24FA.O",
        24000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ALAW
    },
    {
        TESTDATA_DIR "DISK2/PCM_INIT.A",
        TESTDATA_DIR "DISK2/INPUT/NRM.A",
        TESTDATA_DIR "DISK2/HOMING/32/I_INI_32.A",
        TESTDATA_DIR "DISK2/HOMING/32/HN32FA.I",
        TESTDATA_DIR "DISK2/HOMING/32/HN32FA.O",
        32000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ALAW
    },
    {
        "",
        "",
        TESTDATA_DIR "DISK2/HOMING/32/I_INI_32.A",
        TESTDATA_DIR "DISK2/INPUT/I32",
        TESTDATA_DIR "DISK2/HOMING/32/HI32FA.O",
        32000,
        G726_ENCODING_NONE,
        G726_ENCODING_ALAW
    },
    {
        TESTDATA_DIR "DISK2/PCM_INIT.A",
        TESTDATA_DIR "DISK2/INPUT/OVR.A",
        TESTDATA_DIR "DISK2/HOMING/32/I_INI_32.A",
        TESTDATA_DIR "DISK2/HOMING/32/HV32FA.I",
        TESTDATA_DIR "DISK2/HOMING/32/HV32FA.O",
        32000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ALAW
    },
    {
        TESTDATA_DIR "DISK2/PCM_INIT.A",
        TESTDATA_DIR "DISK2/INPUT/NRM.A",
        TESTDATA_DIR "DISK2/HOMING/40/I_INI_40.A",
        TESTDATA_DIR "DISK2/HOMING/40/HN40FA.I",
        TESTDATA_DIR "DISK2/HOMING/40/HN40FA.O",
        40000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ALAW
    },
    {
        "",
        "",
        TESTDATA_DIR "DISK2/HOMING/40/I_INI_40.A",
        TESTDATA_DIR "DISK2/INPUT/I40",
        TESTDATA_DIR "DISK2/HOMING/40/HI40FA.O",
        40000,
        G726_ENCODING_NONE,
        G726_ENCODING_ALAW
    },
    {
        TESTDATA_DIR "DISK2/PCM_INIT.A",
        TESTDATA_DIR "DISK2/INPUT/OVR.A",
        TESTDATA_DIR "DISK2/HOMING/40/I_INI_40.A",
        TESTDATA_DIR "DISK2/HOMING/40/HV40FA.I",
        TESTDATA_DIR "DISK2/HOMING/40/HV40FA.O",
        40000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ALAW
    },
    /* u-law to A-law tests */
    {
        TESTDATA_DIR "DISK1/PCM_INIT.M",
        TESTDATA_DIR "DISK1/INPUT/NRM.M",
        TESTDATA_DIR "DISK2/HOMING/16/I_INI_16.A",
        TESTDATA_DIR "DISK1/HOMING/16/HN16FM.I",
        TESTDATA_DIR "DISK1/HOMING/16/HN16FC.O",
        16000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ALAW
    },
    {
        TESTDATA_DIR "DISK1/PCM_INIT.M",
        TESTDATA_DIR "DISK1/INPUT/OVR.M",
        TESTDATA_DIR "DISK2/HOMING/16/I_INI_16.A",
        TESTDATA_DIR "DISK1/HOMING/16/HV16FM.I",
        TESTDATA_DIR "DISK1/HOMING/16/HV16FC.O",
        16000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ALAW
    },
    {
        TESTDATA_DIR "DISK1/PCM_INIT.M",
        TESTDATA_DIR "DISK1/INPUT/NRM.M",
        TESTDATA_DIR "DISK2/HOMING/24/I_INI_24.A",
        TESTDATA_DIR "DISK1/HOMING/24/HN24FM.I",
        TESTDATA_DIR "DISK1/HOMING/24/HN24FC.O",
        24000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ALAW
    },
    {
        TESTDATA_DIR "DISK1/PCM_INIT.M",
        TESTDATA_DIR "DISK1/INPUT/OVR.M",
        TESTDATA_DIR "DISK2/HOMING/24/I_INI_24.A",
        TESTDATA_DIR "DISK1/HOMING/24/HV24FM.I",
        TESTDATA_DIR "DISK1/HOMING/24/HV24FC.O",
        24000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ALAW
    },
    {
        TESTDATA_DIR "DISK1/PCM_INIT.M",
        TESTDATA_DIR "DISK1/INPUT/NRM.M",
        TESTDATA_DIR "DISK2/HOMING/32/I_INI_32.A",
        TESTDATA_DIR "DISK1/HOMING/32/HN32FM.I",
        TESTDATA_DIR "DISK1/HOMING/32/HN32FC.O",
        32000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ALAW
    },
    {
        TESTDATA_DIR "DISK1/PCM_INIT.M",
        TESTDATA_DIR "DISK1/INPUT/OVR.M",
        TESTDATA_DIR "DISK2/HOMING/32/I_INI_32.A",
        TESTDATA_DIR "DISK1/HOMING/32/HV32FM.I",
        TESTDATA_DIR "DISK1/HOMING/32/HV32FC.O",
        32000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ALAW
    },
    {
        TESTDATA_DIR "DISK1/PCM_INIT.M",
        TESTDATA_DIR "DISK1/INPUT/NRM.M",
        TESTDATA_DIR "DISK2/HOMING/40/I_INI_40.A",
        TESTDATA_DIR "DISK1/HOMING/40/HN40FM.I",
        TESTDATA_DIR "DISK1/HOMING/40/HN40FC.O",
        40000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ALAW
    },
    {
        TESTDATA_DIR "DISK1/PCM_INIT.M",
        TESTDATA_DIR "DISK1/INPUT/OVR.M",
        TESTDATA_DIR "DISK2/HOMING/40/I_INI_40.A",
        TESTDATA_DIR "DISK1/HOMING/40/HV40FM.I",
        TESTDATA_DIR "DISK1/HOMING/40/HV40FC.O",
        40000,
        G726_ENCODING_ULAW,
        G726_ENCODING_ALAW
    },
    /* A-law to u-law tests */
    {
        TESTDATA_DIR "DISK2/PCM_INIT.A",
        TESTDATA_DIR "DISK2/INPUT/NRM.A",
        TESTDATA_DIR "DISK1/HOMING/16/I_INI_16.M",
        TESTDATA_DIR "DISK2/HOMING/16/HN16FA.I",
        TESTDATA_DIR "DISK2/HOMING/16/HN16FX.O",
        16000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ULAW
    },
    {
        TESTDATA_DIR "DISK2/PCM_INIT.A",
        TESTDATA_DIR "DISK2/INPUT/OVR.A",
        TESTDATA_DIR "DISK1/HOMING/16/I_INI_16.M",
        TESTDATA_DIR "DISK2/HOMING/16/HV16FA.I",
        TESTDATA_DIR "DISK2/HOMING/16/HV16FX.O",
        16000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ULAW
    },
    {
        TESTDATA_DIR "DISK2/PCM_INIT.A",
        TESTDATA_DIR "DISK2/INPUT/NRM.A",
        TESTDATA_DIR "DISK1/HOMING/24/I_INI_24.M",
        TESTDATA_DIR "DISK2/HOMING/24/HN24FA.I",
        TESTDATA_DIR "DISK2/HOMING/24/HN24FX.O",
        24000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ULAW
    },
    {
        TESTDATA_DIR "DISK2/PCM_INIT.A",
        TESTDATA_DIR "DISK2/INPUT/OVR.A",
        TESTDATA_DIR "DISK1/HOMING/24/I_INI_24.M",
        TESTDATA_DIR "DISK2/HOMING/24/HV24FA.I",
        TESTDATA_DIR "DISK2/HOMING/24/HV24FX.O",
        24000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ULAW
    },
    {
        TESTDATA_DIR "DISK2/PCM_INIT.A",
        TESTDATA_DIR "DISK2/INPUT/NRM.A",
        TESTDATA_DIR "DISK1/HOMING/32/I_INI_32.M",
        TESTDATA_DIR "DISK2/HOMING/32/HN32FA.I",
        TESTDATA_DIR "DISK2/HOMING/32/HN32FX.O",
        32000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ULAW
    },
    {
        TESTDATA_DIR "DISK2/PCM_INIT.A",
        TESTDATA_DIR "DISK2/INPUT/OVR.A",
        TESTDATA_DIR "DISK1/HOMING/32/I_INI_32.M",
        TESTDATA_DIR "DISK2/HOMING/32/HV32FA.I",
        TESTDATA_DIR "DISK2/HOMING/32/HV32FX.O",
        32000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ULAW
    },
    {
        TESTDATA_DIR "DISK2/PCM_INIT.A",
        TESTDATA_DIR "DISK2/INPUT/NRM.A",
        TESTDATA_DIR "DISK1/HOMING/40/I_INI_40.M",
        TESTDATA_DIR "DISK2/HOMING/40/HN40FA.I",
        TESTDATA_DIR "DISK2/HOMING/40/HN40FX.O",
        40000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ULAW
    },
    {
        TESTDATA_DIR "DISK2/PCM_INIT.A",
        TESTDATA_DIR "DISK2/INPUT/OVR.A",
        TESTDATA_DIR "DISK1/HOMING/40/I_INI_40.M",
        TESTDATA_DIR "DISK2/HOMING/40/HV40FA.I",
        TESTDATA_DIR "DISK2/HOMING/40/HV40FX.O",
        40000,
        G726_ENCODING_ALAW,
        G726_ENCODING_ULAW
    },
    {
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        0,
        0,
        0
    }
};

/* ------------------------------------------------------------------------- */
/* Helper functions                                                          */
/* ------------------------------------------------------------------------- */

static int hex_get(char *s)
{
    int i;
    int value;
    int x;

    for (value = i = 0;  i < 2;  i++)
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

static int get_vector(void *file, uint8_t vec[])
{
    char buf[132 + 1];
    char *s;
    int i;
    int value;

    while (TEST_GETS(buf, sizeof(buf), (FILE *)file))
    {
        s = buf;
        i = 0;
        while ((value = hex_get(s)) >= 0)
        {
            vec[i++] = value;
            s += 2;
        }
        return i;
    }
    return 0;
}

/*!
 * Load a test vector file.
 * \param file   Name of the file to read.
 * \param buf    Buffer to store the data.
 * \param max_len Maximum number of values to store.
 * \return The total number of values read, or negative error code.
 */
static int get_test_vector(const char *file, uint8_t buf[], int max_len)
{
    int octets;
    int i;
    int sum;
    void *infile;

    (void)max_len;

    if ((infile = TEST_OPEN(file, "r")) == NULL)
    {
        TEST_PRINTF("[ERROR]: Failed to open '%s'\n", file);
        return -1;
    }
    octets = 0;
    while ((i = get_vector(infile, buf + octets)) > 0)
        octets += i;
    TEST_CLOSE(infile);
    /* The last octet is a sumcheck, so the real data octets are one less than
       the total we have */
    octets--;
    /* Test the checksum */
    for (sum = i = 0;  i < octets;  i++)
        sum += buf[i];
    if (sum % 255 != (int) buf[i])
    {
        TEST_PRINTF("[ERROR]: Sumcheck failed in '%s' - %x %x\n", file, sum % 255, buf[i]);
        return -2;
    }
    return octets;
}

/* ------------------------------------------------------------------------- */
/* Public function                                                           */
/* ------------------------------------------------------------------------- */

/*!
 * Perform all ITU-T G.726 compliance tests.
 * \return 0 on success, negative error code on failure.
 */
int itu_compliance_tests(void)
{
    g726_state_t enc_state;
    g726_state_t dec_state;
    int len2;
    int len3;
    int i;
    int test;
    int bad_samples;
    int conditioning_samples;
    int samples;
    int conditioning_adpcm;
    int adpcm;
    int result;

    len2 = 0;
    conditioning_samples = 0;
    for (test = 0;  itu_test_sets[test].rate;  test++)
    {
        TEST_PRINTF("Test %2d: '%s' + '%s'\n"
                    "      -> '%s' + '%s'\n"
                    "      -> '%s' [%d, %d, %d]\n",
                    test,
                    itu_test_sets[test].conditioning_pcm_file,
                    itu_test_sets[test].pcm_file,
                    itu_test_sets[test].conditioning_adpcm_file,
                    itu_test_sets[test].adpcm_file,
                    itu_test_sets[test].output_file,
                    itu_test_sets[test].rate,
                    itu_test_sets[test].compression_law,
                    itu_test_sets[test].decompression_law);
        if (itu_test_sets[test].compression_law != G726_ENCODING_NONE)
        {
            /* Test the encode side */
            g726_init(&enc_state, itu_test_sets[test].rate, itu_test_sets[test].compression_law, G726_PACKING_NONE);
            if (itu_test_sets[test].conditioning_pcm_file[0])
            {
                conditioning_samples = get_test_vector(itu_test_sets[test].conditioning_pcm_file, xlaw, MAX_TEST_VECTOR_LEN);
                if (conditioning_samples < 0)
                    return conditioning_samples;
                TEST_PRINTF("Test %d: Homing %d samples at %dbps\n", test, conditioning_samples, itu_test_sets[test].rate);
            }
            else
            {
                conditioning_samples = 0;
            }
            samples = get_test_vector(itu_test_sets[test].pcm_file, xlaw + conditioning_samples, MAX_TEST_VECTOR_LEN);
            if (samples < 0)
                return samples;
            memcpy(itudata, xlaw, conditioning_samples + samples);
            TEST_PRINTF("Test %d: Compressing %d samples at %dbps\n", test, samples, itu_test_sets[test].rate);
            len2 = g726_encode(&enc_state, adpcmdata, itudata, conditioning_samples + samples);
        }

        /* Test the decode side */
        g726_init(&dec_state, itu_test_sets[test].rate, itu_test_sets[test].decompression_law, G726_PACKING_NONE);
        if (itu_test_sets[test].conditioning_adpcm_file[0])
        {
            conditioning_adpcm = get_test_vector(itu_test_sets[test].conditioning_adpcm_file, unpacked, MAX_TEST_VECTOR_LEN);
            if (conditioning_adpcm < 0)
                return conditioning_adpcm;
            TEST_PRINTF("Test %d: Homing %d octets at %dbps\n", test, conditioning_adpcm, itu_test_sets[test].rate);
        }
        else
        {
            conditioning_adpcm = 0;
        }
        adpcm = get_test_vector(itu_test_sets[test].adpcm_file, unpacked + conditioning_adpcm, MAX_TEST_VECTOR_LEN);
        if (adpcm < 0)
            return adpcm;

        if (itu_test_sets[test].compression_law != G726_ENCODING_NONE)
        {
            /* Test our compressed version against the reference compressed version */
            TEST_PRINTF("Test %d: Compressed data check - %d/%d octets\n", test, conditioning_adpcm + adpcm, len2);
            if (conditioning_adpcm + adpcm == len2)
            {
                for (bad_samples = 0, i = conditioning_samples;  i < len2;  i++)
                {
                    if (adpcmdata[i] != unpacked[i])
                    {
                        bad_samples++;
                        TEST_PRINTF("Test %d: Compressed mismatch %d %x %x\n", test, i, adpcmdata[i], unpacked[i]);
                    }
                }
                if (bad_samples > 0)
                {
                    TEST_PRINTF("Test failed\n");
                    return -4;
                }
                TEST_PRINTF("Test passed\n");
            }
            else
            {
                TEST_PRINTF("Test %d: Length mismatch - ref = %d, processed = %d\n", test, conditioning_adpcm + adpcm, len2);
                return -3;
            }
        }

        len3 = g726_decode(&dec_state, outdata, unpacked, conditioning_adpcm + adpcm);

        /* Get the output reference data */
        samples = get_test_vector(itu_test_sets[test].output_file, xlaw, MAX_TEST_VECTOR_LEN);
        if (samples < 0)
            return samples;
        memcpy(itu_ref, xlaw, samples);
        /* Test our decompressed version against the reference decompressed version */
        TEST_PRINTF("Test %d: Decompressed data check - %d/%d samples\n", test, samples, len3 - conditioning_adpcm);
        if (samples == len3 - conditioning_adpcm)
        {
            for (bad_samples = 0, i = 0;  i < len3;  i++)
            {
                if (itu_ref[i] != ((uint8_t *) outdata)[i + conditioning_adpcm])
                {
                    bad_samples++;
                    TEST_PRINTF("Test %d: Decompressed mismatch %d %x %x\n", test, i, itu_ref[i], ((uint8_t *) outdata)[i + conditioning_adpcm]);
                }
            }
            if (bad_samples > 0)
            {
                TEST_PRINTF("Test failed\n");
                return -4;
            }
            TEST_PRINTF("Test passed\n");
        }
        else
        {
            TEST_PRINTF("Test %d: Length mismatch - ref = %d, processed = %d\n", test, samples, len3 - conditioning_adpcm);
            return -3;
        }
    }

    TEST_PRINTF("Tests passed.\n");
    return 0;
}
