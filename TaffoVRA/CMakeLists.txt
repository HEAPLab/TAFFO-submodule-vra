add_llvm_loadable_module(TaffoVRA
  RangeOperations.cpp
  RangeOperations.hpp
  Range.hpp
  ValueRangeAnalysis.cpp
  ValueRangeAnalysis.hpp
  # TaffoInitializerPass.h
  # FixedPointType.cpp
  # FixedPointType.h
  # Annotations.cpp

  PLUGIN_TOOL
  opt
)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set_property(TARGET TaffoVRA APPEND_STRING PROPERTY
    LINK_FLAGS " -undefined dynamic_lookup")
endif()

set_property(TARGET TaffoVRA PROPERTY CXX_STANDARD 11)
set_property(TARGET TaffoVRA PROPERTY CXX_STANDARD_REQUIRED ON)

target_link_libraries(TaffoVRA MDUtils)
