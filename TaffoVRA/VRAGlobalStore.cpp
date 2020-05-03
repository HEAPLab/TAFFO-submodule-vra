#include "VRAGlobalStore.hpp"

#include "ValueRangeAnalyzer.hpp"
#include "RangeOperations.hpp"

using namespace llvm;
using namespace taffo;

void
VRAGlobalStore::convexMerge(const AnalysisStore &Other) {
  if (isa<ValueRangeAnalyzer>(Other)) {
    VRAStore::convexMerge(llvm::cast<VRAStore>(llvm::cast<ValueRangeAnalyzer>(Other)));
  } else {
    VRAStore::convexMerge(llvm::cast<VRAStore>(llvm::cast<VRAGlobalStore>(Other)));
  }
}

std::shared_ptr<CodeAnalyzer>
VRAGlobalStore::newCodeAnalyzer(CodeInterpreter &CI) {
  return std::make_shared<ValueRangeAnalyzer>(CI);
}

generic_range_ptr_t
VRAGlobalStore::findRetVal(const llvm::Function* F) {
  auto it = ReturnValues.find(F);
  if (it != ReturnValues.end()) {
    return it->second;
  }

  return nullptr;
}

void
VRAGlobalStore::setRetVal(const llvm::Function* F,
                          generic_range_ptr_t RetVal) {
  ReturnValues[F] = RetVal;
}

void
VRAGlobalStore::setArgumentRanges(const llvm::Function &F,
                                  const std::list<range_node_ptr_t> &AARanges) {
  assert(AARanges.size() == F.arg_size()
         && "Mismatch between number of actual and formal parameters.");
  auto derived_info_it = AARanges.begin();
  auto derived_info_end = AARanges.end();

  for (const llvm::Argument &formal_arg : F.args()) {
    assert(derived_info_it != derived_info_end);
    setNode(&formal_arg, *derived_info_it);
    ++derived_info_it;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Metadata Processing
////////////////////////////////////////////////////////////////////////////////

bool
VRAGlobalStore::isValidRange(mdutils::Range *rng) const {
  return rng != nullptr && !std::isnan(rng->Min) && !std::isnan(rng->Max);
}

void
VRAGlobalStore::harvestMetadata(Module &M) {
  using namespace mdutils;
  MetadataManager &MDManager = MetadataManager::getMetadataManager();

  for (const auto &v : M.globals()) {
    // retrieve info about global var v, if any
    InputInfo *II = MDManager.retrieveInputInfo(v);
    if (II != nullptr && isValidRange(II->IRange.get())) {
      UserInput[&v] = make_range(II->IRange->Min, II->IRange->Max, II->isFinal());
      DerivedRanges[&v] = make_range_node(UserInput[&v]);
    } else if (StructInfo *SI = MDManager.retrieveStructInfo(v)) {
      UserInput[&v] = harvestStructMD(SI);
      DerivedRanges[&v] = make_range_node(UserInput[&v]);
    }
  }

  for (llvm::Function &f : M.functions()) {
    if (f.empty()) {
      continue;
    }
    const std::string name = f.getName();

    // retrieve info about function parameters
    SmallVector<mdutils::MDInfo*, 5U> argsII;
    MDManager.retrieveArgumentInputInfo(f, argsII);
    SmallVector<int, 5U> argsW;
    MetadataManager::retrieveInputInfoInitWeightMetadata(&f, argsW);
    auto itII = argsII.begin();
    auto itW = argsW.begin();
    for (Argument &formal_arg : f.args()) {
      if (itII == argsII.end())
        break;
      if (itW != argsW.end()) {
        if (*itW == 1U) {
          UserInput[&formal_arg] = harvestStructMD(*itII);
        }
        ++itW;
      } else {
        UserInput[&formal_arg] = harvestStructMD(*itII);
      }
      ++itII;
    }

    // retrieve info about instructions, for each basic block bb
    for (const auto &bb : f.getBasicBlockList()) {
      for (const auto &i : bb.getInstList()) {
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
            // Kludge for alloca not to be always roots, please check
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
            UserInput[i_ptr] =
              make_range(II->IRange->Min, II->IRange->Max, II->isFinal());
          }
        } else if (StructInfo *SI = dyn_cast<StructInfo>(MDI)) {
          const llvm::Value* i_ptr = &i;
          UserInput[i_ptr] = harvestStructMD(SI);
        }
      }
    }

  } // end iteration over Function in Module
  return;
}

generic_range_ptr_t
VRAGlobalStore::harvestStructMD(mdutils::MDInfo *MD) {
  using namespace mdutils;
  if (MD == nullptr) {
    return nullptr;

  } else if (InputInfo *II = dyn_cast<InputInfo>(MD)) {
    if (isValidRange(II->IRange.get()))
      return make_range(II->IRange->Min, II->IRange->Max, II->isFinal());
    else
      return nullptr;

  } else if (StructInfo *SI = dyn_cast<StructInfo>(MD)) {
    std::vector<generic_range_ptr_t> rngs;
    for (auto it = SI->begin(); it != SI->end(); it++) {
      rngs.push_back(harvestStructMD(it->get()));
    }

    return make_s_range(rngs);
  }

  llvm_unreachable("unknown type of MDInfo");
}

