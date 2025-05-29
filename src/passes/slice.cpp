#include "passes.hpp"

#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"

#include <cmath>
#include <queue>
#include <unordered_set>

using namespace llvm;

void backwardSlice(Value *root, std::unordered_set<Value *> &slice) {
  std::queue<Value *> worklist;

  auto add2Slice = [&](Value *i) {
    if (slice.insert(i).second) {
      worklist.push(i);
    }
  };

  slice.insert(root);
  worklist.push(root);

  while (!worklist.empty()) {
    auto *val = worklist.front();
    worklist.pop();

    if (auto *phi = dyn_cast<PHINode>(val)) {
      for (int i = 0; i < phi->getNumIncomingValues(); ++i) {
        auto *ival = phi->getIncomingValue(i);
        if (auto *ivalInst = dyn_cast<Instruction>(ival)) {
          add2Slice(ivalInst);
        }
        auto *iBB = phi->getIncomingBlock(i);
        Instruction *term = iBB->getTerminator();
        add2Slice(term);
      }
      continue;

    } else if (auto *select = dyn_cast<SelectInst>(val)) {
      Value *tval = select->getTrueValue();
      Value *fval = select->getFalseValue();
      if (auto *tvalInst = dyn_cast<Instruction>(tval)) {
        add2Slice(tvalInst);
      }
      if (auto *fvalInst = dyn_cast<Instruction>(fval)) {
        add2Slice(fvalInst);
      }

    } else if (auto *cast = dyn_cast<CastInst>(val)) {
      Value *src = cast->getOperand(0);
      if (auto *srcInst = dyn_cast<Instruction>(src)) {
        add2Slice(srcInst);
      }

    } else if (auto *inst = dyn_cast<Instruction>(val)) {
      for (auto &use : inst->operands()) {
        if (auto *op = dyn_cast<Instruction>(use)) {
          add2Slice(op);
        }
      }

    } else {
      add2Slice(val);
    }

    if (auto *inst = dyn_cast<Instruction>(val)) {
      for (BasicBlock *predBB : predecessors(inst->getParent())) {
        auto *term = predBB->getTerminator();
        add2Slice(term);
      }
    }
    // iter end
  }
}

void forwardSlice(Value *root, std::unordered_set<Value *> &slice) {
  std::queue<Value *> worklist;

  auto add2Slice = [&](Value *i) {
    if (slice.insert(i).second) {
      worklist.push(i);
    }
  };

  slice.insert(root);
  worklist.push(root);

  while (!worklist.empty()) {
    auto *val = worklist.front();
    worklist.pop();

    for (auto *user : val->users()) {
      add2Slice(user);
    }
  }
}

void sliceFunc(Function &func) {
  for (auto &BB : func) {
    for (auto &inst : BB) {
      if (isa<GetElementPtrInst>(inst)) {
        std::unordered_set<Value *> slice;
        backwardSlice(&inst, slice);
        forwardSlice(&inst, slice);
      } else if (isa<AllocaInst>(inst)) {
        std::unordered_set<Value *> slice;
        forwardSlice(&inst, slice);
      }
    }
  }
  for (auto &arg : func.args()) {
    std::unordered_set<Value *> slice;
    forwardSlice(&arg, slice);
  }
}

void Slicing::run(Function &func) { sliceFunc(func); }