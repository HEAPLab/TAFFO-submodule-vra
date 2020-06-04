#include "VRAGlobalStore.hpp"

#include "VRAnalyzer.hpp"
#include "RangeOperations.hpp"
#include <TypeUtils.h>

using namespace llvm;
using namespace taffo;

void
VRAGlobalStore::convexMerge(const AnalysisStore &Other) {
  // Since llvm::dyn_cast<T>() does not do cross-casting, we must do this:
  if (llvm::isa<VRAnalyzer>(Other)) {
    VRAStore::convexMerge(llvm::cast<VRAStore>(llvm::cast<VRAnalyzer>(Other)));
  } else if (llvm::isa<VRAGlobalStore>(Other)) {
    VRAStore::convexMerge(llvm::cast<VRAStore>(llvm::cast<VRAGlobalStore>(Other)));
  } else {
    VRAStore::convexMerge(llvm::cast<VRAStore>(llvm::cast<VRAFunctionStore>(Other)));
  }
}

std::shared_ptr<CodeAnalyzer>
VRAGlobalStore::newCodeAnalyzer(CodeInterpreter &CI) {
  return std::make_shared<VRAnalyzer>(CI);
}

std::shared_ptr<AnalysisStore>
VRAGlobalStore::newFunctionStore(CodeInterpreter &CI) {
  return std::make_shared<VRAFunctionStore>(CI);
}

////////////////////////////////////////////////////////////////////////////////
// Metadata Processing
////////////////////////////////////////////////////////////////////////////////

bool
VRAGlobalStore::isValidRange(const mdutils::Range *rng) const {
  return rng != nullptr && !std::isnan(rng->Min) && !std::isnan(rng->Max);
}

void
VRAGlobalStore::harvestMetadata(Module &M) {
  using namespace mdutils;
  MetadataManager &MDManager = MetadataManager::getMetadataManager();

  for (const llvm::GlobalVariable &v : M.globals()) {
    // retrieve info about global var v, if any
    const InputInfo *II = MDManager.retrieveInputInfo(v);
    if (II && isValidRange(II->IRange.get())) {
      auto SN = std::make_shared<VRAScalarNode>(
                  make_range(II->IRange->Min, II->IRange->Max, II->isFinal()));
      //UserInput[&v] = SN;
      // TODO see if we can avoid this:
      DerivedRanges[&v] = std::make_shared<VRAPtrNode>(SN);
    } else if (const StructInfo *SI = MDManager.retrieveStructInfo(v)) {
      UserInput[&v] = std::static_ptr_cast<VRARangeNode>(
                        harvestStructMD(SI, fullyUnwrapPointerOrArrayType(v.getType())));
      // TODO see if we can avoid this, avoid lookup otherwise:
      DerivedRanges[&v] = UserInput[&v];
    } else if (v.getValueType()->isStructTy()) {
      DerivedRanges[&v] = std::make_shared<VRAStructNode>();
    } else {
      DerivedRanges[&v] = std::make_shared<VRAPtrNode>();
    }
  }

  for (llvm::Function &f : M.functions()) {
    if (f.empty())
      continue;

    // retrieve info about function parameters
    SmallVector<mdutils::MDInfo*, 5U> argsII;
    MDManager.retrieveArgumentInputInfo(f, argsII);
    SmallVector<int, 5U> argsW;
    MetadataManager::retrieveInputInfoInitWeightMetadata(&f, argsW);
    auto itII = argsII.begin();
    auto itW = argsW.begin();
    for (const Argument &formal_arg : f.args()) {
      if (itII == argsII.end())
        break;
      if (itW != argsW.end()) {
        if (*itW == 1U) {
          if (auto UINode = harvestStructMD(*itII,
                fullyUnwrapPointerOrArrayType(formal_arg.getType()))) {
            UserInput[&formal_arg] = std::static_ptr_cast<VRARangeNode>(UINode);
          }
        }
        ++itW;
      } else {
        if (auto UINode = harvestStructMD(*itII,
              fullyUnwrapPointerOrArrayType(formal_arg.getType()))) {
          UserInput[&formal_arg] = std::static_ptr_cast<VRARangeNode>(UINode);
        }
      }
      ++itII;
    }

    // retrieve info about instructions, for each basic block bb
    for (const llvm::BasicBlock &bb : f.getBasicBlockList()) {
      for (const llvm::Instruction &i : bb.getInstList()) {
        // fetch info about Instruction i
        MDInfo *MDI = MDManager.retrieveMDInfo(&i);
        if (!MDI)
          continue;
        // only retain info of instruction i if its weight is lesser than
        // the weight of all of its parents
        int weight = MDManager.retrieveInputInfoInitWeightMetadata(&i);
        bool root = true;
        if (weight > 0) {
          if (isa<AllocaInst>(i)) {
            // Kludge for alloca not to be always roots
            root = false;
          }
          for (auto& v: i.operands()) {
            int parentWeight = -1;
            if (Argument* a = dyn_cast<Argument>(v.get())) {
              parentWeight = argsW[a->getArgNo()];
            } else if (isa<Instruction>(v.get()) || isa<GlobalVariable>(v.get())) {
              parentWeight = MDManager.retrieveInputInfoInitWeightMetadata(v.get());
            } else {
              continue;
            }
            // only consider parameters with the same metadata
            MDInfo *MDIV = MDManager.retrieveMDInfo(v.get());
            if (MDIV != MDI)
              continue;
            if (parentWeight < weight) {
              root = false;
              break;
            }
          }
        }
        if (!root)
          continue;
        LLVM_DEBUG(Logger->lineHead();
                   dbgs() << " Considering input metadata of " << i << " (weight=" << weight << ")\n");
        if (InputInfo *II = dyn_cast<InputInfo>(MDI)) {
          if (isValidRange(II->IRange.get())) {
            const llvm::Value* i_ptr = &i;
            UserInput[i_ptr] = std::make_shared<VRAScalarNode>(
                                 make_range(II->IRange->Min, II->IRange->Max, II->isFinal()));
          }
        } else if (StructInfo *SI = dyn_cast<StructInfo>(MDI)) {
          const llvm::Value* i_ptr = &i;
          UserInput[i_ptr] = std::static_ptr_cast<VRARangeNode>(
                               harvestStructMD(SI, fullyUnwrapPointerOrArrayType(i_ptr->getType())));
        }
      }
    }

  } // end iteration over Function in Module
  return;
}

