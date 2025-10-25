cmake_minimum_required(VERSION 3.22)

project(retort LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE RETORT_SOURCES CONFIGURE_DEPENDS
    src/main.cpp
    src/app/*.cpp
    src/cli/*.cpp
    src/index/*.cpp
    src/search/*.cpp
    src/server/*.cpp
    src/writer/*.cpp
    src/util/*.cpp
)

add_executable(retort
    ${RETORT_SOURCES}
)

find_package(SQLite3 REQUIRED)

target_include_directories(retort
    PRIVATE
        src
)

target_link_libraries(retort
    PRIVATE
        SQLite::SQLite3
)

if(CMAKE_EXPORT_COMPILE_COMMANDS AND NOT TARGET link_compile_commands)
    set(link_compile_commands_script "${CMAKE_BINARY_DIR}/link_compile_commands.cmake")
    file(WRITE ${link_compile_commands_script}
        "if(EXISTS \"${CMAKE_BINARY_DIR}/compile_commands.json\")\n"
        "  if(WIN32)\n"
        "    file(COPY \"${CMAKE_BINARY_DIR}/compile_commands.json\" DESTINATION \"${CMAKE_SOURCE_DIR}\")\n"
        "  else()\n"
        "    file(REMOVE \"${CMAKE_SOURCE_DIR}/compile_commands.json\")\n"
        "    file(CREATE_LINK \"${CMAKE_BINARY_DIR}/compile_commands.json\" \"${CMAKE_SOURCE_DIR}/compile_commands.json\" SYMBOLIC)\n"
        "  endif()\n"
        "endif()\n"
    )

    add_custom_target(link_compile_commands ALL
        COMMAND ${CMAKE_COMMAND} -P ${link_compile_commands_script}
        COMMENT "Sync compile_commands.json to the project root"
    )
endif()
