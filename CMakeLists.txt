cmake_minimum_required(VERSION 3.14)
project(pASMc VERSION 1.0)

# Set the C and C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Set the environment for finding GTK4
set(ENV{PKG_CONFIG_PATH} "C:/_privat/bins/msys64/mingw64/lib/pkgconfig")

# Find GTK4 using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED gtk4)

# Include GTK4 directories and link libraries
include_directories(${GTK4_INCLUDE_DIRS})
link_directories(${GTK4_LIBRARY_DIRS})
add_definitions(${GTK4_CFLAGS_OTHER})

# Add source files to the project
set(SOURCES
    main.c          # Main file to generate the executable
    gtkgui.c        # GUI source that uses CTools and GTK4
    putils.c        # Utility functions
    pconstants.c        # Utility functions
)

# Add headers for documentation/organization
set(HEADERS
    include/pconstants.h  # Constants
    include/CTools.h      # CTools header
)

# Add CTools as a static library
add_subdirectory(CTools)
include_directories(CTools/include)

# Define the executable
add_executable(pASMc ${SOURCES})

# Set the executable properties
set_target_properties(pASMc PROPERTIES
#     WIN32_EXECUTABLE TRUE           # No-console on Windows
    C_STANDARD 11                   # Set C standard for this target
)

# if (WIN32)
#     target_compile_definitions(pASMc PRIVATE "ENABLE_DYNAMIC_CONSOLE")
# endif()

# Link the libraries
target_link_libraries(pASMc PRIVATE
    CTools           # Link CTools
    ${GTK4_LIBRARIES} # Link GTK4
)

# Installation rules (optional)
install(TARGETS pASMc DESTINATION bin)
install(FILES ${HEADERS} DESTINATION include)

# Print out useful configuration information
message(STATUS "GTK4 include dirs: ${GTK4_INCLUDE_DIRS}")
message(STATUS "GTK4 library dirs: ${GTK4_LIBRARY_DIRS}")
message(STATUS "GTK4 libraries: ${GTK4_LIBRARIES}")
