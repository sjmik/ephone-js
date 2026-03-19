/*
 * Copyright (C) 2005 to 2013 by Jonathan Duddington
 * email: jonsd@users.sourceforge.net
 * Copyright (C) 2013-2017 Reece H. Dunn
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

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#include <espeak-ng/espeak_ng.h>
#include <espeak-ng/speak_lib.h>
#include <espeak-ng/encoding.h>

#include "speech.h"
#include "common.h"               // for GetFileLength
#include "dictionary.h"           // for GetTranslatedPhonemeString, strncpy0
#include "setlengths.h"           // for SetParameter
#include "langopts.h"             // for LoadConfig
#include "readclause.h"           // for PARAM_STACK, param_stack
#include "synthdata.h"            // for FreePhData, LoadPhData
#include "translate.h"            // for p_decoder, InitText, translator
#include "voice.h"                // for FreeVoiceList, VoiceReset, current_...
#include "wavegen.h"              // for WavegenFill, WavegenInit

static int voice_samplerate = 22050;
static espeak_ng_STATUS err = ENS_OK;

char path_home[N_PATH_HOME]; // this is the espeak-ng-data directory
extern int saved_parameters[N_SPEECH_PARAM]; // Parameters saved on synthesis start

static int check_data_path(const char *path, int allow_directory)
{
	if (!path) return 0;

	snprintf(path_home, sizeof(path_home), "%s/espeak-ng-data", path);
	if (GetFileLength(path_home) == -EISDIR)
		return 1;

	if (!allow_directory)
		return 0;

	snprintf(path_home, sizeof(path_home), "%s", path);
	return GetFileLength(path_home) == -EISDIR;
}

#pragma GCC visibility push(default)

ESPEAK_NG_API espeak_ng_STATUS espeak_ng_InitializeOutput(espeak_ng_OUTPUT_MODE output_mode, int buffer_length, const char *device)
{
	(void)output_mode;
	(void)buffer_length;
	(void)device; // unused if  USE_LIBPCAUDIO is not defined
	return ENS_OK;
}


ESPEAK_NG_API void espeak_ng_InitializePath(const char *path)
{
	if (check_data_path(path, 1))
		return;

#if !defined(PLATFORM_DOS)
	if (check_data_path(getenv("ESPEAK_DATA_PATH"), 1))
		return;

	if (check_data_path(getenv("HOME"), 0))
		return;
#endif

	strcpy(path_home, PATH_ESPEAK_DATA);
}

const int param_defaults[N_SPEECH_PARAM] = {
	0,   // silence (internal use)
	espeakRATE_NORMAL, // rate wpm
	100, // volume
	50,  // pitch
	50,  // range
	0,   // punctuation
	0,   // capital letters
	0,   // wordgap
	0,   // options
	0,   // intonation
	100, // ssml break mul
	0,
	0,   // emphasis
	0,   // line length
	0,   // voice type
};


ESPEAK_NG_API espeak_ng_STATUS espeak_ng_Initialize(espeak_ng_ERROR_CONTEXT *context)
{
	int param;
	int srate = 22050; // default sample rate 22050 Hz

	// It seems that the wctype functions don't work until the locale has been set
	// to something other than the default "C".  Then, not only Latin1 but also the
	// other characters give the correct results with iswalpha() etc.
	if (setlocale(LC_CTYPE, "C.UTF-8") == NULL) {
		if (setlocale(LC_CTYPE, "UTF-8") == NULL) {
			if (setlocale(LC_CTYPE, "en_US.UTF-8") == NULL)
				setlocale(LC_CTYPE, "");
		}
	}

	espeak_ng_STATUS result = LoadPhData(NULL, context);
	if (result != ENS_OK)
		return result;

	WavegenInit(srate, 0);
	LoadConfig();

	espeak_VOICE *current_voice_selected = espeak_GetCurrentVoice();
	memset(current_voice_selected, 0, sizeof(espeak_VOICE));
	SetVoiceStack(NULL, "");
	InitNamedata();

	VoiceReset(0);

	for (param = 0; param < N_SPEECH_PARAM; param++)
		param_stack[0].parameter[param] = saved_parameters[param] = param_defaults[param];

	SetParameter(espeakRATE, espeakRATE_NORMAL, 0);
	SetParameter(espeakVOLUME, 100, 0);
	SetParameter(espeakCAPITALS, option_capitals, 0);
	SetParameter(espeakPUNCTUATION, option_punctuation, 0);
	SetParameter(espeakWORDGAP, 0, 0);

	option_phonemes = 0;
	option_phoneme_events = 0;

	// Seed random generator
	espeak_srand(time(NULL));

	return ENS_OK;
}

ESPEAK_NG_API espeak_ng_STATUS espeak_ng_SetPhonemeEvents(int enable, int ipa) {
	option_phoneme_events = 0;
	if (enable) {
		option_phoneme_events |= espeakINITIALIZE_PHONEME_EVENTS;
		if (ipa) {
			option_phoneme_events |= espeakINITIALIZE_PHONEME_IPA;
		}
	}
	return ENS_OK;
}

ESPEAK_NG_API int espeak_ng_GetSampleRate(void)
{
	return samplerate;
}

#pragma GCC visibility pop

void sync_espeak_SetPunctuationList(const wchar_t *punctlist)
{
	// Set the list of punctuation which are spoken for "some".

	option_punctlist[0] = 0;
	if (punctlist != NULL) {
		wcsncpy(option_punctlist, punctlist, N_PUNCTLIST);
		option_punctlist[N_PUNCTLIST-1] = 0;
	}
}

#pragma GCC visibility push(default)

ESPEAK_API int espeak_GetParameter(espeak_PARAMETER parameter, int current)
{
	// current: 0=default value, 1=current value
	if (current)
		return param_stack[0].parameter[parameter];
	return param_defaults[parameter];
}

ESPEAK_NG_API espeak_ng_STATUS espeak_ng_SetParameter(espeak_PARAMETER parameter, int value, int relative)
{
	return SetParameter(parameter, value, relative);
}

ESPEAK_NG_API espeak_ng_STATUS espeak_ng_SetPunctuationList(const wchar_t *punctlist)
{
	// Set the list of punctuation which are spoken for "some".
	sync_espeak_SetPunctuationList(punctlist);
	return ENS_OK;
}

ESPEAK_API void espeak_SetPhonemeTrace(int phonememode, FILE *stream)
{
	/* phonememode:  Controls the output of phoneme symbols for the text
	      bits 0-2:
	         value=0  No phoneme output (default)
	         value=1  Output the translated phoneme symbols for the text
	         value=2  as (1), but produces IPA phoneme names rather than ascii
	      bit 3:   output a trace of how the translation was done (showing the matching rules and list entries)
	      bit 4:   produce pho data for mbrola
	      bit 7:   use (bits 8-23) as a tie within multi-letter phonemes names
	      bits 8-23:  separator character, between phoneme names

	   stream   output stream for the phoneme symbols (and trace).  If stream=NULL then it uses stdout.
	*/

	option_phonemes = phonememode;
	f_trans = stream;
	if (stream == NULL)
		f_trans = stderr;
}