NodePtrT
VRAGlobalStore::harvestStructMD(const mdutils::MDInfo *MD, const llvm::Type *T) {
  using namespace mdutils;
  if (MD == nullptr) {
    return nullptr;

  } else if (T->isArrayTy()) {
    return harvestStructMD(MD, T->getArrayElementType());

  } else if (T->isPointerTy() && !T->getPointerElementType()->isStructTy()) {
    return std::make_shared<VRAPtrNode>(harvestStructMD(MD, T->getPointerElementType()));

  } else if (const InputInfo *II = llvm::dyn_cast<InputInfo>(MD)) {
    if (isValidRange(II->IRange.get()))
      return std::make_shared<VRAScalarNode>(
               make_range(II->IRange->Min, II->IRange->Max, II->isFinal()));
    else
      return nullptr;

  } else if (const StructInfo *SI = llvm::dyn_cast<StructInfo>(MD)) {
    if (T->isPointerTy())
      T = T->getPointerElementType();
    assert(T->isStructTy());

    llvm::SmallVector<NodePtrT, 4U> Fields;
    unsigned Idx = 0;
    for (auto it = SI->begin(); it != SI->end(); it++) {
      Fields.push_back(harvestStructMD(it->get(), T->getStructElementType(Idx)));
      ++Idx;
    }

    return std::make_shared<VRAStructNode>(Fields);
  }

  llvm_unreachable("unknown type of MDInfo");
}

