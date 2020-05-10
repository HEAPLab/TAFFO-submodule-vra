#include "VRAStore.hpp"

#include "llvm/Support/Debug.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/GlobalAlias.h"
#include "llvm/IR/GlobalIFunc.h"
#include "RangeOperations.hpp"

using namespace llvm;
using namespace taffo;

void
VRAStore::convexMerge(const VRAStore &Other) {
  for (auto &OValueRangeNode : Other.DerivedRanges) {
    const llvm::Value *V = OValueRangeNode.first;
    range_node_ptr_t ThisNode = this->getNode(V);
    if (ThisNode) {
      this->saveValueInfo(V, fetchInfo(V));
    } else {
      DerivedRanges[V] = OValueRangeNode.second;
    }
  }
}

const generic_range_ptr_t
VRAStore::fetchInfo(const llvm::Value* v) {
  if (const auto node = getNode(v)) {
    if (node->isScalar()) {
      return node->getRange();
    } else if (v->getType()->isPointerTy()) {
      std::list<std::vector<unsigned>> offset;
      return fetchRange(node, offset);
    }
  }

  const llvm::Constant* const_i = llvm::dyn_cast_or_null<llvm::Constant>(v);
  if (const_i) {
    const generic_range_ptr_t k = fetchConstant(const_i);
    saveValueInfo(v, k);
    return k;
  }
  // no info available
  return nullptr;
}

void
VRAStore::saveValueInfo(const llvm::Value* v,
                        const generic_range_ptr_t& info) {
  if (range_node_ptr_t node = getNode(v)) {
    if (!node->hasRange() && !node->hasParent()) {
      node->setRange(info);
      return;
    }

    std::list<std::vector<unsigned>> offset;
    const generic_range_ptr_t old = fetchRange(node, offset);
    if (old && old->isFinal())
      return;

    const generic_range_ptr_t updated = getUnionRange(old, info);

    offset.clear();
    setRange(node, updated, offset);
    return;
  }

  DerivedRanges[v] = make_range_node(info);
}

range_node_ptr_t
VRAStore::getNode(const llvm::Value* v) const {
  // TODO add assert(v) here and see what happens
  const auto it = DerivedRanges.find(v);
  if (it != DerivedRanges.end()) {
    return it->second;
  }

  return nullptr;
}

range_node_ptr_t
VRAStore::getOrCreateNode(const llvm::Value* v) {
  range_node_ptr_t ret = getNode(v);
  if (ret == nullptr) {
    if (isa<AllocaInst>(v)) {
      // create root node
      ret = make_range_node();
      DerivedRanges[v] = ret;
    } else if (const llvm::ConstantExpr* ce = dyn_cast<llvm::ConstantExpr>(v)) {
      if (ce->isGEPWithNoNotionalOverIndexing()) {
        llvm::Value* pointer_op = ce->getOperand(0U);
        llvm::Type* source_element_type =
          llvm::cast<llvm::PointerType>(pointer_op->getType()->getScalarType())->getElementType();
        std::vector<unsigned> offset;
        if (extractGEPOffset(source_element_type,
                             llvm::iterator_range<llvm::User::const_op_iterator>(ce->op_begin()+1,
                                                                                 ce->op_end()),
                             offset)) {
          ret = make_range_node(pointer_op, offset);
          DerivedRanges[v] = ret;
        }
      }
    }
  }
  return ret;
}

generic_range_ptr_t
VRAStore::fetchRange(const range_node_ptr_t node,
                     std::list<std::vector<unsigned>>& offset) const {
  if (!node) return nullptr;
  if (node->hasRange()) {
    if (node->isScalar()) {
      return node->getScalarRange();
    } else if (node->isStruct()) {
      range_s_ptr_t current = node->getStructRange();
      generic_range_ptr_t child = current;
      for (auto offset_it = offset.rbegin();
           offset_it != offset.rend(); ++offset_it) {
        for (unsigned idx : *offset_it) {
          child = current->getRangeAt(idx);
          current = std::dynamic_ptr_cast_or_null<VRA_Structured_Range>(child);
          if (!current)
            break;
        }
        if (!current)
          break;
      }
      return child;
    }
    // should have a range, but it is empty
    return nullptr;
  }
  // if this is not a range-node, lookup parent
  offset.push_back(node->getOffset());
  return fetchRange(getNode(node->getParent()), offset);
}

