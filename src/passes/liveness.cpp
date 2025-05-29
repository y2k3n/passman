#include "passes.hpp"

#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"

#include <algorithm>
#include <cstdlib>
#include <queue>
#include <set>
#include <unordered_set>

using namespace llvm;

// std::set<BasicBlock *> findExitBBs(Function &func) {
//   std::set<BasicBlock *> exitBBs;
//   for (auto &BB : func) {
//     auto &lastI = BB.back();
//     if (isa<ReturnInst>(lastI)) {
//       exitBBs.insert(&BB);
//     }
//   }
//   return exitBBs;
// }

// PhiDefs(B) the variables defined by φ-functions at the entry of block B
// PhiUses(B) the set of variables used in a φ-function at the entry of a
// successor of the block B
void findUSEsDEFs(
    Function &func, std::unordered_map<BasicBlock *, std::set<Value *>> &USEs,
    std::unordered_map<BasicBlock *, std::set<Value *>> &DEFs,
    std::unordered_map<BasicBlock *, std::set<Value *>> &phiUSEs,
    std::unordered_map<BasicBlock *, std::set<Value *>> &phiDEFs) {
  for (auto &BB : func) {
    auto &DEF = DEFs[&BB];
    auto &USE = USEs[&BB];
    auto &pDEF = phiDEFs[&BB];

    auto iter = BB.begin();
    for (; iter != BB.end(); ++iter) {
      if (!isa<PHINode>(*iter))
        break;
      auto phi = dyn_cast<PHINode>(&*iter);
      pDEF.insert(phi);
      int numIncoming = phi->getNumIncomingValues();
      for (int i = 0; i < numIncoming; ++i) {

        Value *inVal = phi->getIncomingValue(i);
        BasicBlock *inBB = phi->getIncomingBlock(i);
        if (!isa<Instruction>(inVal) && !isa<Argument>(inVal))
          continue;
        phiUSEs[inBB].insert(inVal);
      }
    }

    for (; iter != BB.end(); ++iter) {
      auto &inst = *iter;
      // if (inst.mayHaveSideEffects())
      //   sideBBs.insert(&BB);

      for (auto &oprand : inst.operands()) {
        Value *val = oprand.get();
        if (isa<Instruction>(val) || isa<Argument>(val)) {
          if (DEF.find(val) == DEF.end()) {
            USE.insert(val); // use without def
          }
        }
      }
      if (!inst.getType()->isVoidTy()) {
        DEF.insert(&inst);
      }
    }
  }
}

void findLiveVars(Function &func,
                  std::unordered_map<BasicBlock *, std::set<Value *>> &INs,
                  std::unordered_map<BasicBlock *, std::set<Value *>> &OUTs) {
  if (func.isDeclaration())
    return;

  std::unordered_map<BasicBlock *, std::set<Value *>> USEs, DEFs, phiUSEs,
      phiDEFs;
  std::set<BasicBlock *> sideBBs;
  findUSEsDEFs(func, USEs, DEFs, phiUSEs, phiDEFs);
  std::queue<BasicBlock *> worklist;
  std::unordered_set<BasicBlock *> hashWL;
  // auto exitBBs = findExitBBs(func);
  // for (BasicBlock *eBB : exitBBs) {
  //   if (hashWL.insert(eBB).second)
  //     worklist.push(eBB);
  // }
  // for (BasicBlock *sBB : sideBBs) {
  //   if (hashWL.insert(sBB).second)
  //     worklist.push(sBB);
  // }
  ReversePostOrderTraversal<Function *> RPOT(&func);
  for (BasicBlock *BB : RPOT) {
    if (hashWL.insert(BB).second)
      worklist.push(BB);
  }

  // std::unordered_map<BasicBlock *, std::set<Value *>> INs, OUTs;
  while (!worklist.empty()) {
    BasicBlock *BB = worklist.front();
    worklist.pop();
    hashWL.erase(BB);

    // LiveOut(B) = ⋃_S∈succs(B) (LiveIn(S) \ PhiDefs(S)) ∪ PhiUses(B)
    // LiveIn(B) = PhiDefs(B) ∪ UpwardExposed(B) ∪ (LiveOut(B) \ Defs(B))
    // std::set<Value *> oldIN = INs[BB], oldOUT = OUTs[BB];
    bool changed = false;
    std::set<Value *> liveIN, liveOUT;
    liveOUT = phiUSEs[BB];
    for (BasicBlock *succ : successors(BB)) {
      std::set_difference(INs[succ].begin(), INs[succ].end(),
                          phiDEFs[succ].begin(), phiDEFs[succ].end(),
                          std::inserter(liveOUT, liveOUT.end()));
    }
    changed |= (OUTs[BB] != liveOUT);
    OUTs[BB] = liveOUT;

    liveIN = phiDEFs[BB];
    std::set_difference(OUTs[BB].begin(), OUTs[BB].end(), DEFs[BB].begin(),
                        DEFs[BB].end(), std::inserter(liveIN, liveIN.end()));
    liveIN.insert(USEs[BB].begin(), USEs[BB].end());
    changed |= (INs[BB] != liveIN);
    INs[BB] = liveIN;

    if (changed) {
      for (BasicBlock *pred : predecessors(BB)) {
        if (hashWL.insert(pred).second)
          worklist.push(pred);
      }
    }
  }
}

void LivenessAnalysis::run(Function &func) {
  std::unordered_map<BasicBlock *, std::set<Value *>> INs, OUTs;
  findLiveVars(func, INs, OUTs);
}
