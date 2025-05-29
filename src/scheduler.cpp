#include "scheduler.hpp"
#include "passes/passes.hpp"

#include "llvm/IR/Module.h"

#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

using namespace llvm;

std::mutex outsmtx;

void Sequential::run(const std::vector<std::shared_ptr<FuncPass>> &passes,
                     Module &module) {
  for (auto pass : passes) {
    auto start = std::chrono::high_resolution_clock::now();
    for (auto &func : module) {
      if (func.isDeclaration())
        continue;
      pass->run(func);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    outs() << "\t" << pass->name() << ": " << duration.count() << " us\n";
  }
}

void passThread(std::shared_ptr<FuncPass> pass, Module &module) {
  auto start = std::chrono::high_resolution_clock::now();

  for (auto &func : module) {
    if (func.isDeclaration())
      continue;
    pass->run(func);
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  {
    std::lock_guard<std::mutex> lock(outsmtx);
    outs() << "\t" << pass->name() << ": " << duration.count() << " us\n";
  }
}

void ConcurrentPasses::run(const std::vector<std::shared_ptr<FuncPass>> &passes,
                           Module &module) {
  int nthreads = passes.size();
  std::vector<std::thread> threads;
  for (auto pass : passes) {
    threads.emplace_back(passThread, pass, std::ref(module));
  }
  for (auto &t : threads) {
    t.join();
  }
}

struct FuncInfo {
  Function *func;
  size_t size;
  int index;

  bool operator<(const FuncInfo &rhs) const { return size < rhs.size; }
};

#define PRINT_STATS
void funcThread(std::vector<std::shared_ptr<FuncPass>> passes,
                std::mutex &Qmutex, std::priority_queue<FuncInfo> &funcQ,
                int tid) {
#ifdef PRINT_STATS
  auto start = std::chrono::high_resolution_clock::now();
  int max_time = 0;
  int max_size = 0;
  int task_count = 0;
  // int total_size = 0;
  // int total_size_sq = 0;
  // int total_time = 0;
  // int total_time_sq = 0;
#endif

  while (true) {
    int index;
    Function *func;
    int size;
    {
      std::lock_guard<std::mutex> lock(Qmutex);
      if (funcQ.empty())
        break;
      index = funcQ.top().index;
      func = funcQ.top().func;
      size = funcQ.top().size;
      funcQ.pop();
    }
#ifdef PRINT_STATS
    auto sub_start = std::chrono::high_resolution_clock::now();
#endif

    for (auto pass : passes) {
      pass->run(*func);
    }

#ifdef PRINT_STATS
    auto sub_end = std::chrono::high_resolution_clock::now();
    auto sub_duration = std::chrono::duration_cast<std::chrono::microseconds>(
        sub_end - sub_start);
    int time = sub_duration.count();
    if (time > max_time) {
      max_time = time;
      max_size = size;
    }
    task_count++;
    // total_size += size;
    // total_size_sq += size * size;
    // total_time += time;
    // total_time_sq += time * time;
#endif
  }

#ifdef PRINT_STATS
  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  // double mean_size = (task_count > 0) ? total_size / task_count : 0;
  // double var_size = (task_count > 0)
  //                       ? (total_size_sq / task_count) - (mean_size * mean_size)
  //                       : -(mean_size * mean_size);
  // double mean_time = (task_count > 0) ? total_time / task_count : 0;
  // double var_time = (task_count > 0)
  //                       ? (total_time_sq / task_count) - (mean_time * mean_time)
  //                       : -(mean_time * mean_time);

  {
    std::lock_guard<std::mutex> lock(outsmtx);
    outs() << "\tThread " << tid << "\ttime:\t" << duration.count() << " ms\n";
    outs() << "\t\tMax task time :\t " << max_time << " ms with\t " << max_size
           << " BBs\n";
    outs() << "\t\tTasks processed:\t" << task_count << "\n";
    // outs() << "\t\tTask size mean:\t" << mean_size << ", var:\t" << var_size
    //        << ", std dev:\t" << std::sqrt(var_size) << "\n";
    // outs() << "\t\tTask time mean:\t" << mean_time << ", var:\t" << var_time
    //        << ", std dev:\t" << std::sqrt(var_time) << "\n";
  }
#endif
}

void ConcurrentFuncs::run(const std::vector<std::shared_ptr<FuncPass>> &passes,
                          Module &module) {
  std::priority_queue<FuncInfo> funcQ;

  for (auto [i, func] : enumerate(module)) {
    if (func.isDeclaration())
      continue;
    funcQ.push({&func, func.size(), (int)i});
  }

  std::mutex Qmutex;
  std::vector<std::thread> threads;
  threads.reserve(nthreads);
  for (int i = 0; i < nthreads; ++i) {
    threads.emplace_back(funcThread, passes, std::ref(Qmutex), std::ref(funcQ),
                         i);
  }
  for (auto &t : threads) {
    t.join();
  }
}