void
VRAStore::setRange(range_node_ptr_t node, const generic_range_ptr_t& info,
                   std::list<std::vector<unsigned>>& offset) {
  if (!node || !info) return;
  if (node->hasRange() || node->getParent() == nullptr) {
    if (node->isScalar()
        || (offset.empty() && std::isa_ptr<range_t>(info))) {
      const range_ptr_t scalar_info = std::static_ptr_cast<range_t>(info);
      node->setRange(scalar_info);
    } else {
      range_s_ptr_t child = node->getStructRange();
      if (child == nullptr) {
        child = make_s_range();
        node->setStructRange(child);
      }

      range_s_ptr_t parent = child;
      int child_idx = -1;
      for (auto offset_it = offset.rbegin();
           offset_it != offset.rend(); ++offset_it) {
        for (unsigned idx : *offset_it) {
          if (child_idx > -1) {
            generic_range_ptr_t gen_child = parent->getRangeAt(child_idx);
            if (gen_child == nullptr) {
              child = make_s_range();
              parent->setRangeAt(child_idx, child);
            } else {
              child = std::dynamic_ptr_cast<VRA_Structured_Range>(gen_child);
              if (child == nullptr)
                break;
            }
          }
          child_idx = idx;
          parent = child;
        }
      }
      if (child_idx == -1)
        node->setRange(info);
      else
        parent->setRangeAt(child_idx, info);
    }
  } else {
    // recursively call parent to update its structure
    offset.push_back(node->getOffset());
    setRange(getOrCreateNode(node->getParent()), info, offset);
  }
}

