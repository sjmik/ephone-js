set(ESPEAK_CONFIG_DIR ${CMAKE_CURRENT_BINARY_DIR}/include)
set(ESPEAK_CONFIG_H ${ESPEAK_CONFIG_DIR}/config.h)
configure_file(config.h.in ${ESPEAK_CONFIG_H})

add_library(espeak-ng-config INTERFACE)
target_include_directories(espeak-ng-config INTERFACE ${ESPEAK_CONFIG_DIR})

add_library(espeak-ng
  common.c
  mnemonics.c
  error.c
  ieee80.c

  compiledata.c
  compiledict.c

  dictionary.c
  encoding.c
  langopts.c
  numbers.c
  phoneme.c
  phonemelist.c
  readclause.c
  setlengths.c
  soundicon.c
  spect.c
  ssml.c
  synthdata.c
  synthesize.c
  tr_languages.c
  translate.c
  translateword.c
  voices.c
  wavegen.c
  speech.c

  espeak_api.c
)
set_target_properties(
  espeak-ng PROPERTIES
  MACOSX_RPATH ON
  INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib"
)

target_include_directories(espeak-ng BEFORE PRIVATE $<TARGET_PROPERTY:espeak-include,INTERFACE_INCLUDE_DIRECTORIES>)

if (NOT MSVC)
  target_compile_options(espeak-ng PRIVATE
    "-fPIC"
    "-fvisibility=hidden"
    "-fno-exceptions"
    "-fwrapv"

    "-pedantic"

    "-Wunused-parameter"
    "-Wunused"
    "-Wuninitialized"
    "-Wreturn-type"
    "-Wmissing-prototypes"
    "-Wint-conversion"
    "-Wimplicit"
    "-Wmisleading-indentation"
  )
endif()
target_compile_definitions(espeak-ng PRIVATE "LIBESPEAK_NG_EXPORT=1")
target_compile_definitions(espeak-ng INTERFACE "LIBESPEAK_NG_EXPORT=1")

target_compile_definitions(espeak-ng PRIVATE "PATH_ESPEAK_DATA=\"${CMAKE_INSTALL_PREFIX}/share/espeak-ng-data\"")

target_link_libraries(espeak-ng PRIVATE espeak-ng-config ucd)
if (NOT MSVC)
  target_link_libraries(espeak-ng PRIVATE m)
endif()
target_link_libraries(espeak-ng PUBLIC espeak-include)

set_target_properties(espeak-ng PROPERTIES
  SOVERSION ${PROJECT_VERSION_MAJOR}
  VERSION ${PROJECT_VERSION}
)

install(TARGETS espeak-ng LIBRARY)
