if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libdshowcapture/dshowcapture.hpp")
	message(STATUS "libdshowcapture submodule not found!  Please fetch submodules.  cell plugin disabled.")
	return()
endif()

project(cell)

find_package(FFmpeg REQUIRED COMPONENTS avcodec avutil)
include_directories(${FFMPEG_INCLUDE_DIRS})

set(cell_HEADERS
	#encode-dstr.hpp
	ffmpeg-decode.h)

set(MODULE_DESCRIPTION "OBS cell plugin module")

configure_file(${CMAKE_SOURCE_DIR}/cmake/winrc/obs-module.rc.in win-dshow.rc)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/virtualcam-guid.h.in ${CMAKE_CURRENT_BINARY_DIR}/virtualcam-guid.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(win-dshow_SOURCES
	win-dshow.cpp
	#win-dshow-encoder.cpp
	dshow-plugin.cpp
	ffmpeg-decode.c
	win-dshow.rc)

set(virtualcam_LIBS
	"${CMAKE_CURRENT_SOURCE_DIR}/vcam_strm/vcam_strm.lib"
)
file(GLOB virtualcam-output_SOURCES
	tiny-nv12-scale.c
	${CMAKE_CURRENT_SOURCE_DIR}/vcam_strm/vcam.cpp
	shared-memory-queue.c
	virtualcam.c)
file(GLOB virtualcam-output_HEADERS
	tiny-nv12-scale.h
	${CMAKE_CURRENT_SOURCE_DIR}/vcam_strm/vcam.h
	${CMAKE_CURRENT_SOURCE_DIR}/vcam_strm/vcam_strm.h
	shared-memory-queue.h)


file(GLOB libairplay_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/libairplay/*.cpp)
file(GLOB libairplay_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/libairplay/*.h)
set(AIRPLAY_LIBS
	"${CMAKE_CURRENT_SOURCE_DIR}/libairplay/libs/postproc.lib"
	"${CMAKE_CURRENT_SOURCE_DIR}/libairplay/libs/fdk-aac.lib"
	"${CMAKE_CURRENT_SOURCE_DIR}/libairplay/libs/mycrypt.lib"
	"${CMAKE_CURRENT_SOURCE_DIR}/libairplay/libs/AirPlay.lib"
)


add_library(win-dshow MODULE
	${win-dshow_SOURCES}
	${win-dshow_HEADERS}
	${virtualcam-output_SOURCES}
	${virtualcam-output_HEADERS}
	#${libdshowcapture_SOURCES}
	#${libdshowcapture_HEADERS}
	${libairplay_SOURCES}
	${libairplay_HEADERS}
	)
target_link_libraries(win-dshow
	libobs
	${virtualcam_LIBS}
	${AIRPLAY_LIBS}
	strmiids
	ksuser
	wmcodecdspuuid
	w32-pthreads
	${FFMPEG_LIBRARIES})
set_target_properties(win-dshow PROPERTIES FOLDER "plugins/win-dshow")

source_group("libairplay\\Source Files" FILES ${libairplay_SOURCES})
source_group("libairplay\\Header Files" FILES ${libairplay_HEADERS})

source_group("cellcamera\\Source Files" FILES ${virtualcam-output_SOURCES})
source_group("cellcamera\\Header Files" FILES ${virtualcam-output_HEADERS})

install_obs_plugin_with_data(win-dshow data)
