########################## Version and language setup ##########################

# Minimum version of cmake required to run this project
cmake_minimum_required(VERSION 3.16)

# Name of the project, languages used, version of the project
project(dmenu LANGUAGES CXX VERSION 5.0)

# Language standards
set(CMAKE_CXX_STANDARD 20)

################################## File setup ##################################
# Defining project files
set(DMENU_SOURCE_FILES
    drw.cpp
    dmenu.cpp
    util.cpp
)

set(DMENU_HEADER_FILES
        config.hpp
        drw.hpp
        util.hpp
    )


############################### Executable setup ###############################
# Adding the executable
add_executable(${PROJECT_NAME} ${DMENU_SOURCE_FILES} ${DMENU_HEADER_FILES})
add_executable(stest stest.cpp)

################################ Warnings setup ################################
set(WARNINGS "-Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wpedantic -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wmisleading-indentation -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast")
set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS ${WARNINGS})
set_target_properties(stest PROPERTIES COMPILE_FLAGS ${WARNINGS})


################################## Libs setup ##################################
set(X11_DEPS "X11" "Xft" "Xinerama" "Xrender")
set(FREETYPE_DEPS "fontconfig")


include_directories (/usr/include/freetype2)

foreach(LIB IN LISTS X11_DEPS FREETYPE_DEPS)
    find_library(${LIB}_LIB ${LIB})

    # Xinerama is an optional library, if there are more optional libraries,
    # they will be brought out into a separated loop
    if("${LIB}" STREQUAL "Xinerama")
        if(NOT ${LIB}_LIB)
            message(WARNING "l${LIB} was not found. Proceeding without it")
        else()
            target_link_libraries(${PROJECT_NAME} PRIVATE ${${LIB}_LIB})
            target_link_libraries(stest PRIVATE ${${LIB}_LIB})
            add_compile_definitions(XINERAMA=1)
        endif()
    else()
        if (NOT ${LIB}_LIB)
            message(FATAL_ERROR "l${LIB} was not found. Cannot proceed.")
        else()
            target_link_libraries(${PROJECT_NAME} PRIVATE ${${LIB}_LIB})
            target_link_libraries(stest PRIVATE ${${LIB}_LIB})
        endif()
    endif()
endforeach()

################################ Compiler setup ################################
# definitions
add_compile_definitions(
    VERSION="${PROJECT_VERSION}"
    _DEFAULT_SOURCE
    _BSD_SOURCE
    _XOPEN_SOURCE=700
    _POSIX_C_SOURCE=200809L
    )

# warnings
add_compile_options("-pedantic" "-Wall" "-Werror")

# optimisation
add_compile_options(-Os)

############################## installation setup ##############################
SET(CMAKE_INSTALL_PREFIX "/usr/local")
# Install dmenu and stest
INSTALL(TARGETS ${PROJECT_NAME} stest RUNTIME)

# Install dmenu_path and dmenu_run
INSTALL(PROGRAMS dmenu_path dmenu_run TYPE BIN)

# Install documentation
INSTALL(FILES ${PROJECT_NAME}.1 stest.1 TYPE MAN)

# uninstall target
if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/modules/uninstall.cmake"
    IMMEDIATE @ONLY)

  add_custom_target(uninstall
      COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/modules/uninstall.cmake)
endif()
