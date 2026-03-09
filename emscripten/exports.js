Module.getVoices = () => {
  return Array.from(Module._voices, ([, [name, , , desc]]) => ({ name, desc }));
};

const _stringPtr = (string) => {
  let ptr;
  const sz = Module.lengthBytesUTF8(string) + 1;
  return {
    get: () => {
      if (ptr !== undefined) return ptr;
      ptr = Module._malloc(sz);
      Module.stringToUTF8(string, ptr, sz);
      return ptr;
    },
    free: () => {
      if (ptr === undefined) return;
      Module._free(ptr);
      ptr = undefined;
    }
  };
};

Module.setVoice = (name) => {
  const voice = Module._voices.get(name.toLowerCase());
  if (voice) {
    const [properName, dataPath, voicePath, desc] = voice;
    const dataPathPtr = _stringPtr(dataPath);
    const voicePathPtr = _stringPtr(voicePath);
    try {
      if (Module._wasm_LoadVoiceForTextToIpa(dataPathPtr.get(), voicePathPtr.get()) === 1) {
        Module._currentVoice = properName;
        return { name: properName, desc };
      }
    } finally {
      dataPathPtr.free();
      voicePathPtr.free();
    }
  }
  throw new Error(`Requested voice is not available [${name}]`);
};

Module._ensureVoice = () => {
  if (Module._currentVoice) return;
  const first = Module._voices.values().next().value;
  if (!first) throw new Error(`No voices are available!`);
  Module.setVoice(first[0]);
};

Module.textToIpa = (text) => {
  Module._ensureVoice();
  const textPtr = _stringPtr(text);
  let ipaPtr, ipa;
  try {
    ipaPtr = Module._wasm_TextToIpaWithSourceMap(textPtr.get(), null, 0);
    if (ipaPtr === 0) throw new Error(`Failed to produce IPA [${text}]`);
    ipa = Module.UTF8ToString(ipaPtr);
  } finally {
    textPtr.free();
    if (ipaPtr !== undefined) Module._free(ipaPtr);
  }
  return ipa;
};

const _resultAsSpans = (result) => {
  const spans = [];
  for (let i = 0; i < result.sourceMap.length; i++) {
    const [source_start, ipa_start] = result.sourceMap[i];
    const [source_end, ipa_end] =
      i + 1 < result.sourceMap.length
        ? result.sourceMap[i + 1]
        : [result.text.length, result.ipa.length];
    spans.push([
      result.text.substring(source_start, source_end).trim(),
      result.ipa.substring(ipa_start, ipa_end).trim()
    ]);
  }
  return spans;
};

Module.textToIpaWithSourceMap = (text) => {
  Module._ensureVoice();
  const textPtr = _stringPtr(text);
  let ipaPtr, sourceMapPtr, ipa;
  const sourceMap = [];
  const mapCapacity = text.length * 2;
  try {
    sourceMapPtr = Module._malloc(mapCapacity * 4);
    ipaPtr = Module._wasm_TextToIpaWithSourceMap(textPtr.get(), sourceMapPtr, mapCapacity);
    if (ipaPtr === 0) throw new Error(`Failed to produce IPA [${text}]`);
    ipa = Module.UTF8ToString(ipaPtr);
    const sourceMapIx = sourceMapPtr >> 2;
    const rawSourceMap = Module.HEAP32.subarray(sourceMapIx, sourceMapIx + mapCapacity);
    for (let i = 0; i < mapCapacity - 1; ) {
      const textPos = rawSourceMap[i++];
      const ipaPos = rawSourceMap[i++];
      if (textPos < 0 || ipaPos < 0) break;
      sourceMap.push([textPos, ipaPos]);
    }
  } finally {
    textPtr.free();
    if (ipaPtr !== undefined) Module._free(ipaPtr);
    if (sourceMapPtr !== undefined) Module._free(sourceMapPtr);
  }
  return {
    text,
    ipa,
    sourceMap,
    asSpans() {
      return _resultAsSpans(this);
    }
  };
};
