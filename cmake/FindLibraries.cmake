include(CMakeParseArguments)

function(find_libraries)

    # Parse the parameters
    set(MULTIVALUEARGS LIBRARIES SEARCH_PATHS)
    cmake_parse_arguments(FIND_LIBRARIES "" "" "${MULTIVALUEARGS}" ${ARGN})

    # Clear the found libraries
    unset(FOUND_LIBRARIES PARENT_SCOPE)

    foreach(LIB ${FIND_LIBRARIES_LIBRARIES})

        # try the user provided paths first
        find_library(FOUND_LIB_${LIB} ${LIB} PATHS ${FIND_LIBRARIES_SEARCH_PATHS} NO_DEFAULT_PATH)

        # if we could not find it, drop to the system paths
        if(NOT FOUND_LIB_${LIB})
            find_library(FOUND_LIB_${LIB} ${LIB})
        endif(NOT FOUND_LIB_${LIB})

        if(FOUND_LIB_${LIB})
            message(STATUS "Found " ${LIB} " at: " ${FOUND_LIB_${LIB}})
            list(APPEND FOUND_LIBRARIES ${FOUND_LIB_${LIB}})
        else()
            message(FATAL "Could not find " ${LIB})
        endif(FOUND_LIB_${LIB})
        
    endforeach(LIB ${LIBRARIES})

    set(FOUND_LIBRARIES ${FOUND_LIBRARIES} PARENT_SCOPE)
endfunction(find_libraries)