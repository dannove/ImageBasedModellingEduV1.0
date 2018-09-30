project(class3)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-fPIC")

include_directories("../..")

set(VISUAL_HEADER visualizer.h)  #${VISUAL_HEADER}
set(VISUAL_SOURCE visualizer.cc)

# test triangle
set(TRIANGLE_FILE
        class3_test_triangle.cc)
add_executable(task3_test_triangle ${TRIANGLE_FILE})
target_link_libraries(task3_test_triangle sfm util core)

#test p3p
set(P3P_FILE
        class3_test_p3p.cc)
add_executable(task3_test_p3p ${P3P_FILE})
target_link_libraries(task3_test_p3p sfm util core features)


#test jacobian
set(JACOBIAN_FILE class3_test_jacobian.cc)
add_executable(task3_test_jacobian ${JACOBIAN_FILE})
target_link_libraries(task3_test_jacobian sfm util core)


# test bundle adjustment
set(BA_FILES
        class3_test_bundle_adjustment.cc)
add_executable(task3_test_ba ${BA_FILES} ${VISUAL_HEADER} ${VISUAL_SOURCE})
target_link_libraries(task3_test_ba sfm util core features)

#test lm optimize
set(LM_OPTIMIZE_FILES
        class3_test_lm_optimize.cc)

add_executable(task3_test_lm_optimize ${LM_OPTIMIZE_FILES})

target_link_libraries(task3_test_lm_optimize sfm util core features)
