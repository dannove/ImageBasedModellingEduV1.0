project(class4)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-fPIC")

include_directories("../..")

set(INCREMENTAL_SFM_SOURCES
        class4_test_incremental_sfm.cc defines.h functions.cc functions.h visualizer.cc visualizer.h)
add_executable(task4_incremental_sfm ${INCREMENTAL_SFM_SOURCES})
target_link_libraries(task4_incremental_sfm sfm util core features jsoncpp)

set(test_utm_sources
        test_utm.cpp)

add_executable(test_utm ${test_utm_sources})
target_link_libraries(test_utm sfm util core features)
