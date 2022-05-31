add_library (extensions OBJECT
  algorithms.cc
  algorithms.h
)
target_include_directories (extensions PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

#add_executable(name_test name_test.cc
#    $<TARGET_OBJECTS:algorithms> ${OPEN_SPIEL_OBJECTS})
#add_test(name_test name_test)

#add_subdirectory (subdirname)