generic_range_ptr_t
VRAStore::fetchConstant(const llvm::Constant* kval) {
  const llvm::ConstantInt* int_i = dyn_cast<llvm::ConstantInt>(kval);
  if (int_i) {
    const num_t k = static_cast<num_t>(int_i->getSExtValue());
    return make_range(k, k);
  }
  const llvm::ConstantFP* fp_i = dyn_cast<llvm::ConstantFP>(kval);
  if (fp_i) {
    APFloat tmp = fp_i->getValueAPF();
    bool losesInfo;
    tmp.convert(APFloatBase::IEEEdouble(), APFloat::roundingMode::rmNearestTiesToEven, &losesInfo);
    const num_t k = static_cast<num_t>(tmp.convertToDouble());
    return make_range(k, k);
  }
  const llvm::ConstantTokenNone* none_i = dyn_cast<llvm::ConstantTokenNone>(kval);
  if (none_i) {
    LLVM_DEBUG(Logger->logInfo("Warning: treating llvm::ConstantTokenNone as 0"));
    return make_range(0, 0);
  }
  const llvm::ConstantPointerNull* null_i = dyn_cast<llvm::ConstantPointerNull>(kval);
  if (null_i) {
    LLVM_DEBUG(Logger->logInfo("Warning: treating llvm::ConstantPointerNull as 0"));
    return make_range(0, 0);
  }
  const llvm::UndefValue* undef_i = dyn_cast<llvm::UndefValue>(kval);
  if (undef_i) {
    LLVM_DEBUG(Logger->logInfo("Warning: treating llvm::UndefValue as nullptr"));
    return nullptr;
  }
  const llvm::ConstantAggregateZero* agg_zero_i = dyn_cast<llvm::ConstantAggregateZero>(kval);
  if (agg_zero_i) {
    llvm::Type* zero_type = agg_zero_i->getType();
    if (dyn_cast<llvm::StructType>(zero_type)) {
      std::vector<generic_range_ptr_t> s_range;
      const unsigned num_elements = agg_zero_i->getNumElements();
      s_range.reserve(num_elements);
      for (unsigned i = 0; i < num_elements; i++) {
        s_range.push_back(fetchConstant(agg_zero_i->getElementValue(i)));
      }
      return make_s_range(s_range);
    } else if (dyn_cast<llvm::SequentialType>(zero_type)) {
      // arrayType or VectorType
      const unsigned any_value = 0;
      return fetchConstant(agg_zero_i->getElementValue(any_value));
    }
    LLVM_DEBUG(Logger->logInfo("Found aggrated zeros which is neither struct neither array neither vector"));
    return nullptr;
  }
  const llvm::ConstantDataSequential* seq = dyn_cast<llvm::ConstantDataSequential>(kval);
  if (seq) {
    const unsigned num_elements = seq->getNumElements();
    const unsigned any_value = 0;
    generic_range_ptr_t seq_range = fetchConstant(seq->getElementAsConstant(any_value));
    for (unsigned i = 1; i < num_elements; i++) {
      generic_range_ptr_t other_range = fetchConstant(seq->getElementAsConstant(i));
      seq_range = getUnionRange(seq_range, other_range);
    }
    return seq_range;
  }
  const llvm::ConstantData* data = dyn_cast<llvm::ConstantData>(kval);
  if (data) {
    // FIXME should never happen -- all subcases handled before
    LLVM_DEBUG(Logger->logInfo("Extract value from llvm::ConstantData not implemented yet"));
    return nullptr;
  }
  if (const llvm::ConstantExpr* cexp_i = dyn_cast<llvm::ConstantExpr>(kval)) {
    if (cexp_i->isGEPWithNoNotionalOverIndexing()) {
      if (getOrCreateNode(cexp_i))
        return fetchInfo(cexp_i);
      else
        return nullptr;
    }
    LLVM_DEBUG(Logger->logInfo("Could not fold a llvm::ConstantExpr"));
    return nullptr;
  }
  const llvm::ConstantAggregate* aggr_i = dyn_cast<llvm::ConstantAggregate>(kval);
  if (aggr_i) {
    // TODO implement
    if (dyn_cast<llvm::ConstantStruct>(aggr_i)) {
      LLVM_DEBUG(Logger->logInfo("Constant structs not supported yet"));
      return nullptr;
    } else {
      // ConstantArray or ConstantVector
      const unsigned any_value = 0;
      return fetchConstant(aggr_i->getOperand(any_value));
    }
    return nullptr;
  }
  const llvm::BlockAddress* block_i = dyn_cast<llvm::BlockAddress>(kval);
  if (block_i) {
    LLVM_DEBUG(Logger->logInfo("Could not fetch range from llvm::BlockAddress"));
    return nullptr;
  }
  const llvm::GlobalValue* gv_i = dyn_cast<llvm::GlobalValue>(kval);
  if (gv_i) {
    const llvm::GlobalVariable* gvar_i = dyn_cast<llvm::GlobalVariable>(kval);
    if (gvar_i) {
      if (gvar_i->hasInitializer()) {
        const llvm::Constant* init_val = gvar_i->getInitializer();
        if (init_val) {
          return fetchConstant(init_val);
        }
      }
      LLVM_DEBUG(Logger->logInfo("Could not derive range from a Global Variable"));
      return nullptr;
    }
    const llvm::GlobalAlias* alias_i = dyn_cast<llvm::GlobalAlias>(kval);
    if (alias_i) {
      LLVM_DEBUG(Logger->logInfo("Found alias"));
      const llvm::Constant* aliasee = alias_i->getAliasee();
      return (aliasee) ? fetchConstant(aliasee) : nullptr;
    }
    const llvm::Function* f = dyn_cast<llvm::Function>(kval);
    if (f) {
      LLVM_DEBUG(Logger->logInfo("Could not derive range from a Constant Function"));
      return nullptr;
    }
    const llvm::GlobalIFunc* fun_decl = dyn_cast<llvm::GlobalIFunc>(kval);
    if (fun_decl) {
      LLVM_DEBUG(Logger->logInfo("Could not derive range from a Function declaration"));
      return nullptr;
    }
    // this line should never be reached
    LLVM_DEBUG(Logger->logInfo("Could not fetch range from llvm::GlobalValue"));
    return nullptr;
  }
  LLVM_DEBUG(Logger->logInfo("Could not fetch range from llvm::Constant"));
  return nullptr;
}

bool
VRAStore::extractGEPOffset(const llvm::Type* source_element_type,
                           const llvm::iterator_range<llvm::User::const_op_iterator> indices,
                           std::vector<unsigned>& offset) {
  assert(source_element_type != nullptr);
  LLVM_DEBUG(dbgs() << "indices: ");
  for (auto idx_it = indices.begin() + 1; // skip first index
       idx_it != indices.end(); ++idx_it) {
    if (isa<SequentialType>(source_element_type))
      continue;
    const llvm::ConstantInt* int_i = dyn_cast<llvm::ConstantInt>(*idx_it);
    if (int_i) {
      int n = static_cast<int>(int_i->getSExtValue());
      offset.push_back(n);
      source_element_type =
        cast<StructType>(source_element_type)->getTypeAtIndex(n);
      LLVM_DEBUG(dbgs() << n << " ");
    } else {
      LLVM_DEBUG(Logger->logErrorln("Index of GEP not constant"));
      return false;
    }
  }
  LLVM_DEBUG(dbgs() << "\n");
  return true;
}