void
VRAGlobalStore::saveResults(llvm::Module &M) {
  using namespace mdutils;
  MetadataManager &MDManager = MetadataManager::getMetadataManager();
  for (GlobalVariable &v : M.globals()) {
    const generic_range_ptr_t range = fetchInfo(&v, true);
    if (range != nullptr) {
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
        const generic_range_ptr_t range = fetchInfo(&arg, true);
        if (range != nullptr) {
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
        if (isa<StoreInst>(i))
          continue;
        refreshRange(&i);
        const generic_range_ptr_t range = fetchInfo(&i, true);
        if (range != nullptr) {
          MDInfo *mdi = MDManager.retrieveMDInfo(&i);
          if (mdi != nullptr) {
            std::shared_ptr<MDInfo> cpymdi(mdi->clone());
            updateMDInfo(cpymdi, range);
            MDManager.setMDInfoMetadata(&i, cpymdi.get());
          } else {
            std::shared_ptr<MDInfo> newmdi = toMDInfo(range);
            MDManager.setMDInfoMetadata(&i, newmdi.get());
          }
        }
      } // end instruction
    } // end bb
  } // end function
  return;
}

void
VRAGlobalStore::refreshRange(const llvm::Instruction* i) {
  if (const LoadInst* li = dyn_cast_or_null<LoadInst>(i)) {
    const generic_range_ptr_t newInfo = fetchInfo(li->getPointerOperand());
    saveValueInfo(li, newInfo);
  }
}

std::shared_ptr<mdutils::MDInfo>
VRAGlobalStore::toMDInfo(const generic_range_ptr_t &r) {
  using namespace mdutils;
  if (r == nullptr) return nullptr;
  if (const range_ptr_t scalar = std::dynamic_ptr_cast<range_t>(r)) {
    return std::make_shared<InputInfo>(nullptr,
                                       std::make_shared<Range>(scalar->min(), scalar->max()),
                                       nullptr);
  } else if (const range_s_ptr_t structr = std::dynamic_ptr_cast<range_s_t>(r)) {
    SmallVector<std::shared_ptr<mdutils::MDInfo>, 4U> fields;
    fields.reserve(structr->getNumRanges());
    for (const generic_range_ptr_t f : structr->ranges())
      fields.push_back(toMDInfo(f));
    return std::make_shared<StructInfo>(fields);
  }
  llvm_unreachable("Unknown range type.");
}

void
VRAGlobalStore::updateMDInfo(std::shared_ptr<mdutils::MDInfo> mdi,
                             const generic_range_ptr_t &r) {
  using namespace mdutils;
  if (mdi == nullptr || r == nullptr) return;
  if (const range_ptr_t scalar = std::dynamic_ptr_cast<range_t>(r)) {
    std::shared_ptr<InputInfo> ii = std::static_ptr_cast<InputInfo>(mdi);
    ii->IRange.reset(new Range(scalar->min(), scalar->max()));
    return;
  } else if (const range_s_ptr_t structr = std::dynamic_ptr_cast<range_s_t>(r)) {
    std::shared_ptr<StructInfo> si = std::static_ptr_cast<StructInfo>(mdi);
    auto derfield_it = structr->ranges().begin();
    auto derfield_end = structr->ranges().end();
    for (StructInfo::size_type i = 0; i < si->size(); ++i) {
      if (derfield_it == derfield_end) break;
      std::shared_ptr<mdutils::MDInfo> mdfield = si->getField(i);
      if (mdfield)
        updateMDInfo(mdfield, *derfield_it);
      else
        si->setField(i, toMDInfo(*derfield_it));

      ++derfield_it;
    }
    return;
  }
  llvm_unreachable("Unknown range type.");
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


const generic_range_ptr_t
VRAGlobalStore::fetchInfo(const llvm::Value* v,
                          bool derived_or_final) {
  generic_range_ptr_t input_range = getUserInput(v);
  if (input_range && (!derived_or_final || input_range->isFinal())) {
    return input_range;
  }

  const generic_range_ptr_t Derived = VRAStore::fetchInfo(v);
  if (input_range
      && Derived
      && std::isa_ptr<VRA_Structured_Range>(Derived)
      && v->getType()->isPointerTy()) {
    // fill null input_range fields with corresponding derived fields
    return fillRangeHoles(Derived, input_range);
  }

  return Derived;
}

range_node_ptr_t
VRAGlobalStore::getOrCreateNode(const llvm::Value* v) {
  range_node_ptr_t Node = VRAStore::getOrCreateNode(v);
  if (Node) {
    return Node;
  }

  if (isa<GlobalVariable>(v) || isa<Argument>(v)) {
      // create root node in global analyzer
      Node = make_range_node();
      DerivedRanges[v] = Node;
      return Node;
  }

  return nullptr;
}

void
VRAGlobalStore::setNode(const llvm::Value* V, range_node_ptr_t Node) {
  DerivedRanges[V] = Node;
}

generic_range_ptr_t
VRAGlobalStore::getUserInput(const llvm::Value *V) const {
  auto UIt = UserInput.find(V);
  if (UIt != UserInput.end()) {
    return UIt->second;
  }
  return nullptr;
}
