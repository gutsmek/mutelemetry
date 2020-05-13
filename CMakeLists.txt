project( mutelemetry )
cmake_minimum_required( VERSION 2.8 )

set(VERSION 0.2.2)
set(SOVERSION 0.2)

# enable c++14
set(CMAKE_CXX_STANDARD 14)

set(CPACK_PACKAGE_NAME lib${PROJECT_NAME})

set( CMAKE_EXPORT_COMPILE_COMMANDS 1 )
LIST( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include( CheckIncludeFile )
include( CheckSymbolExists )
include( CheckFunctionExists )
#include( format )

set( BUILD_SHARED_LIBS ON )

set( LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/output/lib )
set( EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/output/bin )

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -latomic" )
set( CMAKE_C_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -gdwarf-3 -O0 -DDEBUG" )
set( CMAKE_C_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -g -Wextra -Wno-deprecated-declarations -Wold-style-cast")

set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -latomic" )
set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -gdwarf-3 -O0 -DDEBUG" )
set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -g -Wextra -Wno-deprecated-declarations -Wold-style-cast")

file( MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/output/include/ )
file( MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/output/lib )
file( MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/output/bin )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_INSTALL_PREFIX}/include)
link_directories( ${CMAKE_INSTALL_PREFIX}/lib  )

include_directories( ${CMAKE_BINARY_DIR}/output/include )
link_directories( ${CMAKE_BINARY_DIR}/output/lib  )

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

find_package(Boost REQUIRED thread chrono regex filesystem
    iostreams system serialization date_time atomic
    program_options unit_test_framework
    )
include_directories( ${Boost_INCLUDE_DIR} )
link_directories( ${Boost_LIBRARY_DIR} )

find_package(Glog REQUIRED)
include_directories(${GLOG_INCLUDE_DIRS})

# mutelemetry installation path if building out of colibri
include_directories(${MUTELEMETRY_INSTALL_PATH}/include)
link_directories(${MUTELEMETRY_INSTALL_PATH}/lib)

add_subdirectory( src )
add_subdirectory( tests )

#ADD_DEPENDENCIES (muflow muqueue muroute)