ESPEAK_API const char* espeak_TextToPhonemesWithTerminator(const void** textptr, int textmode, int phonememode, int* terminator)
{
	/* phoneme_mode
	    bit 1:   0=eSpeak's ascii phoneme names, 1= International Phonetic Alphabet (as UTF-8 characters).
	    bit 7:   use (bits 8-23) as a tie within multi-letter phonemes names
	    bits 8-23:  separator character, between phoneme names
	 */

	if (p_decoder == NULL)
		p_decoder = create_text_decoder();

	if (text_decoder_decode_string_multibyte(p_decoder, *textptr, translator->encoding, textmode) != ENS_OK)
		return NULL;

	TranslateClauseWithTerminator(translator, NULL, NULL, terminator);
	*textptr = text_decoder_get_buffer(p_decoder);

	return GetTranslatedPhonemeString(phonememode, NULL);
}

ESPEAK_API const char *espeak_TextToPhonemes(const void **textptr, int textmode, int phonememode)
{
	return espeak_TextToPhonemesWithTerminator(textptr, textmode, phonememode, NULL);
}

static int utf8_count(const char* str, const char* until) {
    int count = 0;
    while (str < until) {
        if ((*str & 0xC0) != 0x80) count++;  // Not a continuation byte
        str++;
    }
    return count;
}

