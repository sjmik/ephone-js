const LOAD = (Module, loadPackage) => {
  Module.FS_createPath('...');
  loadPackage({
    files: [{ filename: '...', start: 0, end: 99 }],
    remote_package_size: 99
  });
};
const NAME = '%package_name%';
const DATA = `%deflated_data%`;
const META = { name: '...', desc: '...', voices: [] };

export function loadData(Module) {
  if (!Module.preloadResults) Module.preloadResults = {};
  if (Module.preloadResults[NAME]) {
    console.error(`Requested to load [${NAME}] more than once`, Module.preloadResults[NAME]);
    return;
  }
  Module.preloadResults[NAME] = { done: false, fromCache: false };

  const run = (Module) => {
    const u8data = Module._inflateData(DATA);
    const loadPackage = (metadata) => {
      if (metadata.remote_package_size !== u8data.byteLength) {
        throw new Error(`Data is corrupt ${metadata.remote_package_size} !== ${u8data.byteLength}`);
      }
      for (const { filename, start, end } of metadata.files) {
        const data = u8data.subarray(start, end);
        Module.FS_createDataFile(filename, null, data, true, true, true);
      }
    };
    LOAD(Module, loadPackage);
    Module._addLangMeta(META);
    Module.preloadResults[NAME].done = true;
  };

  if (Module.calledRun) {
    run(Module);
  } else {
    Module.preRun.push(run);
  }
}
