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
    NodePtrT ThisNode = this->getNode(V);
    if (ThisNode) {
      if (std::isa_ptr<VRAStructNode>(ThisNode))
        assignStructNode(ThisNode, OValueRangeNode.second);
      else if (NodePtrT Union = assignScalarRange(ThisNode, OValueRangeNode.second)) {
        DerivedRanges[V] = Union;
      }
    } else {
      DerivedRanges[V] = OValueRangeNode.second;
    }
  }
}

const range_ptr_t
VRAStore::fetchRange(const llvm::Value *V) {
  if (const std::shared_ptr<VRAScalarNode> Ret =
      std::dynamic_ptr_cast_or_null<VRAScalarNode>(fetchRangeNode(V)))
    return Ret->getRange();
  return nullptr;
}

const RangeNodePtrT
VRAStore::fetchRangeNode(const llvm::Value* v) {
  if (const NodePtrT Node = getNode(v)) {
    if (std::shared_ptr<VRAScalarNode> Scalar =
        std::dynamic_ptr_cast<VRAScalarNode>(Node)) {
      return Scalar;
    } else if (v->getType()->isPointerTy()) {
      return fetchRange(Node);
    }
  }

  // no info available
  return nullptr;
}

void
VRAStore::saveValueRange(const llvm::Value* v,
                         const range_ptr_t Range) {
  // TODO: make specialized version of this to avoid creating useless node
  saveValueRange(v, std::make_shared<VRAScalarNode>(Range));
}

void
VRAStore::saveValueRange(const llvm::Value* v,
                         const RangeNodePtrT Range) {
  assert(v && "Trying to save range for null value.");
  if (NodePtrT Union = assignScalarRange(getNode(v), Range)) {
    DerivedRanges[v] = Union;
    return;
  }
  DerivedRanges[v] = Range;
}

NodePtrT
VRAStore::getNode(const llvm::Value* v) {
  assert(v && "Trying to get node for null value.");
  const auto it = DerivedRanges.find(v);
  if (it != DerivedRanges.end()) {
    return it->second;
  }

  if (!v->getType()->isPointerTy()) {
    if (const llvm::Constant* Const = llvm::dyn_cast_or_null<llvm::Constant>(v)) {
      NodePtrT K = fetchConstant(Const);
      DerivedRanges[v] = K;
      return K;
    }
  }

  return nullptr;
}

void
VRAStore::setNode(const llvm::Value* V, NodePtrT Node) {
  DerivedRanges[V] = Node;
}

NodePtrT
VRAStore::loadNode(const NodePtrT Node) const {
  llvm::SmallVector<unsigned, 1U> Offset;
  return loadNode(Node, Offset);
}

NodePtrT
VRAStore::loadNode(const NodePtrT Node,
                   llvm::SmallVectorImpl<unsigned>& Offset) const {
  if (!Node) return nullptr;
  switch (Node->getKind()) {
    case VRANode::VRAScalarNodeK:
      return Node;
    case VRANode::VRAStructNodeK:
      if (Offset.empty()) {
        return Node;
      } else {
        std::shared_ptr<VRAStructNode> StructNode =
          std::static_ptr_cast<VRAStructNode>(Node);
        NodePtrT Field = StructNode->getNodeAt(Offset.back());
        Offset.pop_back();
        if (Offset.empty())
          return Field;
        else
          return loadNode(Field, Offset);
      }
    case VRANode::VRAGEPNodeK: {
      std::shared_ptr<VRAGEPNode> GEPNode =
        std::static_ptr_cast<VRAGEPNode>(Node);
      const llvm::ArrayRef<unsigned> GEPOffset = GEPNode->getOffset();
      Offset.append(GEPOffset.begin(), GEPOffset.end());
      return loadNode(GEPNode->getParent(), Offset);
    }
    case VRANode::VRAPtrNodeK: {
      std::shared_ptr<VRAPtrNode> PtrNode =
        std::static_ptr_cast<VRAPtrNode>(Node);
      return PtrNode->getParent();
    }
    default:
      llvm_unreachable("Unhandled node type.");
  }
}

std::shared_ptr<VRAScalarNode>
VRAStore::assignScalarRange(NodePtrT Dst, const NodePtrT Src) const {
  std::shared_ptr<VRAScalarNode> ScalarDst =
    std::dynamic_ptr_cast_or_null<VRAScalarNode>(Dst);
  const std::shared_ptr<VRAScalarNode> ScalarSrc =
    std::dynamic_ptr_cast_or_null<VRAScalarNode>(Src);
  if (!(ScalarDst && ScalarSrc))
    return nullptr;

  if (ScalarDst->isFinal())
    return ScalarDst;

  range_ptr_t Union = getUnionRange(ScalarDst->getRange(), ScalarSrc->getRange());
  return std::make_shared<VRAScalarNode>(Union);
}