void
VRAGlobalStore::saveResults(llvm::Module &M) {
  using namespace mdutils;
  MetadataManager &MDManager = MetadataManager::getMetadataManager();
  for (llvm::GlobalVariable &v : M.globals()) {
    const RangeNodePtrT range = fetchRangeNode(&v);
    if (range) {
      // retrieve info about global var v, if any
      if (MDInfo *mdi = MDManager.retrieveMDInfo(&v)) {
        std::shared_ptr<MDInfo> cpymdi(mdi->clone());
        updateMDInfo(cpymdi, range);
        MDManager.setMDInfoMetadata(&v, cpymdi.get());
      } else {
        std::shared_ptr<MDInfo> newmdi = toMDInfo(range);
        MDManager.setMDInfoMetadata(&v, newmdi.get());
      }
    }
  } // end globals

  for (Function &f : M.functions()) {
    // arg range
    SmallVector<MDInfo*, 5U> argsII;
    MDManager.retrieveArgumentInputInfo(f, argsII);
    if (argsII.size() == f.getFunctionType()->getNumParams()) {
      /* argsII.size() != f.getNumOperands() when there was no metadata
       * on the arguments in the first place */
      SmallVector<std::shared_ptr<MDInfo>, 5U> newII;
      newII.reserve(f.arg_size());
      auto argsIt = argsII.begin();
      for (Argument &arg : f.args()) {
        if (const RangeNodePtrT range = fetchRangeNode(&arg)) {
          if (argsIt != argsII.end() && *argsIt != nullptr) {
            std::shared_ptr<MDInfo> cpymdi((*argsIt)->clone());
            updateMDInfo(cpymdi, range);
            newII.push_back(cpymdi);
            *argsIt = cpymdi.get();
          } else {
            std::shared_ptr<MDInfo> newmdi = toMDInfo(range);
            newII.push_back(newmdi);
            *argsIt = newmdi.get();
          }
        }
        ++argsIt;
      }
      MDManager.setArgumentInputInfoMetadata(f, argsII);
    }

    // retrieve info about instructions, for each basic block bb
    for (BasicBlock &bb : f.getBasicBlockList()) {
      for (Instruction &i : bb.getInstList()) {
        setConstRangeMetadata(MDManager, i);
        if (i.getOpcode() == llvm::Instruction::Store)
          continue;
        // TODO remove if useless:
        // refreshRange(&i);
        if (const RangeNodePtrT range = fetchRangeNode(&i)) {
          if (MDInfo *mdi = MDManager.retrieveMDInfo(&i)) {
            std::shared_ptr<MDInfo> cpymdi(mdi->clone());
            updateMDInfo(cpymdi, range);
            MDManager.setMDInfoMetadata(&i, cpymdi.get());
          } else if (std::shared_ptr<MDInfo> newmdi = toMDInfo(range)) {
            if (std::isa_ptr<InputInfo>(newmdi)
                || fullyUnwrapPointerOrArrayType(i.getType())->isStructTy()) {
              MDManager.setMDInfoMetadata(&i, newmdi.get());
            }
          }
        }
      } // end instruction
    } // end bb
  } // end function
  return;
}

// TODO remove if useless:
/*
void
VRAGlobalStore::refreshRange(const llvm::Instruction* i) {
  if (const LoadInst* li = dyn_cast_or_null<LoadInst>(i)) {
    const generic_range_ptr_t newInfo = fetchInfo(li->getPointerOperand());
    saveValueInfo(li, newInfo);
  }
}
*/

std::shared_ptr<mdutils::MDInfo>
VRAGlobalStore::toMDInfo(const RangeNodePtrT r) {
  using namespace mdutils;
  if (r == nullptr) return nullptr;
  if (const std::shared_ptr<VRAScalarNode> Scalar =
      std::dynamic_ptr_cast<VRAScalarNode>(r)) {
    if (range_ptr_t SRange = Scalar->getRange()) {
      return std::make_shared<InputInfo>(nullptr,
                                         std::make_shared<Range>(SRange->min(), SRange->max()),
                                         nullptr);
    }
    return nullptr;
  } else if (const std::shared_ptr<VRAStructNode> structr =
             std::dynamic_ptr_cast<VRAStructNode>(r)) {
    SmallVector<std::shared_ptr<mdutils::MDInfo>, 4U> fields;
    fields.reserve(structr->fields().size());
    for (const NodePtrT f : structr->fields()) {
      fields.push_back(toMDInfo(fetchRange(f)));
    }
    return std::make_shared<StructInfo>(fields);
  }
  llvm_unreachable("Unknown range type.");
}

