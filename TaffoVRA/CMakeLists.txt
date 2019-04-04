add_llvm_loadable_module(TaffoVRA
  RangeOperationsCallWhitelist.cpp
  RangeOperationsCallWhitelist.hpp
  RangeOperations.cpp
  RangeOperations.hpp
  Range.hpp
  RangeNode.hpp
  ValueRangeAnalysis.cpp
  ValueRangeAnalysis.hpp
  MemSSAUtils.cpp
  MemSSAUtils.hpp

  PLUGIN_TOOL
  opt
)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set_property(TARGET TaffoVRA APPEND_STRING PROPERTY
    LINK_FLAGS " -undefined dynamic_lookup")
endif()

set_property(TARGET TaffoVRA PROPERTY CXX_STANDARD 11)
set_property(TARGET TaffoVRA PROPERTY CXX_STANDARD_REQUIRED ON)

target_link_libraries(TaffoVRA TaffoUtils m stdc++)
