cmake_minimum_required(VERSION 3.9)
project(OSSPAssignment C)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES main.c linenoise.c)
set(HEADER_FILES linenoise.h)

add_executable(OSSPAssignment ${SOURCE_FILES} ${HEADER_FILES} main.c)