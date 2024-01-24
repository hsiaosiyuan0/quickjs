cmake_minimum_required(VERSION 3.24)

include_directories(${PROJECT_SOURCE_DIR}/include)

add_executable(myjs myjs.c)

target_link_libraries(myjs quickjs)
target_link_libraries(myjs qjslibs)



