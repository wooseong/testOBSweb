project(todo-plugin)

if(MSVC)
	set(todo-plugin_PLATFORM_DEPS
		w32-pthreads)
endif()

find_package(FFmpeg REQUIRED
	COMPONENTS avcodec avfilter avdevice avutil swscale avformat swresample)

set(todo-plugin_HEADERS
	obs-ffmpeg-formats.h
	obs-ffmpeg-compat.h
	closest-pixel-format.h)

set(todo-plugin_SOURCES
	obs-ffmpeg-audio-encoders.c
	obs-ffmpeg-nvenc.c
	obs-ffmpeg-output.c
	obs-ffmpeg-mux.c
	obs-ffmpeg-source.c
	obs-ffmpeg.cpp)

if(UNIX AND NOT APPLE)
	list(APPEND todo-plugin_SOURCES
		obs-ffmpeg-vaapi.c)
	LIST(APPEND todo-plugin_PLATFORM_DEPS
		${LIBVA_LBRARIES})
endif()

if(WIN32)
	list(APPEND todo-plugin_SOURCES
		jim-nvenc.c
		jim-nvenc-helpers.c)
	list(APPEND todo-plugin_HEADERS
		jim-nvenc.h)
endif()

add_library(todo-plugin MODULE
	${todo-plugin_HEADERS}
	${todo-plugin_SOURCES})

include_directories(${FFMPEG_INCLUDE_DIRS}
	"../../UI"
	${Qt5Core_INCLUDES}
    ${Qt5Widgets_INCLUDES})

target_link_libraries(todo-plugin
	libobs
	media-playback
	${todo-plugin_PLATFORM_DEPS}
	${FFMPEG_LIBRARIES}
	Qt5::Core
	Qt5::Widgets
	obs-frontend-api)

install_obs_plugin_with_data(todo-plugin data)

add_subdirectory(ffmpeg-mux)
