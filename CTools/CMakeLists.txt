cmake_minimum_required(VERSION 3.28)
project(CTools C)

set(CMAKE_C_STANDARD 17)

# Include directories
include_directories(include)

# Source files
add_library(CTools STATIC src/timid.c
        src/treader.c
        include/CTools/timid.h
        include/CTools/treader.h
        include/CTools.h)