void
VRAStore::assignStructNode(NodePtrT Dst, const NodePtrT Src) const {
  std::shared_ptr<VRAStructNode> StructDst =
    std::dynamic_ptr_cast_or_null<VRAStructNode>(Dst);
  const std::shared_ptr<VRAStructNode> StructSrc =
    std::dynamic_ptr_cast_or_null<VRAStructNode>(Src);
  if (!(StructDst && StructSrc))
    return;

  const llvm::ArrayRef<NodePtrT> SrcFields = StructSrc->fields();
  for (unsigned Idx = 0; Idx < SrcFields.size(); ++Idx) {
    NodePtrT SrcField = SrcFields[Idx];
    if (!SrcField)
      continue;
    NodePtrT DstField = StructDst->getNodeAt(Idx);
    if (!DstField) {
      StructDst->setNodeAt(Idx, SrcField);
    } else {
      if (std::isa_ptr<VRAStructNode>(DstField)) {
        assignStructNode(DstField, SrcField);
      } else if (NodePtrT Union = assignScalarRange(DstField, SrcField)) {
        StructDst->setNodeAt(Idx, Union);
      }
    }
  }
}

void
VRAStore::storeNode(NodePtrT Dst, const NodePtrT Src) {
  llvm::SmallVector<unsigned, 1U> Offset;
  storeNode(Dst, Src, Offset);
}

void
VRAStore::storeNode(NodePtrT Dst, const NodePtrT Src,
                    llvm::SmallVectorImpl<unsigned>& Offset) {
  if (!(Dst && Src)) return;
  NodePtrT Pointed = nullptr;
  switch (Dst->getKind()) {
    case VRANode::VRAGEPNodeK: {
      std::shared_ptr<VRAGEPNode> GEPNode =
        std::static_ptr_cast<VRAGEPNode>(Dst);
      const llvm::ArrayRef<unsigned> GEPOffset = GEPNode->getOffset();
      Offset.append(GEPOffset.begin(), GEPOffset.end());
      storeNode(GEPNode->getParent(), Src, Offset);
      break;
    }
    case VRANode::VRAStructNodeK: {
        std::shared_ptr<VRAStructNode> StructDst =
          std::static_ptr_cast<VRAStructNode>(Dst);
      if (Offset.empty()) {
        assignStructNode(StructDst, Src);
      } else if (Offset.size() == 1U) {
        unsigned Idx = Offset.front();
        NodePtrT Union = assignScalarRange(StructDst->getNodeAt(Idx), Src);
        if (Union) {
          StructDst->setNodeAt(Idx, Union);
        } else {
          StructDst->setNodeAt(Idx, Src);
        }
      } else {
        NodePtrT Field = StructDst->getNodeAt(Offset.back());
        if (!Field) {
          Field = std::make_shared<VRAStructNode>();
          StructDst->setNodeAt(Offset.back(), Field);
        }
        Offset.pop_back();
        storeNode(Field, Src, Offset);
      }
      break;
    }
    case VRANode::VRAPtrNodeK: {
      std::shared_ptr<VRAPtrNode> PtrDst =
        std::static_ptr_cast<VRAPtrNode>(Dst);
      NodePtrT Union = assignScalarRange(PtrDst->getParent(), Src);
      if (Union) {
        PtrDst->setParent(Union);
      } else {
        PtrDst->setParent(Src);
      }
      break;
    }
    default:
      llvm_unreachable("Trying to store into a non-pointer node.");
  }
}

RangeNodePtrT
VRAStore::fetchRange(const NodePtrT Node) const {
  llvm::SmallVector<unsigned, 1U> Offset;
  return fetchRange(Node, Offset);
}

