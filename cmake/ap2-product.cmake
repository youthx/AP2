# AP2 product split (included at end of CMakeLists via cmake_language(DEFER)).
#
#   cforge build / cforge run          Debug   -> sample app (src/main.c)
#   cforge build --config Release      Release -> static library (src/core)

if(NOT TARGET ${PROJECT_NAME})
  return()
endif()

file(GLOB AP2_LIB_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.c")

if(NOT AP2_LIB_SOURCES)
  message(FATAL_ERROR "AP2: no sources found in src/core")
endif()

if(NOT TARGET ap2_lib)
  add_library(ap2_lib STATIC ${AP2_LIB_SOURCES})
endif()

set_target_properties(ap2_lib PROPERTIES OUTPUT_NAME ap2)

target_include_directories(ap2_lib PUBLIC
                           "${CMAKE_CURRENT_SOURCE_DIR}/include"
                           "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb"
                           "${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio")

get_target_property(_ap2_defs ${PROJECT_NAME} COMPILE_DEFINITIONS)
if(_ap2_defs)
  target_compile_definitions(ap2_lib PUBLIC ${_ap2_defs})
endif()

get_target_property(_ap2_opts ${PROJECT_NAME} COMPILE_OPTIONS)
if(_ap2_opts)
  target_compile_options(ap2_lib PRIVATE ${_ap2_opts})
endif()

get_target_property(_ap2_libs ${PROJECT_NAME} LINK_LIBRARIES)
if(_ap2_libs)
  target_link_libraries(ap2_lib PUBLIC ${_ap2_libs})
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(ap2_lib PROPERTIES EXCLUDE_FROM_ALL TRUE)

if(NOT TARGET ap2_default)
  add_custom_target(ap2_default ALL
                    DEPENDS $<IF:$<CONFIG:Debug>,${PROJECT_NAME},ap2_lib>)
endif()
