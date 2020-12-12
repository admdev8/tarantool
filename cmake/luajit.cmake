#
# LuaJIT configuration file.
#
# A copy of LuaJIT is maintained within Tarantool source. It's
# located in third_party/luajit.
#
# LUAJIT_FOUND
# LUAJIT_LIBRARIES
# LUAJIT_INCLUDE_DIRS
#

macro(TestAndAppendFLag flags flag)
    string(REGEX REPLACE "-" "_" TESTFLAG ${flag})
    string(TOUPPER ${TESTFLAG} TESTFLAG)
    # XXX: can't use string(PREPEND ...) on anchient versions.
    set(TESTFLAG "CC_HAS${TESTFLAG}")
    if(${${TESTFLAG}})
        set(${flags} "${${flags}} ${flag}")
    endif()
endmacro()

set(CMAKE_C_FLAGS_BCKP ${CMAKE_C_FLAGS})
set(CMAKE_INSTALL_PREFIX_BCKP ${CMAKE_INSTALL_PREFIX})

# if(NOT ${PROJECT_BINARY_DIR} STREQUAL ${PROJECT_SOURCE_DIR})
#     message(WARNING "QQ: ${PROJECT_BINARY_DIR} => ${PROJECT_SOURCE_DIR}")
#     file(COPY ${PROJECT_SOURCE_DIR}/third_party/luajit DESTINATION ${PROJECT_BINARY_DIR}/third_party/luajit)
#     # execute_process(
#     #     COMMAND ${CMAKE_PROGRAM} -E make_directory ${PROJECT_BINARY_DIR}/third_party/luajit
#     #     COMMAND ${CMAKE_PROGRAM} -E copy_directory ${PROJECT_SOURCE_DIR}/third_party/luajit ${PROJECT_BINARY_DIR}/third_party/luajit
#     # )
# endif()
TestAndAppendFLag(CMAKE_C_FLAGS -Wno-parentheses-equality)
TestAndAppendFLag(CMAKE_C_FLAGS -Wno-tautological-compare)
TestAndAppendFLag(CMAKE_C_FLAGS -Wno-misleading-indentation)
TestAndAppendFLag(CMAKE_C_FLAGS -Wno-varargs)
TestAndAppendFLag(CMAKE_C_FLAGS -Wno-implicit-fallthrough)

set(LUAJIT_SOURCE_ROOT ${PROJECT_SOURCE_DIR}/third_party/luajit)
set(LUAJIT_BINARY_ROOT ${PROJECT_BINARY_DIR}/third_party/luajit)

# To respect CMP0002
set(LUAJIT_ENABLE_GC64 ${LUAJIT_ENABLE_GC64} CACHE BOOL
    "GC64 mode for x64" FORCE)
set(LUAJIT_USE_TEST OFF CACHE BOOL
    "Generate <test> target" FORCE)
set(LUAJIT_TEST_BINARY $<TARGET_FILE:tarantool> CACHE STRING
    "Lua implementation to be used for tests (tarantool)" FORCE)
set(BUILDMODE static CACHE STRING
    "Build mode: build only static lib" FORCE)
set(LUAJIT_SMART_STRINGS ON CACHE BOOL
    "Harder string hashing function" FORCE)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(LUAJIT_USE_APICHECK ON CACHE BOOL
        "Assertions for the Lua/C API" FORCE)
    set(LUAJIT_USE_ASSERT ON CACHE BOOL
        "Assertions for the whole LuaJIT VM" FORCE)
endif()
if(ENABLE_VALGRIND)
    set(LUAJIT_USE_SYSMALLOC ON CACHE BOOL
        "System provided memory allocator (realloc)" FORCE)
    set(LUAJIT_USE_VALGRIND ON CACHE BOOL
        "Valgrind support" FORCE)
endif()

# AddressSanitizer - CFLAGS were set globaly.
if(ENABLE_ASAN)
    set(LUAJIT_USE_ASAN ON CACHE BOOL "Address sanitizer support" FORCE)
    # set(LUAJIT_LDFLAGS "${LUAJIT_LDFLAGS} -fsanitize=address")
endif()
if(ENABLE_GCOV)
    # set(LUAJIT_C_FLAGS "${LUAJIT_C_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

add_subdirectory(${LUAJIT_SOURCE_ROOT} ${LUAJIT_BINARY_ROOT} EXCLUDE_FROM_ALL)

# ExternalProject_Add(libluajit
#     PREFIX "${PROJECT_BINARY_DIR}/third_party/luajit/"
#     SOURCE_DIR "${PROJECT_BINARY_DIR}/third_party/luajit/"
#     BINARY_DIR "${PROJECT_BINARY_DIR}/third_party/luajit/"
#     INSTALL_DIR "${PROJECT_BINARY_DIR}/third_party/luajit/"
#     BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} libluajit
#     CMAKE_ARGS ${LUAJIT_CMAKE_ARGS}
#     TEST_EXCLUDE_FROM_MAIN ON
#     TEST_COMMAND ${CMAKE_MAKE_PROGRAM} test
# )
set(LUAJIT_PREFIX ${LUAJIT_BINARY_ROOT}/src)
set(LUAJIT_INCLUDE ${LUAJIT_PREFIX})
set(LUAJIT_LIB ${LUAJIT_PREFIX}/libluajit.a)

