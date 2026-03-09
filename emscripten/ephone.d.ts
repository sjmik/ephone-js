export interface ephoneVoice {
  name: string;
  desc: string;
}

export interface ephoneModule {
  getVoices(): ephoneVoice[];
  setVoice(name: string): ephoneVoice;
  textToIpa(text: string): string;
  textToIpaWithSourceMap(text: string): {
    text: string;
    ipa: string;
    sourceMap: [number, number][];
    asSpans: () => [string, string][];
  };
}

export type ephoneLanguagePack = (Module: ephoneModule) => Promise<void>;

export interface ephoneOptions {
  languages?: ephoneLanguagePack | ephoneLanguagePack[];
  preRun?: (Module: ephoneModule) => void;
  postRun?: (Module: ephoneModule) => void;
}

export default function createEphone(
  options?: ephoneLanguagePack | ephoneLanguagePack[] | ephoneOptions
): Promise<ephoneModule>;
