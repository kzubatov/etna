find_package(Git)

if(GIT_EXECUTABLE)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE ETNA_VERSION
    RESULT_VARIABLE ERROR_CODE
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  string(REGEX MATCHALL "[0-9]+" NUM_LIST "${ETNA_VERSION}") 
  list(LENGTH NUM_LIST LEN)
  if (LEN STREQUAL "3")
    list(GET NUM_LIST 0 MAJOR)
    list(GET NUM_LIST 1 MINOR)
    list(GET NUM_LIST 2 PATCH)
    target_compile_definitions(etna PUBLIC -DETNA_MAJOR=${MAJOR} -DETNA_MINOR=${MINOR} -DETNA_PATCH=${PATCH})
    return()
  endif()
endif()

target_compile_definitions(etna PUBLIC -DETNA_MAJOR=0 -DETNA_MINOR=1 -DETNA_PATCH=0)

message("Failed to determine version from Git tags. Using default version 0.1.0")
