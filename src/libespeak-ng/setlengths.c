/*
 * Copyright (C) 2005 to 2011 by Jonathan Duddington
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

#include "config.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <espeak-ng/espeak_ng.h>
#include <espeak-ng/speak_lib.h>
#include <espeak-ng/encoding.h>

#include "readclause.h"
#include "setlengths.h"
#include "synthdata.h"
#include "wavegen.h"

#include "phoneme.h"
#include "voice.h"
#include "synthesize.h"
#include "translate.h"

static void SetSpeedFactors(voice_t *voice, int x, int speeds[3]);
static void SetSpeedMods(SPEED_FACTORS *speed, int voiceSpeedF1, int wpm, int x);
static void SetSpeedMultiplier(int *x, int *wpm);

extern int saved_parameters[];

// convert from words-per-minute to internal speed factor
// Use this to calibrate speed for wpm 80-450 (espeakRATE_MINIMUM - espeakRATE_MAXIMUM)
static const unsigned char speed_lookup[] = {
	255, 255, 255, 255, 255, //  80
	253, 249, 245, 242, 238, //  85
	235, 232, 228, 225, 222, //  90
	218, 216, 213, 210, 207, //  95
	204, 201, 198, 196, 193, // 100
	191, 188, 186, 183, 181, // 105
	179, 176, 174, 172, 169, // 110
	168, 165, 163, 161, 159, // 115
	158, 155, 153, 152, 150, // 120
	148, 146, 145, 143, 141, // 125
	139, 137, 136, 135, 133, // 130
	131, 130, 129, 127, 126, // 135
	124, 123, 122, 120, 119, // 140
	118, 117, 115, 114, 113, // 145
	112, 111, 110, 109, 107, // 150
	106, 105, 104, 103, 102, // 155
	101, 100,  99,  98,  97, // 160
	 96,  95,  94,  93,  92, // 165
	 91,  90,  89,  89,  88, // 170
	 87,  86,  85,  84,  83, // 175
	 82,  82,  81,  80,  80, // 180
	 79,  78,  77,  76,  76, // 185
	 75,  75,  74,  73,  72, // 190
	 71,  71,  70,  69,  69, // 195
	 68,  67,  67,  66,  66, // 200
	 65,  64,  64,  63,  62, // 205
	 62,  61,  61,  60,  59, // 210
	 59,  58,  58,  57,  57, // 215
	 56,  56,  55,  54,  54, // 220
	 53,  53,  52,  52,  52, // 225
	 51,  50,  50,  49,  49, // 230
	 48,  48,  47,  47,  46, // 235
	 46,  46,  45,  45,  44, // 240
	 44,  44,  43,  43,  42, // 245
	 41,  40,  40,  40,  39, // 250
	 39,  39,  38,  38,  38, // 255
	 37,  37,  37,  36,  36, // 260
	 35,  35,  35,  35,  34, // 265
	 34,  34,  33,  33,  33, // 270
	 32,  32,  31,  31,  31, // 275
	 30,  30,  30,  29,  29, // 280
	 29,  29,  28,  28,  27, // 285
	 27,  27,  27,  26,  26, // 290
	 26,  26,  25,  25,  25, // 295
	 24,  24,  24,  24,  23, // 300
	 23,  23,  23,  22,  22, // 305
	 22,  21,  21,  21,  21, // 310
	 20,  20,  20,  20,  19, // 315
	 19,  19,  18,  18,  17, // 320
	 17,  17,  16,  16,  16, // 325
	 16,  16,  16,  15,  15, // 330
	 15,  15,  14,  14,  14, // 335
	 13,  13,  13,  12,  12, // 340
	 12,  12,  11,  11,  11, // 345
	 11,  10,  10,  10,   9, // 350
	  9,   9,   8,   8,   8, // 355
};

// speed_factor1 adjustments for speeds 350 to 374: pauses
static const unsigned char pause_factor_350[] = {
	22, 22, 22, 22, 22, 22, 22, 21, 21, 21, // 350
	21, 20, 20, 19, 19, 18, 17, 16, 15, 15, // 360
	15, 15, 15, 15, 15                      // 370
};

// wav_factor adjustments for speeds 350 to 450
// Use this to calibrate speed for wpm 350-450
static const unsigned char wav_factor_350[] = {
	120, 121, 120, 119, 119, // 350
	118, 118, 117, 116, 116, // 355
	115, 114, 113, 112, 112, // 360
	111, 111, 110, 109, 108, // 365
	107, 106, 106, 104, 103, // 370
	103, 102, 102, 102, 101, // 375
	101,  99,  98,  98,  97, // 380
	 96,  96,  95,  94,  93, // 385
	 91,  90,  91,  90,  89, // 390
	 88,  86,  85,  86,  85, // 395
	 85,  84,  82,  81,  80, // 400
	 79,  77,  78,  78,  76, // 405
	 77,  75,  75,  74,  73, // 410
	 71,  72,  70,  69,  69, // 415
	 69,  67,  65,  64,  63, // 420
	 63,  63,  61,  61,  59, // 425
	 59,  59,  58,  56,  57, // 430
	 58,  56,  54,  53,  52, // 435
	 52,  53,  52,  52,  50, // 440
	 48,  47,  47,  45,  46, // 445
	 45                      // 450
};

static int len_speeds[3] = { 130, 121, 118 };

void SetSpeed(int control)
{
	int x;
	int wpm;

	speed.min_sample_len = espeakRATE_MAXIMUM;
	speed.lenmod_factor = 110; // controls the effect of FRFLAG_LEN_MOD reduce length change
	speed.lenmod2_factor = 100;

	wpm = embedded_value[EMBED_S];
	if (control == 2)
		wpm = embedded_value[EMBED_S2];

	speed.min_pause = 5;

	if (voice->speed_percent > 0)
		wpm = (wpm * voice->speed_percent)/100;

	SetSpeedMultiplier(&x, &wpm);

	if (control & 1) {
		SetSpeedFactors(voice, x, len_speeds);
	}

	if (control & 2) {
		SetSpeedMods(&speed, voice->speedf1, wpm, x);
	}
}

static void SetSpeedMultiplier(int *x, int *wpm) {
	int wpm2;

	if (*wpm > espeakRATE_MAXIMUM)
		*wpm = espeakRATE_MAXIMUM;

	wpm2 = *wpm;
	if (*wpm > 359) wpm2 = 359;
	if (*wpm < espeakRATE_MINIMUM) {
		wpm2 = espeakRATE_MINIMUM;
	}

	*x = speed_lookup[wpm2-espeakRATE_MINIMUM];

	if (*wpm >= 380)
		*x = 7;
	if (*wpm >= 400)
		*x = 6;
}

static void SetSpeedFactors(voice_t *voice, int x, int speeds[3]) {
	// set speed factors for different syllable positions within a word
	// these are used in CalcLengths()
	speeds[0] = (x * voice->speedf1)/256;
	speeds[1] = (x * voice->speedf2)/256;
	speeds[2] = (x * voice->speedf3)/256;

	if (x <= 7) {
		speeds[0] = x;
		speeds[1] = speeds[2] = x - 1;
	}
}

static void SetSpeedMods(SPEED_FACTORS *speed, int voiceSpeedF1, int wpm, int x) {
	// these are used in synthesis file

	if (wpm > 350) {
		speed->lenmod_factor = 85 - (wpm - 350) / 3;
		speed->lenmod2_factor = 60 - (wpm - 350) / 8;
	} else if (wpm > 250) {
		speed->lenmod_factor = 110 - (wpm - 250)/4;
		speed->lenmod2_factor = 110 - (wpm - 250)/2;
	}


	int s1 = (x * voiceSpeedF1)/256;

	if (wpm >= 170)
		speed->wav_factor = 110 + (150*s1)/128; // reduced speed adjustment, used for playing recorded sounds
	else
		speed->wav_factor = 128 + (128*s1)/130; // = 215 at 170 wpm

	if (wpm >= 350)
		speed->wav_factor = wav_factor_350[wpm-350];

	if (wpm >= 390) {
		speed->min_sample_len = espeakRATE_MAXIMUM - (wpm - 400)/2;
		if (wpm > 440)
			speed->min_sample_len = 420 - (wpm - 440);
	}

	speed->pause_factor = (256 * s1)/115; // full speed adjustment, used for pause length
	speed->clause_pause_factor = 0;

	if (wpm > 430)
		speed->pause_factor = 12;
	else if (wpm > 400)
		speed->pause_factor = 13;
	else if (wpm > 374)
		speed->pause_factor = 14;
	else if (wpm > 350)
		speed->pause_factor = pause_factor_350[wpm - 350];

	if (speed->clause_pause_factor == 0) {
		// restrict the reduction of pauses between clauses
		if ((speed->clause_pause_factor = speed->pause_factor) < 16)
			speed->clause_pause_factor = 16;
	}
}

espeak_ng_STATUS SetParameter(int parameter, int value, int relative)
{
	// parameter: reset-all, amp, pitch, speed, linelength, expression, capitals, number grouping
	// relative 0=absolute  1=relative

	int new_value = value;
	extern const int param_defaults[N_SPEECH_PARAM];

	if (relative) {
		if (parameter < 5) {
			int default_value;
			default_value = param_defaults[parameter];
			new_value = default_value + (default_value * value)/100;
		}
	}
	param_stack[0].parameter[parameter] = new_value;
	saved_parameters[parameter] = new_value;

	switch (parameter)
	{
	case espeakRATE:
		embedded_value[EMBED_S] = new_value;
		embedded_value[EMBED_S2] = new_value;
		SetSpeed(3);
		break;
	case espeakVOLUME:
		embedded_value[EMBED_A] = new_value;
		GetAmplitude();
		break;
	case espeakPITCH:
		if (new_value > 99) new_value = 99;
		if (new_value < 0) new_value = 0;
		embedded_value[EMBED_P] = new_value;
		break;
	case espeakRANGE:
		if (new_value > 99) new_value = 99;
		embedded_value[EMBED_R] = new_value;
		break;
	case espeakLINELENGTH:
		option_linelength = new_value;
		break;
	case espeakWORDGAP:
		option_wordgap = new_value;
		break;
	case espeakINTONATION:
		if ((new_value & 0xff) != 0)
			translator->langopts.intonation_group = new_value & 0xff;
		option_tone_flags = new_value;
		break;
  case espeakSSML_BREAK_MUL:
    break;
	default:
		return EINVAL;
	}
	return ENS_OK;
}

// Tables of the relative lengths of vowels, depending on the
// type of the two phonemes that follow
// indexes are the "length_mod" value for the following phonemes

// use this table if vowel is not the last in the word
static const unsigned char length_mods_en[LENGTH_MOD_LIMIT * LENGTH_MOD_LIMIT] = {
//	a    ,    t    s    n    d    z    r    N    <- next
	100, 120, 100, 105, 100, 110, 110, 100,  95, 100, // a  <- next2
	105, 120, 105, 110, 125, 130, 135, 115, 125, 100, // ,
	105, 120,  75, 100,  75, 105, 120,  85,  75, 100, // t
	105, 120,  85, 105,  95, 115, 120, 100,  95, 100, // s
	110, 120,  95, 105, 100, 115, 120, 100, 100, 100, // n
	105, 120, 100, 105,  95, 115, 120, 110,  95, 100, // d
	105, 120, 100, 105, 105, 122, 125, 110, 105, 100, // z
	105, 120, 100, 105, 105, 122, 125, 110, 105, 100, // r
	105, 120,  95, 105, 100, 115, 120, 110, 100, 100, // N
	100, 120, 100, 100, 100, 100, 100, 100, 100, 100
};

// as above, but for the last syllable in a word
static const unsigned char length_mods_en0[LENGTH_MOD_LIMIT * LENGTH_MOD_LIMIT] = {
//	a    ,    t    s    n    d    z    r    N    <- next
	100, 150, 100, 105, 110, 115, 110, 110, 110, 100, // a  <- next2
	105, 150, 105, 110, 125, 135, 140, 115, 135, 100, // ,
	105, 150,  90, 105,  90, 122, 135, 100,  90, 100, // t
	105, 150, 100, 105, 100, 122, 135, 100, 100, 100, // s
	105, 150, 100, 105, 105, 115, 135, 110, 105, 100, // n
	105, 150, 100, 105, 105, 122, 130, 120, 125, 100, // d
	105, 150, 100, 105, 110, 122, 125, 115, 110, 100, // z
	105, 150, 100, 105, 105, 122, 135, 120, 105, 100, // r
	105, 150, 100, 105, 105, 115, 135, 110, 105, 100, // N
	100, 100, 100, 100, 100, 100, 100, 100, 100, 100
};


static const unsigned char length_mods_equal[LENGTH_MOD_LIMIT * LENGTH_MOD_LIMIT] = {
//	a    ,    t    s    n    d    z    r    N    <- next
	110, 120, 100, 110, 110, 110, 110, 110, 110, 110, // a  <- next2
	110, 120, 100, 110, 110, 110, 110, 110, 110, 110, // ,
	110, 120, 100, 110, 100, 110, 110, 110, 100, 110, // t
	110, 120, 100, 110, 110, 110, 110, 110, 110, 110, // s
	110, 120, 100, 110, 110, 110, 110, 110, 110, 110, // n
	110, 120, 100, 110, 110, 110, 110, 110, 110, 110, // d
	110, 120, 100, 110, 110, 110, 110, 110, 110, 110, // z
	110, 120, 100, 110, 110, 110, 110, 110, 110, 110, // r
	110, 120, 100, 110, 110, 110, 110, 110, 110, 110, // N
	110, 120, 100, 110, 110, 110, 110, 110, 110, 110
};

static const unsigned char *const length_mod_tabs[6] = {
	length_mods_en,
	length_mods_en,    // 1
	length_mods_en0,   // 2
	length_mods_equal, // 3
	length_mods_equal, // 4
	length_mods_equal  // 5
};

void SetLengthMods(Translator *tr, int value)
{
	int value2;

	tr->langopts.length_mods0 = tr->langopts.length_mods = length_mod_tabs[value % 100];
	if ((value2 = value / (LENGTH_MOD_LIMIT * LENGTH_MOD_LIMIT)) != 0)
		tr->langopts.length_mods0 = length_mod_tabs[value2];
}
