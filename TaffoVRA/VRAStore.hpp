#ifndef TAFFO_VRASTORE_HPP
#define TAFFO_VRASTORE_HPP

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Type.h"
#include <list>
#include <vector>

#include "VRALogger.hpp"
#include "RangeNode.hpp"

namespace taffo {

class VRAStore {
public:
  virtual void convexMerge(const VRAStore &Other);

  virtual const generic_range_ptr_t fetchInfo(const llvm::Value* v);
  virtual void saveValueInfo(const llvm::Value* v, const generic_range_ptr_t& info);
  virtual range_node_ptr_t getNode(const llvm::Value* v) const;
  virtual range_node_ptr_t getOrCreateNode(const llvm::Value* v);
  virtual generic_range_ptr_t fetchRange(const range_node_ptr_t node,
                                         std::list<std::vector<unsigned>>& offset) const;
  virtual void setRange(range_node_ptr_t node, const generic_range_ptr_t& info,
                        std::list<std::vector<unsigned>>& offset);
  virtual generic_range_ptr_t fetchConstant(const llvm::Constant* v);

  enum VRAStoreKind { VRASK_VRAGlobalStore, VRASK_VRAnalyzer };
  VRAStoreKind getKind() const { return Kind; }

protected:
  llvm::DenseMap<const llvm::Value*, range_node_ptr_t> DerivedRanges;
  std::shared_ptr<VRALogger> Logger;

  bool extractGEPOffset(const llvm::Type* source_element_type,
                        const llvm::iterator_range<llvm::User::const_op_iterator> indices,
                        std::vector<unsigned>& offset);

  VRAStore(VRAStoreKind K, std::shared_ptr<VRALogger> L)
    : Kind(K), DerivedRanges(), Logger(L) {}

private:
  const VRAStoreKind Kind;
};

} // end namespace taffo

#endif
