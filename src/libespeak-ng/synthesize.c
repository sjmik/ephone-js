/*
 * Copyright (C) 2005 to 2014 by Jonathan Duddington
 * email: jonsd@users.sourceforge.net
 * Copyright (C) 2015-2017 Reece H. Dunn
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

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <espeak-ng/espeak_ng.h>
#include <espeak-ng/speak_lib.h>
#include <espeak-ng/encoding.h>

#include "synthesize.h"
#include "dictionary.h"           // for WritePhMnemonic, GetTranslatedPhone...
#include "phoneme.h"              // for PHONEME_TAB, phVOWEL, phLIQUID, phN...
#include "setlengths.h"           // for CalcLengths
#include "soundicon.h"               // for soundicon_tab, n_soundicon
#include "synthdata.h"            // for InterpretPhoneme, Inte...
#include "translate.h"            // for translator, LANGUAGE_OPTIONS, Trans...
#include "voice.h"                // for voice_t, voice, LoadVoiceVariant
#include "wavegen.h"              // for WcmdqInc, WcmdqFree, WcmdqStop
#include "speech.h"               // for MAKE_MEM_UNDEFINED

// list of phonemes in a clause
int n_phoneme_list = 0;
PHONEME_LIST phoneme_list[N_PHONEME_LIST+1];

SPEED_FACTORS speed;

static int (*phoneme_callback)(const char *) = NULL;

#define RMS_GLOTTAL1 35   // vowel before glottal stop
#define RMS_START 28  // 28
#define VOWEL_FRONT_LENGTH  50

const char *WordToString(char buf[5], unsigned int word)
{
	// Convert a phoneme mnemonic word into a string
	int ix;

	for (ix = 0; ix < 4; ix++)
		buf[ix] = word >> (ix*8);
	buf[4] = 0;
	return buf;
}

espeak_ng_STATUS DoVoiceChange(voice_t *v)
{
	// allocate memory for a copy of the voice data, and free it in wavegenfill()
	voice_t *v2;
	if ((v2 = (voice_t *)malloc(sizeof(voice_t))) == NULL)
		return ENOMEM;
	memcpy(v2, v, sizeof(voice_t));
	wcmdq[wcmdq_tail][0] = WCMD_VOICE;
	wcmdq[wcmdq_tail][2] = (intptr_t)v2;
	WcmdqInc();
	return ENS_OK;
}

extern espeak_ng_OUTPUT_HOOKS* output_hooks;

#pragma GCC visibility push(default)
ESPEAK_API void espeak_SetPhonemeCallback(int (*PhonemeCallback)(const char *))
{
	phoneme_callback = PhonemeCallback;
}
#pragma GCC visibility pop
