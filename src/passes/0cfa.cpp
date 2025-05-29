#include "passes.hpp"

#include "llvm/ADT/DenseSet.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"

#include <unordered_map>
#include <unordered_set>

using namespace llvm;

struct LocalData {
  std::unordered_map<Instruction *, DenseSet<Value *>> callMap;
  std::unordered_map<Value *, DenseSet<Value *>> points2;
  std::unordered_set<Value *> visited;
};

void analyzePtr(Value *val, LocalData &localdata) {
  auto &callMap = localdata.callMap;
  auto &points2 = localdata.points2;
  auto &visited = localdata.visited;
  if (visited.find(val) != visited.end()) {
    return;
  }
  visited.insert(val);

  if (isa<Function>(val) || isa<Argument>(val)) {
    points2[val] = {val};

  } else if (auto *cast = dyn_cast<CastInst>(val)) {
    auto *src = cast->getOperand(0);
    analyzePtr(src, localdata);
    points2[cast] = points2[src];

  } else if (auto *phi = dyn_cast<PHINode>(val)) {
    for (int i = 0; i < phi->getNumIncomingValues(); ++i) {
      Value *inval = phi->getIncomingValue(i);
      // if (isa<Instruction>(inval) || isa<Argument>(inval)) {
      analyzePtr(inval, localdata);
      points2[phi].insert(points2[inval].begin(), points2[inval].end());
      // }
    }

  } else if (auto *select = dyn_cast<SelectInst>(val)) {
    Value *tval = select->getTrueValue();
    Value *fval = select->getFalseValue();
    // if (isa<Instruction>(tval) || isa<Argument>(tval)) {
    analyzePtr(tval, localdata);
    points2[select].insert(points2[tval].begin(), points2[tval].end());
    // }
    // if (isa<Instruction>(fval) || isa<Argument>(fval)) {
    analyzePtr(fval, localdata);
    points2[select].insert(points2[fval].begin(), points2[fval].end());
    // }

  } else if (auto *load = dyn_cast<LoadInst>(val)) {
    auto *loadptr = load->getPointerOperand();
    analyzePtr(loadptr, localdata);
    points2[load] = points2[loadptr];
    for (auto *user : loadptr->users()) {
      if (auto *store = dyn_cast<StoreInst>(user)) {
        if (store->getPointerOperand() == loadptr) {
          auto *stval = store->getValueOperand();
          analyzePtr(stval, localdata);
          points2[load].insert(points2[stval].begin(), points2[stval].end());
        }
      }
    }

  } else if (auto *global = dyn_cast<GlobalVariable>(val)) {
    points2[val] = {val};
    if (global->hasInitializer()) {
      Value *initval = global->getInitializer();
      analyzePtr(initval, localdata);
      points2[val].insert(points2[initval].begin(), points2[initval].end());
    }
    for (auto *user : global->users()) {
      if (auto *store = dyn_cast<StoreInst>(user)) {
        if (store->getPointerOperand() == global) {
          Value *storedVal = store->getValueOperand();
          analyzePtr(storedVal, localdata);
          points2[val].insert(points2[storedVal].begin(),
                              points2[storedVal].end());
        }
      }
    }

  } else if (auto *gep = dyn_cast<GetElementPtrInst>(val)) {
    Value *baseptr = gep->getPointerOperand();
    analyzePtr(baseptr, localdata);
    points2[gep] = points2[baseptr];

    // } else if (auto *cexpr = dyn_cast<ConstantExpr>(val)) {
    //   auto *ceinst = cexpr->getAsInstruction();
    //   analyzePtr(ceinst, localdata);
    //   points2[cexpr] = points2[ceinst];
    //   points2.erase(ceinst);
    //   ceinst->deleteValue();

  } else {
    points2[val] = {val};
  }
}

void analyzeIntra(Function &func, LocalData &localdata) {
  auto &callMap = localdata.callMap;
  auto &points2 = localdata.points2;
  auto &visited = localdata.visited;

  for (auto &BB : func) {
    for (auto &inst : BB) {
      if (auto *call = dyn_cast<CallInst>(&inst)) {
        auto *callptr = call->getCalledOperand();
        analyzePtr(callptr, localdata);
        callMap[call] = points2[callptr];
      }
    }
  }
}

void ZeroCFAnalysis::run(Function &func) {
  LocalData localdata;
  analyzeIntra(func, localdata);
}
