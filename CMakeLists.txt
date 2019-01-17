cmake_minimum_required (VERSION 3.5)
project (PRIM)


set (CMAKE_C_STANDARD 11)
set (CMAKE_CXX_STANDARD 11)
find_package (Threads REQUIRED)


list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
include (ExternalProject)
include (FeatureSummary)
include (CMakeDependentOption)


include_directories (..)
add_definitions (-D_UNICODE -D_CRT_SECURE_NO_WARNINGS -D_USE_MATH_DEFINES)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    add_definitions (-D_HAS_EXCEPTIONS=0)
endif ()



if (MSVC)
    set (CMAKE_C_FLAGS "/W3 /WX")
endif ()
if (CMAKE_COMPILER_IS_GNUCC)
    set (CMAKE_C_FLAGS "-std=c11 -Wall -Wno-unused-function -DHAVE_PTHREAD -D_GNU_SOURCE -D_O_CLOEXEC")
endif ()


set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")
set (CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} -DDEBUG")
set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DNDEBUG")
set (CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -DDEBUG")




#include_directories (imp)
include_directories (imp/vec)
include_directories (imp/fileu)
file (GLOB IMP_SRC_FILES
        imp/vec/vec.h
        imp/vec/vec.c
        imp/fileu/fileu.h
        imp/fileu/fileu.c
        )
source_group (imp FILES ${IMP_SRC_FILES})

#add_library (imp STATIC ${IMP_SRC_FILES})




file (GLOB SRC_FILES *.h *.c)
source_group (src FILES ${SRC_FILES})


add_executable (main ${SRC_FILES} ${IMP_SRC_FILES})
#target_link_libraries (main imp)














