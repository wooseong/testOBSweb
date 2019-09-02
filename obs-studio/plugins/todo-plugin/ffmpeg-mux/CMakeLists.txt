project(todo-plugin-mux)

find_package(FFmpeg REQUIRED
	COMPONENTS avcodec avutil avformat)
include_directories(${FFMPEG_INCLUDE_DIRS})

set(todo-plugin-mux_SOURCES
	ffmpeg-mux.c)

set(todo-plugin-mux_HEADERS
	ffmpeg-mux.h)

add_executable(todo-plugin-mux
	${todo-plugin-mux_SOURCES}
	${todo-plugin-mux_HEADERS})

target_link_libraries(todo-plugin-mux
	${FFMPEG_LIBRARIES})

install_obs_core(todo-plugin-mux)