void
VRAGlobalStore::updateMDInfo(std::shared_ptr<mdutils::MDInfo> mdi,
                             const RangeNodePtrT r) {
  using namespace mdutils;
  if (mdi == nullptr || r == nullptr) return;
  if (const std::shared_ptr<VRAScalarNode> Scalar =
      std::dynamic_ptr_cast<VRAScalarNode>(r)) {
    if (range_ptr_t SRange = Scalar->getRange()) {
      std::shared_ptr<InputInfo> ii = std::static_ptr_cast<InputInfo>(mdi);
      ii->IRange.reset(new Range(SRange->min(), SRange->max()));
    }
  } else if (const std::shared_ptr<VRAStructNode> structr =
             std::dynamic_ptr_cast<VRAStructNode>(r)) {
    if (std::shared_ptr<StructInfo> si = std::dynamic_ptr_cast<StructInfo>(mdi)) {
      auto derfield_it = structr->fields().begin();
      auto derfield_end = structr->fields().end();
      for (StructInfo::size_type i = 0; i < si->size(); ++i) {
        if (derfield_it == derfield_end) break;
        std::shared_ptr<mdutils::MDInfo> mdfield = si->getField(i);
        if (mdfield)
          updateMDInfo(mdfield, fetchRange(*derfield_it));
        else
          si->setField(i, toMDInfo(fetchRange(*derfield_it)));

        ++derfield_it;
      }
    }
  } else {
    llvm_unreachable("Unknown range type.");
  }
}

void
VRAGlobalStore::setConstRangeMetadata(mdutils::MetadataManager &MDManager,
                                      llvm::Instruction &i) {
  using namespace mdutils;
  unsigned opCode = i.getOpcode();
  if (!(i.isBinaryOp() || i.isUnaryOp() || opCode == Instruction::Store
        || opCode == Instruction::Call || opCode == Instruction::Invoke))
    return;

  bool hasConstOp = false;
  for (const Value *op : i.operands()) {
    if (isa<Constant>(op)) {
      hasConstOp = true;
      break;
    }
  }

  SmallVector<std::unique_ptr<InputInfo>, 2U> CInfoPtr;
  SmallVector<InputInfo *, 2U> CInfo;
  CInfo.reserve(i.getNumOperands());
  if (hasConstOp) {
    for (const Value *op : i.operands()) {
      if (const ConstantFP *c = dyn_cast<ConstantFP>(op)) {
        APFloat apf = c->getValueAPF();
        bool discard;
        apf.convert(APFloat::IEEEdouble(), APFloat::rmNearestTiesToAway, &discard);
        double value = apf.convertToDouble();
        CInfoPtr.push_back(std::unique_ptr<InputInfo>(new InputInfo(nullptr,
                                                                    std::make_shared<Range>(value, value),
                                                                    nullptr)));
        CInfo.push_back(CInfoPtr.back().get());
      } else {
        CInfo.push_back(nullptr);
      }
    }
    MDManager.setConstInfoMetadata(i, CInfo);
  }
}


const range_ptr_t
VRAGlobalStore::fetchRange(const llvm::Value *V) {
  if (const range_ptr_t Derived = VRAStore::fetchRange(V)) {
    return Derived;
  }

  if (const RangeNodePtrT InputRange = getUserInput(V)) {
    saveValueRange(V, InputRange);
    if (const std::shared_ptr<VRAScalarNode> InputScalar =
        std::dynamic_ptr_cast<VRAScalarNode>(InputRange)) {
      return InputScalar->getRange();
    }
  }

  return nullptr;
}

const RangeNodePtrT
VRAGlobalStore::fetchRangeNode(const llvm::Value* V) {
  if (const RangeNodePtrT Derived = VRAStore::fetchRangeNode(V)) {
    if (std::isa_ptr<VRAStructNode>(Derived)) {
      if (RangeNodePtrT InputRange = getUserInput(V)) {
        // fill null input_range fields with corresponding derived fields
        return fillRangeHoles(Derived, InputRange);
      }
    }
    return Derived;
  }

  if (const RangeNodePtrT InputRange = getUserInput(V)) {
    // Save it in this store, so we don't overwrite it if it's final.
    saveValueRange(V, InputRange);
    return InputRange;
  }

  return nullptr;
}

RangeNodePtrT
VRAGlobalStore::getUserInput(const llvm::Value *V) const {
  auto UIt = UserInput.find(V);
  if (UIt != UserInput.end()) {
    return UIt->second;
  }
  return nullptr;
}
