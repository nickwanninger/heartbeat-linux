cmake_minimum_required(VERSION 3.13)
project(heartbeat-linux C ASM)


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(include)


# creates libhb.so
add_library(hb SHARED
	src/heartbeat.c
	src/entry.S
	)


add_executable(ex
	example/example.c)

target_link_libraries(ex PUBLIC hb pthread)
