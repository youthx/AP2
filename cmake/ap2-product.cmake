# Included at the end of CMakeLists via cmake_language(DEFER).
# Generates Debug-app / Release-lib rules. See ap2_product.py at the repo root.

if(DEFINED AP2_PRODUCT_APPLIED)
  return()
endif()
set(AP2_PRODUCT_APPLIED TRUE)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${CMAKE_CURRENT_SOURCE_DIR}/ap2_product.py")

set(_AP2_PRODUCT_GEN "${CMAKE_BINARY_DIR}/ap2-product-gen.cmake")

execute_process(
  COMMAND python "${CMAKE_CURRENT_SOURCE_DIR}/ap2_product.py"
          --emit-cmake
          --out "${_AP2_PRODUCT_GEN}"
  RESULT_VARIABLE _AP2_PRODUCT_PY
  ERROR_VARIABLE _AP2_PRODUCT_PY_ERR
  OUTPUT_VARIABLE _AP2_PRODUCT_PY_OUT
)

if(NOT _AP2_PRODUCT_PY EQUAL 0)
  message(FATAL_ERROR
    "AP2: ap2_product.py failed (${_AP2_PRODUCT_PY}):\n"
    "${_AP2_PRODUCT_PY_OUT}${_AP2_PRODUCT_PY_ERR}")
endif()

include("${_AP2_PRODUCT_GEN}")