RangeNodePtrT
VRAStore::fetchRange(const NodePtrT Node,
                     llvm::SmallVectorImpl<unsigned>& Offset) const {
  if (!Node) return nullptr;
  switch (Node->getKind()) {
    case VRANode::VRAScalarNodeK:
      return std::static_ptr_cast<VRAScalarNode>(Node);
    case VRANode::VRAStructNodeK: {
      std::shared_ptr<VRAStructNode> StructNode =
        std::static_ptr_cast<VRAStructNode>(Node);
      if (Offset.empty()) {
        return StructNode;
      } else {
        NodePtrT Field = StructNode->getNodeAt(Offset.back());
        Offset.pop_back();
        return fetchRange(Field, Offset);
      }
    }
    case VRANode::VRAGEPNodeK: {
      std::shared_ptr<VRAGEPNode> GEPNode =
        std::dynamic_ptr_cast<VRAGEPNode>(Node);
      const llvm::ArrayRef<unsigned> GEPOffset = GEPNode->getOffset();
      Offset.append(GEPOffset.begin(), GEPOffset.end());
      return fetchRange(GEPNode->getParent(), Offset);
    }
    case VRANode::VRAPtrNodeK: {
      std::shared_ptr<VRAPtrNode> PtrNode =
        std::dynamic_ptr_cast<VRAPtrNode>(Node);
      return fetchRange(PtrNode->getParent(), Offset);
    }
    default:
      llvm_unreachable("Unhandled node type.");
  }
}

// TODO: delete
/*
void
VRAStore::setRange(NodePtrT Node, const RangeNodePtrT& Range,
                   llvm::SmallVectorImpl<unsigned>& Offset) {
  if (!Node || !Range) return;
  switch (Node->getKind()) {
    case VRAScalarNodeK: {
      std::shared_ptr<VRAScalarNode> ScalarNode =
          std::static_ptr_cast<VRAScalarNode>(Node);
      const std::shared_ptr<VRAScalarNode> ScalarRange =
          std::static_ptr_cast<VRAScalarNode>(Range);
      ScalarNode->setRange(ScalarRange->getRange());
    }
    case VRAStructNodeK: {
      std::shared_ptr<VRAStructNode> StructNode =
        std::static_ptr_cast<VRAStructNode>(Node);
      if (Offset.empty()) {
        std::shared_ptr<VRAStructNode> StructRange =
          std::static_ptr_cast<VRAStructNode>(Range);
        StructNode->copyFields(StructRange);
      } else {
        NodePtrT Field = StructNode->getNodeAt(Offset.back());
        Offset.pop_back();
        return fetchRange(Field, Offset);
      }
    case VRAGEPNodeK: {
      std::shared_ptr<VRAGEPNode> GEPNode =
        std::dynamic_ptr_cast<VRAGEPNode>(Node);
      Offset.append(GEPNode->getOffset());
      return fetchRange(GEPNode->getParent(), Offset);
    }
    case VRAPtrNodeK: {
      std::shared_ptr<VRAPtrNode> PtrNode =
        std::dynamic_ptr_cast<VRAPtrNode>(Node);
      return fetchRange(PtrNode->getParent(), Offset);
    }
    default:
      llvm_unreachable("Unhandled node type.");
  }
}
*/

