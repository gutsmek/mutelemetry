link_directories( ${LIBRARY_OUTPUT_PATH} )

include_directories( ${PROJECT_SOURCE_DIR}/include/ )

add_executable( mutelemetry_test mutelemetry_test.cc )
target_link_libraries( mutelemetry_test
    mutelemetry muroute
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}
    )

add_executable( mutelemetry_server mutelemetry_server.cc )
target_link_libraries( mutelemetry_server
    mutelemetry muroute
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}
    )
