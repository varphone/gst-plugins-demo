project(gstdemo)

set(SOURCES
	plugin.c
	gstevtest.c
	gsttestsrc.c
)

# Include common headers
include_directories(
	${CMAKE_SOURCE_DIR}/include
)

# Create library
add_library(${PROJECT_NAME} SHARED
	${SOURCES}
)

# Add definitions
add_definitions(
	${GLIB2_CLFAGS}
	${GST1_CFLAGS}
	${GST1_VIDEO_CFLAGS}
)

# Link depends
target_link_libraries(${PROJECT_NAME}
	${GLIB2_LDFALGS}
	${GST1_LDFLAGS}
	${GST1_VIDEO_LDFLAGS}
)
