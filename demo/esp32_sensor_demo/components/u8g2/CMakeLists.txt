file(GLOB u8g2_header_candidates
    "${CMAKE_SOURCE_DIR}/.pio/libdeps/*/U8g2/src/clib/u8g2.h"
    "${CMAKE_SOURCE_DIR}/.pio/libdeps/*/u8g2/src/clib/u8g2.h"
    "${CMAKE_SOURCE_DIR}/.pio/libdeps/*/U8g2/csrc/u8g2.h"
    "${CMAKE_SOURCE_DIR}/.pio/libdeps/*/u8g2/csrc/u8g2.h"
)

list(LENGTH u8g2_header_candidates u8g2_candidate_count)
if(u8g2_candidate_count EQUAL 0)
    message(FATAL_ERROR "U8g2 sources not found under .pio/libdeps/*/(U8g2|u8g2)/(src/clib|csrc). Run 'pio pkg install' first.")
endif()

list(GET u8g2_header_candidates 0 u8g2_header)
get_filename_component(U8G2_SRC_DIR "${u8g2_header}" DIRECTORY)

file(GLOB u8g2_sources
    "${U8G2_SRC_DIR}/u8g2_*.c"
    "${U8G2_SRC_DIR}/u8x8_*.c"
)

idf_component_register(
    SRCS ${u8g2_sources}
    INCLUDE_DIRS "${U8G2_SRC_DIR}"
)
