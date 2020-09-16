add_llvm_library(TaffoVRA OBJECT BUILDTREE_ONLY
  RangeOperationsCallWhitelist.cpp
  RangeOperations.cpp
  ValueRangeAnalysis.cpp
  MemSSAUtils.cpp

  ADDITIONAL_HEADERS
  RangeOperationsCallWhitelist.hpp
  RangeOperations.hpp
  Range.hpp
  RangeNode.hpp
  ValueRangeAnalysis.hpp
  MemSSAUtils.hpp
)

set_property(TARGET obj.TaffoVRA PROPERTY POSITION_INDEPENDENT_CODE ON)
