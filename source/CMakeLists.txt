cmake_minimum_required (VERSION 3.10)
project (muondetector LANGUAGES CXX C)

string(TIMESTAMP PROJECT_DATE_STRING "%b %d, %Y")

option(MUONDETECTOR_BUILD_GUI "Build the gui for the muondetector" ON)
option(MUONDETECTOR_BUILD_DAEMON "Build the daemon for the muondetector. Defaults to ON on armv7l architecture" OFF)
set(MUONDETECTOR_ON_RASPBERRYPI OFF)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.cmake")

execute_process(COMMAND bash "-c" "git rev-parse --short HEAD | tr -d '\n'"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE PROJECT_VERSION_HASH)



if ((${CMAKE_SYSTEM_PROCESSOR} STREQUAL "armv7l"))
    set(MUONDETECTOR_BUILD_DAEMON ON)
    set(MUONDETECTOR_ON_RASPBERRYPI ON)
endif ()

set(MUONDETECTOR_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/output/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/output/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/output/bin")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


set(CMAKE_BUILD_TYPE Release)

add_compile_options(
    -Wall
    -O3
    )
if (MSVC)
  # Force to always compile with W4
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  endif()
else()

add_compile_options(
    -Wextra
    -Wshadow
    -Wpedantic
    )

endif()

if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "armv7l")
add_compile_options(
    -mthumb
    -mthumb-interwork
    -march=armv7-a
    )
endif (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "armv7l")


if (NOT (WIN32 OR APPLE))
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/MuonPi/muondetector")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "MuonPi <developer@muonpi.org>")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
endif ()
set(CPACK_RESOURCE_FILE_LICENSE "${MUONDETECTOR_CONFIG_DIR}/license")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/output/packages")
set(CPACK_PACKAGE_VENDOR "MuonPi.org")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}-${PROJECT_VERSION_HASH}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")


set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)



if(WIN32)
    set(QWT_DIR "C:/Qwt-6.1.4")
    set(OPENSSL_DIR "C:/Qt/Tools/OpenSSL/Win_x64")
    set(CRYPTOPP_DIR "C:/cryptopp")
    list(APPEND CMAKE_PREFIX_PATH "C:/Qt/5.15.2/mingw81_64")
    list(APPEND CMAKE_PREFIX_PATH "C:/Qt/5.15.2/mingw81_64/lib/cmake/Qt5QuickCompiler")
    list(APPEND CMAKE_PREFIX_PATH "C:/Qt/5.15.2/mingw81_64/lib/cmake/Qt5")

    if (MSVC)
        if("${MSVC_RUNTIME}" STREQUAL "")
            set(MSVC_RUNTIME "static")
        endif()
            # Set compiler options.
        set(variables
          CMAKE_C_FLAGS_DEBUG
          CMAKE_C_FLAGS_MINSIZEREL
          CMAKE_C_FLAGS_RELEASE
          CMAKE_C_FLAGS_RELWITHDEBINFO
          CMAKE_CXX_FLAGS_DEBUG
          CMAKE_CXX_FLAGS_MINSIZEREL
          CMAKE_CXX_FLAGS_RELEASE
          CMAKE_CXX_FLAGS_RELWITHDEBINFO
        )
        if(${MSVC_RUNTIME} STREQUAL "static")
          message(STATUS
            "MSVC -> forcing use of statically-linked runtime."
          )
          foreach(variable ${variables})
            if(${variable} MATCHES "/MD")
              string(REGEX REPLACE "/MD" "/MT" ${variable} "${${variable}}")
            endif()
          endforeach()
        else()
          message(STATUS
            "MSVC -> forcing use of dynamically-linked runtime."
          )
          foreach(variable ${variables})
            if(${variable} MATCHES "/MT")
              string(REGEX REPLACE "/MT" "/MD" ${variable} "${${variable}}")
            endif()
          endforeach()
        endif()
    endif()


else()

set(Qt5_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5/")

endif ()



include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/library.cmake")

set(MUONDETECTOR_ALL_FILES
    "${MUONDETECTOR_LIBRARY_SOURCE_FILES}"
    "${MUONDETECTOR_LIBRARY_HEADER_FILES}"
    )
if (MUONDETECTOR_BUILD_GUI)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/gui.cmake")

set(MUONDETECTOR_ALL_FILES
    "${MUONDETECTOR_ALL_FILES}"
    "${MUONDETECTOR_GUI_SOURCE_FILES}"
    "${MUONDETECTOR_GUI_HEADER_FILES}"
    )
endif (MUONDETECTOR_BUILD_GUI)
if (MUONDETECTOR_BUILD_DAEMON)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/daemon.cmake")

set(MUONDETECTOR_ALL_FILES
    "${MUONDETECTOR_ALL_FILES}"
    "${MUONDETECTOR_DAEMON_SOURCE_FILES}"
    "${MUONDETECTOR_DAEMON_HEADER_FILES}"
    "${MUONDETECTOR_LOGIN_SOURCE_FILES}"
    "${MUONDETECTOR_LOGIN_HEADER_FILES}"
    )
endif (MUONDETECTOR_BUILD_DAEMON)

add_custom_target(clangformat COMMAND clang-format -style=WebKit -i ${MUONDETECTOR_ALL_FILES})


set(CPACK_DEB_COMPONENT_INSTALL ON)

include(CPack)
