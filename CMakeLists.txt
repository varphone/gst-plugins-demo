cmake_minimum_required(VERSION 3.0.2)

project(gstdemo
	VERSION 1.0.0
)

add_definitions(-DPACKAGE="${PROJECT_NAME}")
add_definitions(-DVERSION="${PROJECT_VERSION}")

# Build options
if(NOT BUILD_SRC)
	set(BUILD_SRC ON)
endif()
if(NOT BUILD_DOCS)
	set(BUILD_DOCS OFF)
endif()
if(NOT BUILD_EXAMPLES)
	set(BUILD_EXAMPLES OFF)
endif()
if(NOT BUILD_TESTS)
	set(BUILD_TESTS OFF)
endif()
if(NOT BUILD_TOOLS)
	set(BUILD_TOOLS OFF)
endif()

# Find depends
if(UNIX)
	find_package(PkgConfig QUIET)
	pkg_check_modules(GLIB2 REQUIRED glib-2.0)
	pkg_check_modules(GST1 REQUIRED gstreamer-1.0)
	pkg_check_modules(GST1_VIDEO REQUIRED gstreamer-video-1.0)
endif()

# Subdirectories
if(BUILD_SRC)
	add_subdirectory(src)
endif()
if(BUILD_DOCS)
	add_subdirectory(docs)
endif()
if(BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()
if(BUILD_TESTS)
	add_subdirectory(tests)
endif()
if(BUILD_TOOLS)
	add_subdirectory(tools)
endif()