NodePtrT
VRAStore::fetchConstant(const llvm::Constant* kval) {
  const llvm::ConstantInt* int_i = dyn_cast<llvm::ConstantInt>(kval);
  if (int_i) {
    const num_t k = static_cast<num_t>(int_i->getSExtValue());
    return std::make_shared<VRAScalarNode>(make_range(k, k));
  }
  const llvm::ConstantFP* fp_i = dyn_cast<llvm::ConstantFP>(kval);
  if (fp_i) {
    APFloat tmp = fp_i->getValueAPF();
    bool losesInfo;
    tmp.convert(APFloatBase::IEEEdouble(), APFloat::roundingMode::rmNearestTiesToEven, &losesInfo);
    const num_t k = static_cast<num_t>(tmp.convertToDouble());
    return std::make_shared<VRAScalarNode>(make_range(k, k));
  }
  const llvm::ConstantTokenNone* none_i = dyn_cast<llvm::ConstantTokenNone>(kval);
  if (none_i) {
    LLVM_DEBUG(Logger->logInfo("Warning: treating llvm::ConstantTokenNone as 0"));
    return std::make_shared<VRAScalarNode>(make_range(0, 0));
  }
  const llvm::ConstantPointerNull* null_i = dyn_cast<llvm::ConstantPointerNull>(kval);
  if (null_i) {
    LLVM_DEBUG(Logger->logInfo("Warning: treating llvm::ConstantPointerNull as 0"));
    return std::make_shared<VRAScalarNode>(make_range(0, 0));
  }
  const llvm::UndefValue* undef_i = dyn_cast<llvm::UndefValue>(kval);
  if (undef_i) {
    LLVM_DEBUG(Logger->logInfo("Warning: treating llvm::UndefValue as nullptr"));
    return nullptr;
  }
  const llvm::ConstantAggregateZero* agg_zero_i = dyn_cast<llvm::ConstantAggregateZero>(kval);
  if (agg_zero_i) {
    llvm::Type* zero_type = agg_zero_i->getType();
    if (llvm::dyn_cast<llvm::StructType>(zero_type)) {
      llvm::SmallVector<NodePtrT, 1U> Fields;
      const unsigned num_elements = agg_zero_i->getNumElements();
      Fields.reserve(num_elements);
      for (unsigned i = 0; i < num_elements; i++) {
        Fields.push_back(fetchConstant(agg_zero_i->getElementValue(i)));
      }
      return std::make_shared<VRAStructNode>(Fields);
    } else if (dyn_cast<llvm::SequentialType>(zero_type)) {
      // arrayType or VectorType
      const unsigned any_value = 0U;
      return fetchConstant(agg_zero_i->getElementValue(any_value));
    }
    LLVM_DEBUG(Logger->logInfo("Found aggrated zeros which is neither struct neither array neither vector"));
    return nullptr;
  }
  if (const llvm::ConstantDataSequential* seq =
      dyn_cast<llvm::ConstantDataSequential>(kval)) {
    const unsigned num_elements = seq->getNumElements();
    range_ptr_t seq_range = nullptr;
    for (unsigned i = 0; i < num_elements; i++) {
      range_ptr_t other_range =
        std::static_ptr_cast<VRAScalarNode>(fetchConstant(seq->getElementAsConstant(i)))->getRange();
      seq_range = getUnionRange(seq_range, other_range);
    }
    return std::make_shared<VRAScalarNode>(seq_range);
  }
  if (const llvm::ConstantData* data = dyn_cast<llvm::ConstantData>(kval)) {
    // FIXME should never happen -- all subcases handled before
    LLVM_DEBUG(Logger->logInfo("Extract value from llvm::ConstantData not implemented yet"));
    return nullptr;
  }
  if (const llvm::ConstantExpr* cexp_i = dyn_cast<llvm::ConstantExpr>(kval)) {
    if (cexp_i->isGEPWithNoNotionalOverIndexing()) {
      llvm::Value *pointer_op = cexp_i->getOperand(0U);
      llvm::Type *source_element_type =
        llvm::cast<llvm::PointerType>(pointer_op->getType()->getScalarType())->getElementType();
      llvm::SmallVector<unsigned, 1U> offset;
      if (extractGEPOffset(source_element_type,
                           llvm::iterator_range<llvm::User::const_op_iterator>(cexp_i->op_begin()+1,
                                                                               cexp_i->op_end()),
                           offset)) {
        return std::make_shared<VRAGEPNode>(getNode(pointer_op), offset);
      }
    }
    LLVM_DEBUG(Logger->logInfo("Could not fold a llvm::ConstantExpr"));
    return nullptr;
  }
  if (const llvm::ConstantAggregate* aggr_i = dyn_cast<llvm::ConstantAggregate>(kval)) {
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
  if (const llvm::BlockAddress* block_i = dyn_cast<llvm::BlockAddress>(kval)) {
    LLVM_DEBUG(Logger->logInfo("Could not fetch range from llvm::BlockAddress"));
    return nullptr;
  }
  if (const llvm::GlobalValue* gv_i = dyn_cast<llvm::GlobalValue>(kval)) {
    if (const llvm::GlobalVariable* gvar_i = dyn_cast<llvm::GlobalVariable>(kval)) {
      if (gvar_i->hasInitializer()) {
        const llvm::Constant* init_val = gvar_i->getInitializer();
        if (init_val) {
          return fetchConstant(init_val);
        }
      }
      LLVM_DEBUG(Logger->logInfo("Could not derive range from a Global Variable"));
      return nullptr;
    }
    if (const llvm::GlobalAlias* alias_i = dyn_cast<llvm::GlobalAlias>(kval)) {
      LLVM_DEBUG(Logger->logInfo("Found alias"));
      const llvm::Constant* aliasee = alias_i->getAliasee();
      return (aliasee) ? fetchConstant(aliasee) : nullptr;
    }
    if (const llvm::Function* f = dyn_cast<llvm::Function>(kval)) {
      LLVM_DEBUG(Logger->logInfo("Could not derive range from a Constant Function"));
      return nullptr;
    }
    if (const llvm::GlobalIFunc* fun_decl = dyn_cast<llvm::GlobalIFunc>(kval)) {
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
                           llvm::SmallVectorImpl<unsigned>& offset) {
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