add_dependencies(build_bundled_libs libluajit)
# set(luajit_cc ${CMAKE_C_COMPILER} ${CMAKE_C_COMPILER_ARG1})
# set(luajit_hostcc ${CMAKE_HOST_C_COMPILER})
# # CMake rules concerning strings and lists of strings are
# # weird.
# #   set (foo "1 2 3") defines a string, while
# #   set (foo 1 2 3) defines a list.
# # Use separate_arguments() to turn a string into a list
# # (splits at ws). It appears that variable expansion rules are
# # context-dependent.
# # With the current arrangement add_custom_command() does the
# # the right thing. We can even handle pathnames with spaces
# # though a path with an embeded semicolon or a quotation mark
# # will most certainly wreak havok.
# #
# # This stuff is extremely fragile, proceed with caution.
#
# if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
#     # Pass sysroot - prepended in front of system header/lib dirs,
#     # i.e. <sysroot>/usr/include, <sysroot>/usr/lib.
#     # Needed for XCode users without command line tools installed,
#     # they have headers/libs deep inside /Applications/Xcode.app/...
#     if(NOT "${CMAKE_OSX_SYSROOT}" STREQUAL "")
#         set(luajit_cflags ${luajit_cflags} ${CMAKE_C_SYSROOT_FLAG} ${CMAKE_OSX_SYSROOT})
#         set(luajit_ldflags ${luajit_ldlags} ${CMAKE_C_SYSROOT_FLAG} ${CMAKE_OSX_SYSROOT})
#         set(luajit_hostcc ${luajit_hostcc} ${CMAKE_C_SYSROOT_FLAG} ${CMAKE_OSX_SYSROOT})
#     endif()
#     # Pass deployment target
#     if("${CMAKE_OSX_DEPLOYMENT_TARGET}" STREQUAL "")
#         # Default to 10.6 since @rpath support is NOT available in
#         # earlier versions, needed by AddressSanitizer.
#         set(luajit_osx_deployment_target 10.6)
#     else()
#         set(luajit_osx_deployment_target ${CMAKE_OSX_DEPLOYMENT_TARGET})
#     endif()
#     set(luajit_ldflags
#         ${luajit_ldflags} -Wl,-macosx_version_min,${luajit_osx_deployment_target})
# endif()
# # Pass the same toolchain that is used for building of
# # tarantool itself, because tools from different toolchains
# # can be incompatible. A compiler and a linker are already set
# # above.
# set(luajit_ld ${CMAKE_LINKER})
# set(luajit_ar ${CMAKE_AR} rcus)
# # Enablibg LTO for luajit if DENABLE_LTO set.
# if(ENABLE_LTO)
#     message(STATUS "Enable LTO for luajit")
#     set(luajit_ldflags ${luajit_ldflags} ${LDFLAGS_LTO})
#     message(STATUS "ld: " ${luajit_ldflags})
#     set(luajit_cflags ${luajit_cflags} ${CFLAGS_LTO})
#     message(STATUS "cflags: " ${luajit_cflags})
#     set(luajit_ar  ${AR_LTO} rcus)
#     message(STATUS "ar: " ${luajit_ar})
# endif()
# set(luajit_strip ${CMAKE_STRIP})
#
# set(luajit_buildoptions
#     BUILDMODE=static
#     HOST_CC="${luajit_hostcc}"
#     TARGET_CC="${luajit_cc}"
#     TARGET_CFLAGS="${luajit_cflags}"
#     TARGET_LD="${luajit_ld}"
#     TARGET_LDFLAGS="${luajit_ldflags}"
#     TARGET_AR="${luajit_ar}"
#     TARGET_STRIP="${luajit_strip}"
#     TARGET_SYS="${CMAKE_SYSTEM_NAME}"
#     CCOPT="${luajit_ccopt}"
#     CCDEBUG="${luajit_ccdebug}"
#     XCFLAGS="${luajit_xcflags}"
#     Q=''
#     # We need to set MACOSX_DEPLOYMENT_TARGET to at least 10.6,
#     # because 10.4 SDK (which is set by default in LuaJIT's
#     # Makefile) is not longer included in Mac OS X Mojave 10.14.
#     # See also https://github.com/LuaJIT/LuaJIT/issues/484
#     MACOSX_DEPLOYMENT_TARGET="${luajit_osx_deployment_target}")
# unset(luajit_buildoptions)

install(
    FILES
        ${LUAJIT_SOURCE_ROOT}/src/lua.h
        ${LUAJIT_SOURCE_ROOT}/src/lualib.h
        ${LUAJIT_SOURCE_ROOT}/src/lauxlib.h
        ${LUAJIT_SOURCE_ROOT}/src/luaconf.h
        ${LUAJIT_SOURCE_ROOT}/src/lua.hpp
        ${LUAJIT_SOURCE_ROOT}/src/luajit.h
        ${LUAJIT_SOURCE_ROOT}/src/lmisclib.h
    DESTINATION ${MODULE_INCLUDEDIR}
)

set(LuaJIT_FIND_REQUIRED TRUE)
find_package_handle_standard_args(LuaJIT
    REQUIRED_VARS LUAJIT_INCLUDE LUAJIT_LIB)
set(LUAJIT_INCLUDE_DIRS ${LUAJIT_INCLUDE})
set(LUAJIT_LIBRARIES ${LUAJIT_LIB})

include_directories(${LUAJIT_SOURCE_ROOT}/src)

if(TARGET_OS_DARWIN)
    # Necessary to make LuaJIT work on Darwin, see
    # http://luajit.org/install.html
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pagezero_size 10000 -image_base 100000000")
endif()
add_definitions(-DLUAJIT_SMART_STRINGS=1)
if(LUAJIT_ENABLE_GC64)
    add_definitions(-DLUAJIT_ENABLE_GC64)
endif()

set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS_BCKP})
set(CMAKE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX_BCKP})
