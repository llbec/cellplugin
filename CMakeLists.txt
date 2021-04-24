project(cellplugin)

find_package(FFmpeg REQUIRED COMPONENTS avcodec avutil)
include_directories(${FFMPEG_INCLUDE_DIRS})

set(MODULE_DESCRIPTION "OBS cell plugin module")

#configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in win-dshow.rc)
#configure_file(${CMAKE_CURRENT_SOURCE_DIR}/virtualcam-guid.h.in ${CMAKE_CURRENT_BINARY_DIR}/virtualcam-guid.h)
#include_directories(${CMAKE_CURRENT_BINARY_DIR})

file(GLOB util_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/utils/*.c
	${CMAKE_CURRENT_SOURCE_DIR}/utils/*.cpp
)
file(GLOB util_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/utils/*.h)

file(GLOB airplay_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/libairplay/*.c
	cell-airplay.cpp
)
file(GLOB airplay_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/libairplay/*.h)
set(airplay_LIBS ${CMAKE_CURRENT_SOURCE_DIR}/libairplay/libs/*.lib)

file(GLOB vcam_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/libvcam/*.cpp
	cell-vcam.cpp
)
file(GLOB vcam_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/libvcam/*.h)
set(vcam_LIBS ${CMAKE_CURRENT_SOURCE_DIR}/libvcam/*.lib)

set(cellplugin_SOURCES
	cell-plugin.cpp
)

add_library(cellplugin MODULE
	${cellplugin_SOURCES}
	${cellplugin_HEADERS}
	${vcam_SOURCES}
	${vcam_HEADERS}
	${airplay_SOURCES}
	${airplay_HEADERS}
	${util_SOURCES}
	${util_HEADERS}
)
target_link_libraries(cellplugin
	libobs
	${vcam_LIBS}
	${airplay_LIBS}
	#strmiids
	#ksuser
	#wmcodecdspuuid
	#w32-pthreads
	${FFMPEG_LIBRARIES}
)
set_target_properties(cellplugin PROPERTIES FOLDER "plugins/cellplugin")

source_group("libairplay\\Source Files" FILES ${airplay_SOURCES})
source_group("libairplay\\Header Files" FILES ${airplay_HEADERS})

source_group("livcam\\Source Files" FILES ${vcam_SOURCES})
source_group("livcam\\Header Files" FILES ${vcam_HEADERS})

install_obs_plugin_with_data(cellplugin data)

message(STATUS "Cell plugin enabled.")