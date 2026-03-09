/*
 * Copyright (C) 2005 to 2013 by Jonathan Duddington
 * email: jonsd@users.sourceforge.net
 * Copyright (C) 2015-2016 Reece H. Dunn
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see: <http://www.gnu.org/licenses/>.
 */

// this version keeps wavemult window as a constant fraction
// of the cycle length - but that spreads out the HF peaks too much

#include "config.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <espeak-ng/espeak_ng.h>
#include <espeak-ng/speak_lib.h>

#include "wavegen.h"
#include "common.h"                   // for espeak_rand
#include "synthesize.h"               // for WGEN_DATA, RESONATOR, frame_t

#include "sintab.h"
#include "speech.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static voice_t *wvoice = NULL;

static int option_harmonic1 = 10;

static int general_amplitude = 60;
static int consonant_amp = 26;

int embedded_value[N_EMBEDDED_VALUES];

static int PHASE_INC_FACTOR;
int samplerate = 0; // this is set by Wavegeninit()

static int peak_harmonic[N_PEAKS];
static int peak_height[N_PEAKS];

int echo_head;
int echo_tail;
int echo_amp = 0;
short echo_buf[N_ECHO_BUF];
static int echo_length = 0; // period (in sample\) to ensure completion of echo at the end of speech, set in WavegenSetEcho()

static int voicing;
static RESONATOR rbreath[N_PEAKS];

#define N_LOWHARM  30
#define MAX_HARMONIC 400 // 400 * 50Hz = 20 kHz, more than enough
static int harm_inc[N_LOWHARM]; // only for these harmonics do we interpolate amplitude between steps
static int *harmspect;

static int nsamples = 0; // number to do

static WGEN_DATA wdata;

static int samplecount = 0; // number done
static int wavephase;

static double minus_pi_t;
static double two_pi_t;

unsigned char *out_ptr;
unsigned char *out_end;

espeak_ng_OUTPUT_HOOKS* output_hooks = NULL;
static int const_f0 = 0;

// the queue of operations passed to wavegen from sythesize
intptr_t wcmdq[N_WCMDQ][4];
int wcmdq_head = 0;
int wcmdq_tail = 0;

// pitch,speed,
const int embedded_default[N_EMBEDDED_VALUES]    = { 0,     50, espeakRATE_NORMAL, 100, 50,  0,  0, 0, espeakRATE_NORMAL, 0, 0, 0, 0, 0, 0 };

// Flutter table, to add natural variations to the pitch
#define N_FLUTTER  0x170

// waveform shape table for HF peaks, formants 6,7,8
#define N_WAVEMULT 128
static int wavemult_offset = 0;
static int wavemult_max = 0;