// Recommend source map capacity to be at least twice the number of words in the input.
ESPEAK_API char* espeak_TextToIpaWithSourceMap(const char** textptr, int* out_source_map, int map_capacity) {
    InitText(0);
    if (!p_decoder) p_decoder = create_text_decoder();

	int utf8_consumed_len = 0;
    int result_len = 0;
    int result_capacity = 512;
    char* result = malloc(result_capacity);
    if (!result) return NULL;
    result[0] = '\0';

    int out_map_index = 0;
    short source_map[N_CLAUSE_WORDS * 2];

    while (*textptr && **textptr != '\0') {
        if (text_decoder_decode_string(p_decoder, *textptr, -1, ESPEAKNG_ENCODING_UTF_8) != ENS_OK) break;

        int terminator = CLAUSE_INTONATION_NONE;
        TranslateClauseWithTerminator(translator, NULL, NULL, &terminator);
        const char* decoder_current = text_decoder_get_buffer(p_decoder);
        if (decoder_current == *textptr) break;
        *textptr = decoder_current;
		
        const char* phonemes = GetTranslatedPhonemeString(espeakPHONEMES_IPA, source_map);
        if (!phonemes || phonemes[0] == '\0') break;
        if (result_len > 0) result[result_len++] = ' ';
        const int utf8_result_len = utf8_count(result, result + result_len);

        int i = 0;
        while (i < N_CLAUSE_WORDS * 2) {
            const short source_pos = source_map[i++];
            const short phoneme_pos = source_map[i++];
            if (source_pos < 0 || phoneme_pos < 0) break;
            if (map_capacity < out_map_index + 2) {
                if (out_map_index < map_capacity) out_source_map[out_map_index] = -1;
                break;
            }
            out_source_map[out_map_index++] = source_pos + utf8_consumed_len;
            out_source_map[out_map_index++] = phoneme_pos + utf8_result_len;
        }
        utf8_consumed_len = GetCharacterCount();

        char terminator_char = 0;
        switch (terminator & CLAUSE_INTONATION_TYPE) {
        case CLAUSE_INTONATION_FULL_STOP:
            terminator_char = '.';
            break;
        case CLAUSE_INTONATION_COMMA:
            terminator_char = ',';
            break;
        case CLAUSE_INTONATION_QUESTION:
            terminator_char = '?';
            break;
        case CLAUSE_INTONATION_EXCLAMATION:
            terminator_char = '!';
            break;
        }

        const int phonemes_len = strlen(phonemes);
        // Room for the sentence terminator, possibly a space, and the string terminator.
        while (result_len + phonemes_len + 3 > result_capacity) {
            result_capacity <<= 1;
            char* new_result = realloc(result, result_capacity);
            if (!new_result) {
                free(result);
                return NULL;
            }
            result = new_result;
        }

        memcpy(result + result_len, phonemes, phonemes_len);
        result_len += phonemes_len;
        if (terminator_char) result[result_len++] = terminator_char;
        result[result_len] = '\0';
    }

    if (out_source_map) {
        if (out_map_index < map_capacity)
            out_source_map[out_map_index] = -1;
        else if (map_capacity > 0)
            out_source_map[map_capacity - 1] = -1;
    }
    return result;
}

ESPEAK_API int espeak_InitForTextToIpa(const char* voice_name, const char* path) {
    espeak_ng_InitializePath(path);

    if (setlocale(LC_CTYPE, "C.UTF-8") == NULL)
        if (setlocale(LC_CTYPE, "UTF-8") == NULL)
            if (setlocale(LC_CTYPE, "en_US.UTF-8") == NULL)
                setlocale(LC_CTYPE, "");

    espeak_ng_STATUS result = LoadPhData(NULL, NULL);
    if (result != ENS_OK)
        return result;

    samplerate = voice_samplerate;

    InitNamedata();
    VoiceReset(0);

    for (int param = 0; param < N_SPEECH_PARAM; param++)
        param_stack[0].parameter[param] = saved_parameters[param] = param_defaults[param];

    SetParameter(espeakRATE, espeakRATE_NORMAL, 0);
    SetParameter(espeakVOLUME, 100, 0);
    SetParameter(espeakCAPITALS, option_capitals, 0);
    SetParameter(espeakPUNCTUATION, option_punctuation, 0);
    SetParameter(espeakWORDGAP, 0, 0);

    return espeak_LoadVoiceForTextToIpa(voice_name);
}

ESPEAK_API int espeak_LoadVoiceForTextToIpa(const char* voice_name) {
    espeak_VOICE voice_selector;
    char* variant_name = "\0";

    memset(&voice_selector, 0, sizeof(voice_selector));
    voice_selector.name = (char*)voice_name;

    if (LoadVoice(voice_name, 1) != NULL) {
        DoVoiceChange(voice);
        voice_selector.languages = voice->language_name;
        SetVoiceStack(&voice_selector, variant_name);
        return ENS_OK;
    }
    return ENS_VOICE_NOT_FOUND;
}

ESPEAK_API void espeak_TerminateForTextToIpa(void) {
	espeak_ng_Terminate();
}

ESPEAK_NG_API espeak_ng_STATUS espeak_ng_Cancel(void)
{
	embedded_value[EMBED_T] = 0; // reset echo for pronunciation announcements

	for (int i = 0; i < N_SPEECH_PARAM; i++)
		SetParameter(i, saved_parameters[i], 0);

	return ENS_OK;
}

ESPEAK_API int espeak_IsPlaying(void)
{
	return 0;
}

ESPEAK_NG_API espeak_ng_STATUS espeak_ng_Synchronize(void)
{
	espeak_ng_STATUS berr = err;
	err = ENS_OK;
	return berr;
}

ESPEAK_NG_API espeak_ng_STATUS espeak_ng_Terminate(void)
{
	FreePhData();
	FreeVoiceList();

	DeleteTranslator(translator);
	translator = NULL;

	if (p_decoder != NULL) {
		destroy_text_decoder(p_decoder);
		p_decoder = NULL;
	}

	return ENS_OK;
}

static const char version_string[] = PACKAGE_VERSION;
ESPEAK_API const char *espeak_Info(const char **ptr)
{
	if (ptr != NULL)
		*ptr = path_home;
	return version_string;
}

#pragma GCC visibility pop
