#include "scheduler.hpp"
#include "passes/passes.hpp"

#include "llvm/IR/Module.h"

#include <chrono>
#include <cmath>
#include <fstream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

using namespace llvm;

std::mutex outsmtx;

void TaskTimer::run(const std::vector<std::shared_ptr<FuncPass>> &passes,
                    Module &module) {
  std::string csvname = "tasktime.csv";
  std::ofstream csv(csvname);
  csv << "name,size";
  for (auto pass : passes) {
    csv << "," << pass->name();
  }
  csv << "\n";

  for (auto &func : module) {
    if (func.isDeclaration())
      continue;
    csv << func.getName().str() << "," << func.size();
    for (auto pass : passes) {
      auto start = std::chrono::high_resolution_clock::now();
      pass->run(func);
      auto end = std::chrono::high_resolution_clock::now();
      auto duration =
          std::chrono::duration_cast<std::chrono::microseconds>(end - start);
      csv << "," << duration.count();
    }
    csv << "\n";
  }
}

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

void funcThread(std::vector<std::shared_ptr<FuncPass>> passes,
                std::mutex &Qmutex, std::priority_queue<FuncInfo> &funcQ,
                int tid) {
#ifdef PRINT_STATS
  auto start = std::chrono::high_resolution_clock::now();
  int max_time = 0;
  int max_size = 0;
  int task_count = 0;
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
#endif
  }

#ifdef PRINT_STATS
  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  {
    std::lock_guard<std::mutex> lock(outsmtx);
    outs() << "\tThread " << tid << "\ttime:\t" << duration.count() << " us\n";
    outs() << "\t\tMax task time :\t " << max_time << " us with\t " << max_size
           << " BBs\n";
    outs() << "\t\tTasks processed:\t" << task_count << "\n";
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

struct TaskInfo {
  std::shared_ptr<FuncPass> pass;
  Function *func;
  size_t size;
  int index;

  bool operator<(const TaskInfo &rhs) const { return size < rhs.size; }
};

void taskThread(std::mutex &Qmutex, std::priority_queue<TaskInfo> &taskQ,
                int tid) {
#ifdef PRINT_STATS
  auto start = std::chrono::high_resolution_clock::now();
  int max_time = 0;
  int max_size = 0;
  int task_count = 0;
#endif

  while (true) {
    int index;
    Function *func;
    std::shared_ptr<FuncPass> pass;
    int size;
    {
      std::lock_guard<std::mutex> lock(Qmutex);
      if (taskQ.empty())
        break;
      index = taskQ.top().index;
      func = taskQ.top().func;
      pass = taskQ.top().pass;
      size = taskQ.top().size;
      taskQ.pop();
    }
#ifdef PRINT_STATS
    auto sub_start = std::chrono::high_resolution_clock::now();
#endif

    pass->run(*func);

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
#endif
  }

#ifdef PRINT_STATS
  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  {
    std::lock_guard<std::mutex> lock(outsmtx);
    outs() << "\tThread " << tid << "\ttime:\t" << duration.count() << " us\n";
    outs() << "\t\tMax task time :\t " << max_time << " us with\t " << max_size
           << " BBs\n";
    outs() << "\t\tTasks processed:\t" << task_count << "\n";
  }
#endif
}

void ConcurrentTasks::run(const std::vector<std::shared_ptr<FuncPass>> &passes,
                          Module &module) {
  std::priority_queue<TaskInfo> taskQ;

  for (auto [i, func] : enumerate(module)) {
    for (auto pass : passes) {
      if (func.isDeclaration())
        continue;
      taskQ.push({pass, &func, func.size(), (int)i});
    }
  }

  std::mutex Qmutex;
  std::vector<std::thread> threads;
  threads.reserve(nthreads);
  for (int i = 0; i < nthreads; ++i) {
    threads.emplace_back(taskThread, std::ref(Qmutex), std::ref(taskQ),
                         i);
  }
  for (auto &t : threads) {
    t.join();
  }
}



#include "llvm/Support/SourceMgr.h"
#include "llvm/IRReader/IRReader.h"

struct TaskMInfo {
  std::shared_ptr<FuncPass> pass;
  int func_idx;
  size_t size;
  int index;

  bool operator<(const TaskInfo &rhs) const { return size < rhs.size; }
};

void taskMThread(std::mutex &Qmutex, std::priority_queue<TaskInfo> &taskQ,
                int tid) {
#ifdef PRINT_STATS
  auto start = std::chrono::high_resolution_clock::now();
  int max_time = 0;
  int max_size = 0;
  int task_count = 0;
#endif

  while (true) {
    int index;
    Function *func;
    std::shared_ptr<FuncPass> pass;
    int size;
    {
      std::lock_guard<std::mutex> lock(Qmutex);
      if (taskQ.empty())
        break;
      index = taskQ.top().index;
      func = taskQ.top().func;
      pass = taskQ.top().pass;
      size = taskQ.top().size;
      taskQ.pop();
    }
#ifdef PRINT_STATS
    auto sub_start = std::chrono::high_resolution_clock::now();
#endif

    pass->run(*func);

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
#endif
  }

#ifdef PRINT_STATS
  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  {
    std::lock_guard<std::mutex> lock(outsmtx);
    outs() << "\tThread " << tid << "\ttime:\t" << duration.count() << " us\n";
    outs() << "\t\tMax task time :\t " << max_time << " us with\t " << max_size
           << " BBs\n";
    outs() << "\t\tTasks processed:\t" << task_count << "\n";
  }
#endif
}

void ConcurrentModules::runOnFile(
    const std::vector<std::shared_ptr<FuncPass>> &passes,
    const std::string &filename) {
  
    std::priority_queue<TaskInfo> taskQ;

  // for (auto [i, func] : enumerate(module)) {
  //   for (auto pass : passes) {
  //     if (func.isDeclaration())
  //       continue;
  //     taskQ.push({pass, &func, func.size(), (int)i});
  //   }
  // }

  // std::mutex Qmutex;
  // std::vector<std::thread> threads;
  // threads.reserve(nthreads);
  // for (int i = 0; i < nthreads; ++i) {
  //   threads.emplace_back(taskThread, std::ref(Qmutex), std::ref(taskQ),
  //                        i);
  // }
  // for (auto &t : threads) {
  //   t.join();
  // }

  // int nthreads = passes.size();
  // std::vector<std::thread> threads;
  // for (auto pass : passes) {
  //   threads.emplace_back(passMThread, pass, std::ref(filename));
  // }
  // for (auto &t : threads) {
  //   t.join();
  // }
}