// the presets are for 22050 Hz sample rate.
// A different rate will need to recalculate the presets in WavegenInit()
static unsigned char wavemult[N_WAVEMULT] = {
	  0,   0,   0,   2,   3,   5,   8,  11,  14,  18,  22,  27,  32,  37,  43,  49,
	 55,  62,  69,  76,  83,  90,  98, 105, 113, 121, 128, 136, 144, 152, 159, 166,
	174, 181, 188, 194, 201, 207, 213, 218, 224, 228, 233, 237, 240, 244, 246, 249,
	251, 252, 253, 253, 253, 253, 252, 251, 249, 246, 244, 240, 237, 233, 228, 224,
	218, 213, 207, 201, 194, 188, 181, 174, 166, 159, 152, 144, 136, 128, 121, 113,
	105,  98,  90,  83,  76,  69,  62,  55,  49,  43,  37,  32,  27,  22,  18,  14,
	 11,   8,   5,   3,   2,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

// set from y = pow(2,x) * 128,  x=-1 to 1
#define MAX_PITCH_VALUE  101

void WcmdqInc(void)
{
	wcmdq_tail++;
	if (wcmdq_tail >= N_WCMDQ) wcmdq_tail = 0;
}

#define PEAKSHAPEW 256

static const unsigned char pk_shape1[PEAKSHAPEW+1] = {
	255, 254, 254, 254, 254, 254, 253, 253, 252, 251, 251, 250, 249, 248, 247, 246,
	245, 244, 242, 241, 239, 238, 236, 234, 233, 231, 229, 227, 225, 223, 220, 218,
	216, 213, 211, 209, 207, 205, 203, 201, 199, 197, 195, 193, 191, 189, 187, 185,
	183, 180, 178, 176, 173, 171, 169, 166, 164, 161, 159, 156, 154, 151, 148, 146,
	143, 140, 138, 135, 132, 129, 126, 123, 120, 118, 115, 112, 108, 105, 102,  99,
	 96,  95,  93,  91,  90,  88,  86,  85,  83,  82,  80,  79,  77,  76,  74,  73,
	 72,  70,  69,  68,  67,  66,  64,  63,  62,  61,  60,  59,  58,  57,  56,  55,
	 55,  54,  53,  52,  52,  51,  50,  50,  49,  48,  48,  47,  47,  46,  46,  46,
	 45,  45,  45,  44,  44,  44,  44,  44,  44,  44,  43,  43,  43,  43,  44,  43,
	 42,  42,  41,  40,  40,  39,  38,  38,  37,  36,  36,  35,  35,  34,  33,  33,
	 32,  32,  31,  30,  30,  29,  29,  28,  28,  27,  26,  26,  25,  25,  24,  24,
	 23,  23,  22,  22,  21,  21,  20,  20,  19,  19,  18,  18,  18,  17,  17,  16,
	 16,  15,  15,  15,  14,  14,  13,  13,  13,  12,  12,  11,  11,  11,  10,  10,
	 10,   9,   9,   9,   8,   8,   8,   7,   7,   7,   7,   6,   6,   6,   5,   5,
	  5,   5,   4,   4,   4,   4,   4,   3,   3,   3,   3,   2,   2,   2,   2,   2,
	  2,   1,   1,   1,   1,   1,   1,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	  0
};

static const unsigned char pk_shape2[PEAKSHAPEW+1] = {
	255, 254, 254, 254, 254, 254, 254, 254, 254, 254, 253, 253, 253, 253, 252, 252,
	252, 251, 251, 251, 250, 250, 249, 249, 248, 248, 247, 247, 246, 245, 245, 244,
	243, 243, 242, 241, 239, 237, 235, 233, 231, 229, 227, 225, 223, 221, 218, 216,
	213, 211, 208, 205, 203, 200, 197, 194, 191, 187, 184, 181, 178, 174, 171, 167,
	163, 160, 156, 152, 148, 144, 140, 136, 132, 127, 123, 119, 114, 110, 105, 100,
	 96,  94,  91,  88,  86,  83,  81,  78,  76,  74,  71,  69,  66,  64,  62,  60,
	 57,  55,  53,  51,  49,  47,  44,  42,  40,  38,  36,  34,  32,  30,  29,  27,
	 25,  23,  21,  19,  18,  16,  14,  12,  11,   9,   7,   6,   4,   3,   1,   0,
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	  0
};

static const unsigned char *pk_shape;

void WavegenInit(int rate, int wavemult_fact)
{
	int ix;
	double x;

	if (wavemult_fact == 0)
		wavemult_fact = 60; // default

	wvoice = NULL;
	samplerate = rate;
	PHASE_INC_FACTOR = 0x8000000 / samplerate; // assumes pitch is Hz*32
	samplecount = 0;
	nsamples = 0;
	wavephase = 0x7fffffff;

	wdata.amplitude = 32;
	wdata.amplitude_fmt = 100;

	for (ix = 0; ix < N_EMBEDDED_VALUES; ix++)
		embedded_value[ix] = embedded_default[ix];

	// set up window to generate a spread of harmonics from a
	// single peak for HF peaks
	wavemult_max = (samplerate * wavemult_fact)/(256 * 50);
	if (wavemult_max > N_WAVEMULT) wavemult_max = N_WAVEMULT;

	wavemult_offset = wavemult_max/2;

	if (samplerate != 22050) {
		// wavemult table has preset values for 22050 Hz, we only need to
		// recalculate them if we have a different sample rate
		for (ix = 0; ix < wavemult_max; ix++) {
			x = 127*(1.0 - cos((M_PI*2)*ix/wavemult_max));
			wavemult[ix] = (int)x;
		}
	}

	pk_shape = pk_shape2;
}

int GetAmplitude(void)
{
	int amp;

	// normal, none, reduced, moderate, strong
	static const unsigned char amp_emphasis[5] = { 16, 16, 10, 16, 22 };

	amp = (embedded_value[EMBED_A])*55/100;
	general_amplitude = amp * amp_emphasis[embedded_value[EMBED_F]] / 16;
	return general_amplitude;
}

static void WavegenSetEcho(void)
{
	if (wvoice == NULL)
		return;

	int delay;
	int amp;

	voicing = wvoice->voicing;
	delay = wvoice->echo_delay;
	amp = wvoice->echo_amp;

	if (delay >= N_ECHO_BUF)
		delay = N_ECHO_BUF-1;
	if (amp > 100)
		amp = 100;

	memset(echo_buf, 0, sizeof(echo_buf));
	echo_tail = 0;

	if (embedded_value[EMBED_H] > 0) {
		// set echo from an embedded command in the text
		amp = embedded_value[EMBED_H];
		delay = 130;
	}

	if (delay == 0)
		amp = 0;

	echo_head = (delay * samplerate)/1000;
	echo_length = echo_head; // ensure completion of echo at the end of speech. Use 1 delay period?
	if (amp == 0)
		echo_length = 0;
	if (amp > 20)
		echo_length = echo_head * 2; // perhaps allow 2 echo periods if the echo is loud.

	// echo_amp units are 1/256ths of the amplitude of the original sound.
	echo_amp = amp;
	// compensate (partially) for increase in amplitude due to echo
	general_amplitude = GetAmplitude();
	general_amplitude = ((general_amplitude * (500-amp))/500);
}

int PeaksToHarmspect(wavegen_peaks_t *peaks, int pitch, int *htab, int control)
{
	if (wvoice == NULL)
		return 1;

	// Calculate the amplitude of each  harmonics from the formants
	// Only for formants 0 to 5

	// control 0=initial call, 1=every 64 cycles

	// pitch and freqs are Hz<<16

	int f;
	wavegen_peaks_t *p;
	int fp;  // centre freq of peak
	int fhi; // high freq of peak
	int h;   // harmonic number
	int pk;
	int hmax;
	int hmax_samplerate; // highest harmonic allowed for the samplerate
	int x;
	int h1;

	// initialise as much of *out as we will need
	hmax = (peaks[wvoice->n_harmonic_peaks].freq + peaks[wvoice->n_harmonic_peaks].right)/pitch;
	if (hmax >= MAX_HARMONIC)
		hmax = MAX_HARMONIC-1;

	// restrict highest harmonic to half the samplerate
	hmax_samplerate = (((samplerate * 19)/40) << 16)/pitch; // only 95% of Nyquist freq

	if (hmax > hmax_samplerate)
		hmax = hmax_samplerate;

	for (h = 0; h <= hmax; h++)
		htab[h] = 0;

	for (pk = 0; pk <= wvoice->n_harmonic_peaks; pk++) {
		p = &peaks[pk];
		if ((p->height == 0) || (fp = p->freq) == 0)
			continue;

		fhi = p->freq + p->right;
		h = ((p->freq - p->left) / pitch) + 1;
		if (h <= 0) h = 1;

		for (f = pitch*h; f < fp; f += pitch)
			htab[h++] += pk_shape[(fp-f)/(p->left>>8)] * p->height;
		for (; f < fhi; f += pitch)
			htab[h++] += pk_shape[(f-fp)/(p->right>>8)] * p->height;
	}

	int y;
	int h2;
	// increase bass
	y = peaks[1].height * 10; // addition as a multiple of 1/256s
	h2 = (1000<<16)/pitch; // decrease until 1000Hz
	if (h2 > 0) {
		x = y/h2;
		h = 1;
		while (y > 0) {
			htab[h++] += y;
			y -= x;
		}
	}

	// find the nearest harmonic for HF peaks where we don't use shape
	for (; pk < N_PEAKS; pk++) {
		x = peaks[pk].height >> 14;
		peak_height[pk] = (x * x * 5)/2;

		// find the nearest harmonic for HF peaks where we don't use shape
		if (control == 0) {
			// set this initially, but make changes only at the quiet point
			peak_harmonic[pk] = peaks[pk].freq / pitch;
		}
		// only use harmonics up to half the samplerate
		if (peak_harmonic[pk] >= hmax_samplerate)
			peak_height[pk] = 0;
	}

	// convert from the square-rooted values
	f = 0;
	for (h = 0; h <= hmax; h++, f += pitch) {
		x = htab[h] >> 15;
		htab[h] = (x * x) >> 8;

		int ix;
		if ((ix = (f >> 19)) < N_TONE_ADJUST)
			htab[h] = (htab[h] * wvoice->tone_adjust[ix]) >> 13; // index tone_adjust with Hz/8
	}

	// adjust the amplitude of the first harmonic, affects tonal quality
	h1 = htab[1] * option_harmonic1;
	htab[1] = h1/8;

	// calc intermediate increments of LF harmonics
	if (control & 1) {
		for (h = 1; h < N_LOWHARM; h++)
			harm_inc[h] = (htab[h] - harmspect[h]) >> 3;
	}

	return hmax; // highest harmonic number
}

static void setresonator(RESONATOR *rp, int freq, int bwidth, int init)
{
	// freq    Frequency of resonator in Hz
	// bwidth  Bandwidth of resonator in Hz
	// init    Initialize internal data

	double x;
	double arg;

	if (init) {
		rp->x1 = 0;
		rp->x2 = 0;
	}

	arg = minus_pi_t * bwidth;
	x = exp(arg);

	rp->c = -(x * x);

	arg = two_pi_t * freq;
	rp->b = x * cos(arg) * 2.0;

	rp->a = 1.0 - rp->b - rp->c;
}

void InitBreath(void)
{
	int ix;

	minus_pi_t = -M_PI / samplerate;
	two_pi_t = -2.0 * minus_pi_t;

	for (ix = 0; ix < N_PEAKS; ix++)
		setresonator(&rbreath[ix], 2000, 200, 1);
}

static void SetPitchFormants(void)
{
	if (wvoice == NULL)
		return;

	int ix;
	int factor = 256;
	int pitch_value;

	// adjust formants to give better results for a different voice pitch
	if ((pitch_value = embedded_value[EMBED_P]) > MAX_PITCH_VALUE)
		pitch_value = MAX_PITCH_VALUE;

	if (pitch_value > 50) {
		// only adjust if the pitch is higher than normal
		factor = 256 + (25 * (pitch_value - 50))/50;
	}

	for (ix = 0; ix <= 5; ix++)
		wvoice->freq[ix] = (wvoice->freq2[ix] * factor)/256;

	factor = embedded_value[EMBED_T]*3;
	wvoice->height[0] = (wvoice->height2[0] * (256 - factor*2))/256;
	wvoice->height[1] = (wvoice->height2[1] * (256 - factor))/256;
}

void WavegenSetVoice(voice_t *v)
{
	static voice_t v2;

	memcpy(&v2, v, sizeof(v2));
	wvoice = &v2;

	if (v->peak_shape == 0)
		pk_shape = pk_shape1;
	else
		pk_shape = pk_shape2;

	consonant_amp = (v->consonant_amp * 26) /100;
	if (samplerate <= 11000) {
		consonant_amp = consonant_amp*2; // emphasize consonants at low sample rates
		option_harmonic1 = 6;
	}
	WavegenSetEcho();
	SetPitchFormants();
}

void Write4Bytes(FILE *f, int value)
{
	// Write 4 bytes to a file, least significant first
	int ix;

	for (ix = 0; ix < 4; ix++) {
		fputc(value & 0xff, f);
		value = value >> 8;
	}
}

#pragma GCC visibility push(default)

ESPEAK_NG_API espeak_ng_STATUS
espeak_ng_SetOutputHooks(espeak_ng_OUTPUT_HOOKS* hooks)
{
	output_hooks = hooks;
	return 0;
}

ESPEAK_NG_API espeak_ng_STATUS
espeak_ng_SetConstF0(int f0)
{
	const_f0 = f0;
	return ENS_OK;
}

#pragma GCC visibility pop
