#include <espeak-ng/speak_lib.h>
#include <string.h>

static int initialized = 0;
static char current_data_path[64] = {0};
static char current_voice_path[40] = {0};

// Returns 1=success 0=failure
int wasm_InitForTextToIpa(const char* data_path, const char* voice_path) {
    if (initialized) {
        if (strcmp(current_data_path, data_path) == 0)
            return 1;  // Already initialized with path

        initialized = 0;
        espeak_TerminateForTextToIpa();
    }

    if (espeak_InitForTextToIpa(voice_path, data_path) != 0)
        return 0;  // Failed to init

    strncpy(current_data_path, data_path, sizeof(current_data_path) - 1);
    strncpy(current_voice_path, voice_path, sizeof(current_voice_path) - 1);
    initialized = 1;
    return 1;  // Initialized with path and voice
}

// Returns 1=success 0=failure
int wasm_LoadVoiceForTextToIpa(const char* data_path, const char* voice_path) {
    if (!wasm_InitForTextToIpa(data_path, voice_path))
        return 0;  // Failed to init

    if (strcmp(current_voice_path, voice_path) == 0)
        return 1;  // Voice is already set

    if (espeak_LoadVoiceForTextToIpa(voice_path) != 0)
        return 0;  // Failed to load voice

    strncpy(current_voice_path, voice_path, sizeof(current_voice_path) - 1);
    return 1;  // Voice was loaded
}

char* wasm_TextToIpaWithSourceMap(const char* text, int* out_source_map, int map_capacity) {
    if (!initialized)
        return NULL;  // Caller must first load a voice

    const char* textptr = text;
    return espeak_TextToIpaWithSourceMap(&textptr, out_source_map, map_capacity);
}
