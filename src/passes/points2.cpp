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

#include <queue>
#include <set>
#include <unordered_map>

using namespace llvm;

struct LocalData {
  std::unordered_map<Value *, std::set<Value *>> pt;
  std::queue<std::pair<Value *, std::set<Value *>>> worklist;
  std::unordered_map<Value *, std::set<Value *>> PFG;

  ~LocalData() {}
};

void addEdge(Value *s, Value *t, LocalData &localdata) {
  auto &pt = localdata.pt;
  auto &worklist = localdata.worklist;
  auto &PFG = localdata.PFG;
  if (PFG[s].find(t) == PFG[s].end()) {
    PFG[s].insert(t);
    if (!pt[s].empty()) {
      worklist.push({t, pt[s]});
    }
  }
}

void propagate(Value *n, const std::set<Value *> &pts, LocalData &localdata) {
  auto &pt = localdata.pt;
  auto &worklist = localdata.worklist;
  auto &PFG = localdata.PFG;
  if (!pts.empty()) {
    pt[n].insert(pts.begin(), pts.end());
    for (auto *s : PFG[n]) {
      worklist.push({s, pts});
    }
  }
}

void initialize(Function &func, LocalData &localdata) {
  auto &worklist = localdata.worklist;
  for (auto &BB : func) {
    for (auto &inst : BB) {

      if (auto *alloca = dyn_cast<AllocaInst>(&inst)) {
        worklist.push({alloca, {alloca}});

      } else if (auto *gep = dyn_cast<GetElementPtrInst>(&inst)) {
        worklist.push({gep, {gep}});

      } else if (auto *phi = dyn_cast<PHINode>(&inst)) {
        for (int i = 0; i < phi->getNumIncomingValues(); ++i) {
          Value *val = phi->getIncomingValue(i);
          if (isa<Instruction>(val) || isa<Argument>(val)) {
            addEdge(val, phi, localdata);
          }
        }

      } else if (auto *select = dyn_cast<SelectInst>(&inst)) {
        Value *tval = select->getTrueValue();
        Value *fval = select->getFalseValue();
        if (isa<Instruction>(tval) || isa<Argument>(tval)) {
          addEdge(tval, select, localdata);
        }
        if (isa<Instruction>(fval) || isa<Argument>(fval)) {
          addEdge(fval, select, localdata);
        }

      } else if (auto *cast = dyn_cast<CastInst>(&inst)) {
        Value *src = cast->getOperand(0);
        addEdge(src, cast, localdata);
      }
      // iter end
    }
  }
}

void solve(LocalData &localdata) {
  auto &pt = localdata.pt;
  auto &worklist = localdata.worklist;
  // auto &PFG = localdata.PFG;
  while (!worklist.empty()) {
    auto [n, pts] = worklist.front();
    worklist.pop();

    std::set<Value *> delta;
    std::set_difference(pts.begin(), pts.end(), pt[n].begin(), pt[n].end(),
                        std::inserter(delta, delta.begin()));
    propagate(n, delta, localdata);

    for (auto *user : n->users()) {
      if (StoreInst *store = dyn_cast<StoreInst>(user)) {
        // *x = y (store y -> ptr x)
        if (store->getPointerOperand() == n) {
          Value *y = store->getValueOperand();
          if (isa<Instruction>(y) || isa<Argument>(y)) {
            for (Value *oi : delta) {
              addEdge(y, oi, localdata);
            }
          }
        }

      } else if (LoadInst *load = dyn_cast<LoadInst>(user)) {
        // y = *x (load ptr x -> y)
        if (load->getPointerOperand() == n) {
          Value *y = load;
          for (Value *oi : delta) {
            addEdge(oi, y, localdata);
          }
        }
      }
    }
    // iter end
  }
}

void Points2Analysis::run(Function &func) {
  LocalData localdata;
  initialize(func, localdata);
  solve(localdata);
}
