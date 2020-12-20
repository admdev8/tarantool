macro(add_compile_flags langs)
    foreach(_lang ${langs})
        string (REPLACE ";" " " _flags "${ARGN}")
        set ("CMAKE_${_lang}_FLAGS" "${CMAKE_${_lang}_FLAGS} ${_flags}")
        unset (${_lang})
        unset (${_flags})
    endforeach()
endmacro(add_compile_flags)

macro(set_source_files_compile_flags)
    foreach(file ${ARGN})
        get_filename_component(_file_ext ${file} EXT)
        set(_lang "")
        if ("${_file_ext}" STREQUAL ".m")
            set(_lang OBJC)
            # CMake believes that Objective C is a flavor of C++, not C,
            # and uses g++ compiler for .m files.
            # LANGUAGE property forces CMake to use CC for ${file}
            set_source_files_properties(${file} PROPERTIES LANGUAGE C)
        elseif("${_file_ext}" STREQUAL ".mm")
            set(_lang OBJCXX)
        endif()

        if (_lang)
            get_source_file_property(_flags ${file} COMPILE_FLAGS)
            if ("${_flags}" STREQUAL "NOTFOUND")
                set(_flags "${CMAKE_${_lang}_FLAGS}")
            else()
                set(_flags "${_flags} ${CMAKE_${_lang}_FLAGS}")
            endif()
            # message(STATUS "Set (${file} ${_flags}")
            set_source_files_properties(${file} PROPERTIES COMPILE_FLAGS
                "${_flags}")
        endif()
    endforeach()
    unset(_file_ext)
    unset(_lang)
endmacro(set_source_files_compile_flags)

# A helper function to compile *.lua source into *.lua.c sources
function(lua_source varname filename)
    if (IS_ABSOLUTE "${filename}")
        set (srcfile "${filename}")
        set (tmpfile "${filename}.new.c")
        set (dstfile "${filename}.c")
    else(IS_ABSOLUTE "${filename}")
        set (srcfile "${CMAKE_CURRENT_SOURCE_DIR}/${filename}")
        set (tmpfile "${CMAKE_CURRENT_BINARY_DIR}/${filename}.new.c")
        set (dstfile "${CMAKE_CURRENT_BINARY_DIR}/${filename}.c")
    endif(IS_ABSOLUTE "${filename}")
    get_filename_component(module ${filename} NAME_WE)
    get_filename_component(_name ${dstfile} NAME)
    string(REGEX REPLACE "${_name}$" "" dstdir ${dstfile})
    if (IS_DIRECTORY ${dstdir})
    else()
        file(MAKE_DIRECTORY ${dstdir})
    endif()

    ADD_CUSTOM_COMMAND(OUTPUT ${dstfile}
        COMMAND ${ECHO} 'const char ${module}_lua[] =' > ${tmpfile}
        COMMAND ${CMAKE_BINARY_DIR}/extra/txt2c ${srcfile} >> ${tmpfile}
        COMMAND ${ECHO} '\;' >> ${tmpfile}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${tmpfile} ${dstfile}
        COMMAND ${CMAKE_COMMAND} -E remove ${tmpfile}
        DEPENDS ${srcfile} txt2c libluajit)

    set(var ${${varname}})
    set(${varname} ${var} ${dstfile} PARENT_SCOPE)
endfunction()

function(bin_source varname srcfile dstfile)
    set(var ${${varname}})
    set (srcfile "${CMAKE_CURRENT_SOURCE_DIR}/${srcfile}")
    set (dstfile "${CMAKE_CURRENT_BINARY_DIR}/${dstfile}")
    set(${varname} ${var} "${dstfile}" PARENT_SCOPE)
    set (tmpfile "${dstfile}.tmp")
    get_filename_component(module ${dstfile} NAME_WE)

    ADD_CUSTOM_COMMAND(OUTPUT ${dstfile}
        COMMAND ${ECHO} 'const unsigned char ${module}_bin[] = {' > ${tmpfile}
        COMMAND ${CMAKE_BINARY_DIR}/extra/bin2c "${srcfile}" >> ${tmpfile}
        COMMAND ${ECHO} '}\;' >> ${tmpfile}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${tmpfile} ${dstfile}
        COMMAND ${CMAKE_COMMAND} -E remove ${tmpfile}
        DEPENDS ${srcfile} bin2c)

endfunction()

function(cdef_source varname filename modulename)
    set (srcfile "${CMAKE_CURRENT_SOURCE_DIR}/${filename}")
    set (tmpfile "${CMAKE_CURRENT_BINARY_DIR}/${filename}.new.c")
    set (dstfile "${CMAKE_CURRENT_BINARY_DIR}/${filename}.c")
    get_filename_component(dstdir ${dstfile} DIRECTORY)
    if (NOT IS_DIRECTORY ${dstdir})
        file(MAKE_DIRECTORY ${dstdir})
    endif()

    get_directory_property(includes INCLUDE_DIRECTORIES)
    # `COMMAND_EXPAND_LISTS` requires cmake 3.8+, if we need to
    # support anything older, than we could wrap the logics inside of
    # fficdefgen.sh script where we would need to manually convert
    # cmake lists to `-I...`
    if (NOT POLICY CMP0067)
        message(FATAL_ERROR "cdef_source() requires CMake 3.8+")
    endif()
    set(incs $<$<BOOL:${includes}>:-I;$<JOIN:${includes},;-I;>>)
    set(generated_sql_inc ${CMAKE_BINARY_DIR}/src/box/sql)

    add_custom_command(OUTPUT ${dstfile}
        COMMAND_EXPAND_LISTS
        COMMAND
            "${CMAKE_CXX_COMPILER}" -E -CC "${incs}" "-I${generated_sql_inc}" 
                ${srcfile} |
            ${CMAKE_SOURCE_DIR}/extra/fficdefgen.sh > ${tmpfile}
        COMMAND ${ECHO} 'const char ${modulename}_cdef[] =' > ${dstfile}
        COMMAND ${CMAKE_BINARY_DIR}/extra/txt2c ${tmpfile} >> ${dstfile}
        COMMAND ${ECHO} '\\\;' >> ${dstfile}
        COMMAND ${CMAKE_COMMAND} -E remove ${tmpfile}
        DEPENDS
            ${srcfile}
            txt2c generate_sql_files
            ${CMAKE_SOURCE_DIR}/extra/fficdefgen.sh
    )

    set(var ${${varname}})
    set(${varname} ${var} ${dstfile} PARENT_SCOPE)
endfunction()
