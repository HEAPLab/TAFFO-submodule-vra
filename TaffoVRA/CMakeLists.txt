add_llvm_library(TaffoVRA MODULE
  PtrCasts.hpp
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
  CodeInterpreter.cpp
  CodeInterpreter.hpp
  VRAStore.cpp
  VRAStore.hpp
  VRAnalyzer.cpp
  VRAnalyzer.hpp
  VRAGlobalStore.cpp
  VRAGlobalStore.hpp
  VRAFunctionStore.cpp
  VRAFunctionStore.hpp
  VRALogger.hpp

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
