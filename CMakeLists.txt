cmake_minimum_required(VERSION 3.1)
project(RCX8Asm)

set_property(GLOBAL PROPERTY C_STANDARD 17)
set_property(GLOBAL PROPERTY C_STANDARD_REQUIRED true)

set(CMAKE_BUILD_TYPE Debug)

add_executable(rcx8asm main.c)
