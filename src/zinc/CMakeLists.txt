
file(GLOB_RECURSE SOURCE_FILES *.cpp *.h)
add_executable(zinc ${SOURCE_FILES})
target_link_libraries(zinc libzinc json CLI11)
