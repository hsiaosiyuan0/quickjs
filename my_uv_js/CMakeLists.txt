cmake_minimum_required(VERSION 3.24)

include_directories(${PROJECT_SOURCE_DIR}/include)

find_path(LIBUV_INCLUDE_DIR
  NAMES uv.h
  PATHS /usr/include /usr/local/include /opt/homebrew/include  # 覆盖常见路径
)

find_library(LIBUV_LIBRARY
  NAMES uv libuv.dylib
  PATHS /usr/lib /usr/local/lib /opt/homebrew/lib  # 覆盖常见路径
)

if (NOT LIBUV_INCLUDE_DIR OR NOT LIBUV_LIBRARY)
  message(FATAL_ERROR "libuv not found!")
endif()

include_directories(${LIBUV_INCLUDE_DIR})

add_executable(my_uv_js my_uv_js.c)

target_link_libraries(my_uv_js quickjs)
target_link_libraries(my_uv_js qjslibs)
target_link_libraries(my_uv_js ${LIBUV_LIBRARY})



