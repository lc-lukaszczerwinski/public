# Specify the minimum required version of CMake
cmake_minimum_required(VERSION 3.25)

# Set the project name
project(HFTDojo LANGUAGES CXX)

include(CTest)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Threads REQUIRED)
find_package(asio CONFIG QUIET)

set(HFT_HAS_ASIO_TARGET FALSE)
if(NOT TARGET asio::asio)
  find_path(ASIO_INCLUDE_DIR NAMES asio.hpp)
  if(ASIO_INCLUDE_DIR)
    add_library(asio::asio INTERFACE IMPORTED)
    set_target_properties(asio::asio PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${ASIO_INCLUDE_DIR}"
    )
    set(HFT_HAS_ASIO_TARGET TRUE)
  endif()
else()
  set(HFT_HAS_ASIO_TARGET TRUE)
endif()

# Set output directory for all executables
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Require C++20 standard for all targets
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HFT_REGISTER_CTESTS "Register each executable as a CTest test" ON)

function(add_dojo_executable source_file)
  get_filename_component(target_name ${source_file} NAME_WE)

  add_executable(${target_name} ${source_file})
  target_include_directories(${target_name} PRIVATE ${CMAKE_SOURCE_DIR})

  if(HFT_REGISTER_CTESTS)
    add_test(NAME ${target_name} COMMAND $<TARGET_FILE:${target_name}>)
  endif()

  if("${target_name}" MATCHES "asio")
    if(NOT HFT_HAS_ASIO_TARGET)
      message(FATAL_ERROR
        "Target ${target_name} requires Asio headers (asio.hpp), but none were found. "
        "Install Asio or provide it via CMAKE_PREFIX_PATH/include paths."
      )
    endif()

    target_link_libraries(${target_name}
      PRIVATE
        asio::asio
        Threads::Threads
    )
  endif()
endfunction()

file(GLOB DOJO_EXECUTABLE_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/*.cpp")

foreach(dojo_source ${DOJO_EXECUTABLE_SOURCES})
  add_dojo_executable(${dojo_source})
endforeach()
