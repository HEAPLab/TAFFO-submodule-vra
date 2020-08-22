add_llvm_library(TaffoVRA OBJECT BUILDTREE_ONLY
  RangeOperationsCallWhitelist.cpp
  RangeOperations.cpp
  ValueRangeAnalysis.cpp
  MemSSAUtils.cpp
  CodeInterpreter.cpp
  VRAStore.cpp
  VRAnalyzer.cpp
  VRAGlobalStore.cpp
  VRAFunctionStore.cpp

  ADDITIONAL_HEADERS
  PtrCasts.hpp
  RangeOperationsCallWhitelist.hpp
  RangeOperations.hpp
  Range.hpp
  RangeNode.hpp
  ValueRangeAnalysis.hpp
  MemSSAUtils.hpp
  CodeInterpreter.hpp
  VRAStore.hpp
  VRAnalyzer.hpp
  VRAGlobalStore.hpp
  VRAFunctionStore.hpp
  VRALogger.hpp
)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set_property(TARGET obj.TaffoVRA APPEND_STRING PROPERTY
    LINK_FLAGS " -undefined dynamic_lookup")
endif()

set_property(TARGET obj.TaffoVRA PROPERTY CXX_STANDARD 11)
set_property(TARGET obj.TaffoVRA PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET obj.TaffoVRA PROPERTY POSITION_INDEPENDENT_CODE ON)
