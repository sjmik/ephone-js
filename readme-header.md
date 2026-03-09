# ephone-js : eSpeak NG phoneme generation for the web

This repo is a fork of [espeak-ng](https://github.com/espeak-ng/espeak-ng). The speech generation and other parts unnecessary for generating IPA were removed to make it easier to understand what the code is actually doing. Utility and glue code for inter-operating with JS and improved source-phoneme mapping were added.

The JS package is available on [NPM](https://www.npmjs.com/package/ephone) and its [readme](emscripten/README.md) is copied below.

## Building

There are two build scripts located in the [emscripten](emscripten) folder: `build.sh` and `build-multiple.sh`. The scripts are only expected to work on Linux. You will first need to install python, cmake, and the [emscripten skd](https://github.com/emscripten-core/emsdk) (which includes clang and node; which are also required).

Calling `build-multiple.sh` will call:

- `build.sh clean` to wipe the `./build` folder
- `build.sh native` to build the native program, which is then used to compile specific language files
- `build.sh preload` to convert those language files to a form usable by the wasm build
  - The above two are repeated once for each different set of languages being built
- `build.sh wasm` to use emscripten to build the main program to JS/WASM
- `build.sh ephone` to use emscripten to build the final JS/WASM package
  - Build artifacts can be found in `./build/dist`

If you want to build a custom language pack, it should be pretty straightforward to figure it out by looking at [build-multiple](emscripten/build-multiple.sh).

##
