#!/bin/bash
set -e

cd "$(dirname "${BASH_SOURCE[0]}")"
build="./build.sh"

# Most languages use EN as a fallback, so must also include en_dict (assuming it was already compiled).
en_native='../build/native/espeak-ng-data/en_dict'
en_temp='../build/temp/en_dict'

build_lang() {
    if [[ -f "$en_native" ]]; then
        mv "$en_native" "$en_temp"
    fi
    rm -rf '../build/native/espeak-ng-data'
    if [[ -f "$en_temp" ]]; then
        mkdir -p '../build/native/espeak-ng-data'
        cp "$en_temp" "$en_native"
    fi

    "$build" native preload -name "$1" -desc "$2" -dict "$3" -lang "$4"
}

"$build" clean

build_lang 'en-us'  'English - American'      '^en$'            'gmw/en-US$'
build_lang 'en-all' 'English - All (US/GB/+)' '^en$'            'gmw/en(-(US|029|(GB-(x-gbclan|x-rp|scotland|x-gbcwmd))))?$'
build_lang 'roa'    'Romance - ES/FR/IT/PT'   '^(es|fr|it|pt)$' 'roa/(es(-419)?|fr|it|pt(-BR)?)$'
build_lang 'gmw'    'Germanic - DE/NL'        '^(de|nl)$'       'gmw/(de|nl)$'
build_lang 'sit'    'Chinese - CMN/YUE'       '^(cmn|yue)$'     'sit/(cmn|yue)$'
build_lang 'jpx'    'Japanese - JA'           '^ja$'            'jpx/ja$'
build_lang 'zlx'    'Slavic - PL/RU/UK'       '^(pl|ru|uk)$'    'zl[we]/(pl|ru|uk)$'
build_lang 'all'    'All languages'           ''                ''

"$build" wasm ephone

exit 0
