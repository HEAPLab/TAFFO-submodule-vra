#ifndef TAFFO_VRASTORE_HPP
#define TAFFO_VRASTORE_HPP

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Type.h"
#include <list>
#include <vector>

#include "RangeNode.hpp"

#define DEBUG_TYPE "taffo-vra"
#define DEBUG_HEAD "[TAFFO][VRA]"

namespace taffo {

class VRAStore {
public:
  virtual void convexMerge(const VRAStore &Other);

  virtual const generic_range_ptr_t fetchInfo(const llvm::Value* v, bool derived_or_final = false);
  virtual void saveValueInfo(const llvm::Value* v, const generic_range_ptr_t& info);
  virtual range_node_ptr_t getNode(const llvm::Value* v) const;
  virtual range_node_ptr_t getOrCreateNode(const llvm::Value* v);
  virtual generic_range_ptr_t fetchRange(const range_node_ptr_t node,
                                         std::list<std::vector<unsigned>>& offset) const;
  virtual void setRange(range_node_ptr_t node, const generic_range_ptr_t& info,
                        std::list<std::vector<unsigned>>& offset);
  virtual generic_range_ptr_t fetchConstant(const llvm::Constant* v);

  enum VRAStoreKind { VRASK_VRAGlobalStore, VRASK_ValueRangeAnalyzer };
  VRAStoreKind getKind() const { return Kind; }

protected:
  llvm::DenseMap<const llvm::Value*, range_node_ptr_t> DerivedRanges;

  bool extractGEPOffset(const llvm::Type* source_element_type,
                        const llvm::iterator_range<llvm::User::const_op_iterator> indices,
                        std::vector<unsigned>& offset);

  // Logging stuff
  static void emitError(const std::string& message);
  static std::string to_string(const generic_range_ptr_t& range);
  static void logInstruction(const llvm::Value* v);
  virtual void logRangeln(const llvm::Value* v);
  static void logRangeln(const generic_range_ptr_t& range);
  static void logInfo(const llvm::StringRef info);
  static void logInfoln(const llvm::StringRef info);
  static void logError(const llvm::StringRef error);

  VRAStore(VRAStoreKind K) : Kind(K), DerivedRanges() {}

private:
  const VRAStoreKind Kind;
};

} // end namespace taffo

#endif
