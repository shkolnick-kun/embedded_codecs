/*
 * SpanDSP - a series of DSP components for telephony
 *
 * g722.h - The ITU G.722 codec.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2005 Steve Underwood
 *
 *  Despite my general liking of the GPL, I place my own contributions
 *  to this code in the public domain for the benefit of all mankind -
 *  even the slimy ones who might try to proprietize my work and use it
 *  to my detriment.
 *
 * Based on a single channel G.722 codec which is:
 *
 *****    Copyright (c) CMU    1993      *****
 * Computer Science, Speech Group
 * Chengxiang Lu and Alex Hauptmann
 *
 * $Id: g722.h,v 1.10 2006/06/16 12:45:53 steveu Exp $
 */


/*! \file */

#if !defined(_G722_H_)
#define _G722_H_

#include <stdint.h>

/*! \page g722_page G.722 encoding and decoding
\section g722_page_sec_1 What does it do?
The G.722 module is a bit exact implementation of the ITU G.722 specification for all three
specified bit rates - 64000bps, 56000bps and 48000bps. It passes the ITU tests.

To allow fast and flexible interworking with narrow band telephony, the encoder and decoder
support an option for the linear audio to be an 8k samples/second stream. In this mode the
codec is considerably faster, and still fully compatible with wideband terminals using G.722.

\section g722_page_sec_2 How does it work?
???.
*/

enum
{
    G722_SAMPLE_RATE_8000 = 0x0001,
    G722_PACKED = 0x0002
};

typedef struct
{
    /*! TRUE if the operating in the special ITU test mode, with the band split filters
             disabled. */
    int32_t itu_test_mode;
    /*! TRUE if the G.722 data is packed */
    int32_t packed;
    /*! TRUE if encode from 8k samples/second */
    int32_t eight_k;
    /*! 6 for 48000kbps, 7 for 56000kbps, or 8 for 64000kbps. */
    int32_t bits_per_sample;

    /*! Signal history for the QMF */
    int32_t x[24];

    struct
    {
        int32_t s;
        int32_t sp;
        int32_t sz;
        int32_t r[3];
        int32_t a[3];
        int32_t ap[3];
        int32_t p[3];
        int32_t d[7];
        int32_t b[7];
        int32_t bp[7];
        int32_t sg[7];
        int32_t nb;
        int32_t det;
    } band[2];

    uint32_t in_buffer;
    int32_t in_bits;
    uint32_t out_buffer;
    int32_t out_bits;
} g722_encode_state_t;

typedef struct
{
    /*! TRUE if the operating in the special ITU test mode, with the band split filters
             disabled. */
    int32_t itu_test_mode;
    /*! TRUE if the G.722 data is packed */
    int32_t packed;
    /*! TRUE if decode to 8k samples/second */
    int32_t eight_k;
    /*! 6 for 48000kbps, 7 for 56000kbps, or 8 for 64000kbps. */
    int32_t bits_per_sample;

    /*! Signal history for the QMF */
    int32_t x[24];

    struct
    {
        int32_t s;
        int32_t sp;
        int32_t sz;
        int32_t r[3];
        int32_t a[3];
        int32_t ap[3];
        int32_t p[3];
        int32_t d[7];
        int32_t b[7];
        int32_t bp[7];
        int32_t sg[7];
        int32_t nb;
        int32_t det;
    } band[2];

    uint32_t in_buffer;
    int32_t in_bits;
    uint32_t out_buffer;
    int32_t out_bits;
} g722_decode_state_t;

#ifdef __cplusplus
extern "C" {
#endif

g722_encode_state_t *g722_encode_init(g722_encode_state_t *s, int32_t rate, int32_t options);
int32_t g722_encode_release(g722_encode_state_t *s);
int32_t g722_encode(g722_encode_state_t *s, uint8_t g722_data[], const int16_t amp[], int32_t len);

g722_decode_state_t *g722_decode_init(g722_decode_state_t *s, int32_t rate, int32_t options);
int32_t g722_decode_release(g722_decode_state_t *s);
int32_t g722_decode(g722_decode_state_t *s, int16_t amp[], const uint8_t g722_data[], int32_t len);

#ifdef __cplusplus
}
#endif

#endif
