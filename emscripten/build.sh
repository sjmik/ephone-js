#!/bin/bash
set -e

dict_regex='^en$'
lang_regex='gmw/en-US$'
data_regex='/(phonindex|phontab)$'
preload_name='en-us'
preload_desc='English - American'

while [[ $# -gt 0 ]]; do
    case "$1" in
        clean)   do_clean=1 ;;
        native)  do_native=1 ;;
        preload) do_preload=1 ;;
        wasm)    do_wasm=1 ;;
        ephone)  do_ephone=1 ;;
        all)     do_native=1; do_preload=1; do_wasm=1; do_ephone=1 ;;
        -dict)   dict_regex="$2"; shift ;;
        -lang)   lang_regex="$2"; shift ;;
        -name)   preload_name="$2"; shift ;;
        -desc)   preload_desc="$2"; shift ;;
        *)       echo "Unknown parameter: $1"; exit 1 ;;
    esac
    shift
done

init() {
    cd "$(dirname "${BASH_SOURCE[0]}")"
    (( $do_clean )) && rm -rf ../build

    mkdir -p ../build/{native,emscripten,dist/lang,temp}
    cd ../build
}

build_native() {
    cmake -S .. -B native \
        -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_BUILD_TYPE=Release \
        -DDICT_COMPILE_REGEX="$dict_regex" \
        -DLANG_COPY_REGEX="$lang_regex"
    # If compiling more than one dictionary, make can fail when j>1.
    make -C native -j1
}

configure_preload() {
    local lang_meta="const META = { name: '$preload_name', desc: '$preload_desc', voices: ["

    # Only preload files matching one of the regex.
    while IFS= read -r -d '' file; do
        normal_file="${file%.exclude}"
        normal_after_lang="${normal_file#*/lang/}"
        normal_base_name="${normal_file##*/}"

        [[ "$file" == "$normal_file" ]] && already_excluded=0 || already_excluded=1

        should_include=0
        if [[ "$normal_file" == */lang/* ]] && [[ "$normal_after_lang" =~ $lang_regex ]]; then
            should_include=1
            # Language folders each need an entry in the voice list.
            # Get the first line with ^name, remove ^name, remove // comment, remove single quote, trim whitespace.
            voice_desc=$(sed -n "/^name / { s~^name ~~; s~//.*~~; s~'~~g; s~^\s*~~; s~\s*$~~; p; q }" "$file")
            lang_meta+="\n [ '$normal_base_name', '$normal_after_lang', '${voice_desc:-$normal_after_lang}' ],"
        elif [[ "$normal_file" == *_dict ]] && [[ "${normal_base_name%_dict}" =~ $dict_regex ]]; then
            should_include=1
        elif [[ "$normal_file" == */en_dict ]]; then
            # Most languages use EN as a fallback, so must also include en_dict (assuming it was already compiled).
            should_include=1
        elif [[ "$normal_file" =~ $data_regex ]]; then
            should_include=1
        fi

        if (( should_include && already_excluded )); then
            mv "$file" "$normal_file"
        elif (( !should_include && !already_excluded )); then
            mv "$file" "$file.exclude"
        fi
    done < <(find 'native/espeak-ng-data' -type f -print0 | sort -z)

    python3 "$EMSDK/upstream/emscripten/tools/file_packager.py" "temp/$preload_name.data" \
        --preload "native/espeak-ng-data@/$preload_name/espeak-ng-data" --exclude '*.exclude' \
        --js-output="temp/$preload_name.py.js" --export-es6 --quiet

    # Splitting the js and data into multiple files is much easier, and gives a smaller file size, and loads faster.
    # But... it causes enough headaches with bundlers that I decided to embed everything.
    (
        head -n1 '../emscripten/loadData.js';
        grep -e'FS_createPath' -e'^\s*loadPackage' "temp/$preload_name.py.js";
        sed -n "7,8{s/%package_name%/$preload_name/;p}" '../emscripten/loadData.js';
        printf 'const DATA = `'; node '../emscripten/deflate.js' "temp/$preload_name.data"; printf '`;\n';
        printf "%b\n]};\n" "$lang_meta";
        tail -n+12 '../emscripten/loadData.js';
    ) > "dist/lang/$preload_name.js"

    js_func_name="${preload_name//-/_}"
    printf "export async function %s(Module) { (await import('./lang/%s.js')).loadData(Module); }\n" "$js_func_name" "$preload_name" > "temp/$preload_name.part.js"

    [[ -f "temp/defaultLang.js" ]] || echo "const _defaultLang = $js_func_name;" > "temp/defaultLang.js"

    [[ -f "dist/ephone.d.ts" ]] || cp '../emscripten/ephone.d.ts' dist/
    printf "\nexport const %s: ephoneLanguagePack;\n" "$js_func_name" >> 'dist/ephone.d.ts'
}

build_wasm() {
    emcmake cmake -S .. -B emscripten \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
    make -C emscripten -j1
}

build_ephone() {
    cat temp/*.part.js > 'temp/langPacks.js'

    emcc -O3 -flto \
         -I'../src/include' \
         '../emscripten/ephone_js.c' \
         'emscripten/src/libespeak-ng/libespeak-ng.a' 'emscripten/src/ucd-tools/libucd.a' -lm \
         -s EXPORTED_FUNCTIONS='_wasm_LoadVoiceForTextToIpa,_wasm_TextToIpaWithSourceMap,_malloc,_free' \
         -s EXPORTED_RUNTIME_METHODS='lengthBytesUTF8,UTF8ToString,stringToUTF8,HEAP32,FS' \
         -s ALLOW_MEMORY_GROWTH=1 \
         -s MODULARIZE=1 \
         -s EXPORT_ES6=1 \
         -s FORCE_FILESYSTEM=1 \
         -s SINGLE_FILE=1 \
         --pre-js 'temp/defaultLang.js' \
         --pre-js '../emscripten/inputs.js' \
         --pre-js '../emscripten/inflate.js' \
         --post-js '../emscripten/exports.js' \
         --extern-post-js 'temp/langPacks.js' \
         -s EXPORT_NAME='createEphone' \
         -o 'dist/ephone.js'

    cp '../emscripten/package.json' dist/
    cp '../COPYING' dist/
    cp '../emscripten/README.md' dist/
    cat '../readme-header.md' '../emscripten/README.md' > '../README.md'
}

init
(( do_native ))  && build_native
(( do_preload )) && configure_preload
(( do_wasm ))    && build_wasm
(( do_ephone ))  && build_ephone
exit 0
