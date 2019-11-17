cmake_minimum_required(VERSION 3.5)
project(GemmDemo LANGUAGES C CXX)

set(OS          "linux" CACHE STRING "Target OS: linux, anroid")

#set(CMAKE_BUILD_TYPE "Debug")
if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Release")
    message("cmake compiler in release mode")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
else()
    message("cmake compiler in debug mode")
    set(CMAKE_CXX_FLAGS "-g -O0")
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

####################################
## to use C++11
set (CMAKE_CXX_STANDARD 11)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
if (${CMAKE_CXX_COMPILER_ID} STREQUAL GNU)
    set (CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")
endif()
####################################

# add OpenCV
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

file(GLOB srcs "src/*.cpp")
ADD_EXECUTABLE(
    gemm_demo
    ${srcs}
    )

TARGET_LINK_LIBRARIES(
    gemm_demo
    ${OpenCV_LIBRARIES}
    )

# print
function(string_status text value)
    set(status_placeholder_length 24)
    string(RANDOM LENGTH ${status_placeholder_length} ALPHABET " " status_placeholder)
    string(LENGTH "${text}" status_text_length)
    if(status_text_length LESS status_placeholder_length)
        string(SUBSTRING "${text}${status_placeholder}" 0 ${status_placeholder_length} status_text)
    else()
        set(status_text "${text}")
    endif()
    string(TOLOWER ${value} status_value)
    message(STATUS "${status_text}  : ${status_value}")
endfunction()
message("===============================================================================")
string_status("C compiler"                     ${CMAKE_C_COMPILER})
string_status("C++ compiler"                   ${CMAKE_CXX_COMPILER})
string_status("opencv libs"                    ${OpenCV_LIBRARIES})
string_status("opencv include"                 ${OpenCV_INCLUDE_DIRS})
message("===============================================================================")



