include(CheckSymbolExists)
include(CheckIncludeFile)

check_symbol_exists(mkstemp "stdlib.h" HAVE_MKSTEMP)
check_include_file("nbtool_config.h" HAVE_NBTOOL_CONFIG_H)
check_symbol_exists(optreset "getopt.h;unistd.h" HAVE_DECL_OPTRESET)
check_include_file("sys/endian.h" HAVE_SYS_ENDIAN_H)
check_symbol_exists(iswblank "wctype.h" HAVE_ISWBLANK)
