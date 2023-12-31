// Copyright 2017 The Ray Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <boost/asio/thread_pool.hpp>
#include <boost/thread.hpp>
#include <list>
#include <queue>
#include <set>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/mutex.h"
#include "ray/common/task/task_spec.h"

namespace ray {
namespace core {

/// Wraps a thread-pool to block posts until the pool has free slots. This is used
/// by the SchedulingQueue to provide backpressure to clients.
class BoundedExecutor {
 public:
  BoundedExecutor(int max_concurrency);

  /// Posts work to the pool, blocking if no free threads are available.
  void PostBlocking(std::function<void()> fn);

  /// Stop the thread pool.
  void Stop();

  /// Join the thread pool.
  void Join();

 private:
  bool ThreadsAvailable() EXCLUSIVE_LOCKS_REQUIRED(mu_);

  /// Protects access to the counters below.
  absl::Mutex mu_;
  /// The number of currently running tasks.
  int num_running_ GUARDED_BY(mu_);
  /// The max number of concurrently running tasks allowed.
  const int max_concurrency_;
  /// The underlying thread pool for running tasks.
  boost::asio::thread_pool pool_;
};

/// A manager that manages a set of thread pool. which will perform
/// the methods defined in one concurrency group.
class PoolManager final {
 public:
  explicit PoolManager(const std::vector<ConcurrencyGroup> &concurrency_groups = {},
                       const int32_t default_group_max_concurrency = 1);

  std::shared_ptr<BoundedExecutor> GetPool(const std::string &concurrency_group_name,
                                           ray::FunctionDescriptor fd);

  /// Stop and join the thread pools that the pool manager owns.
  void Stop();

 private:
  // Map from the name to their corresponding thread pools.
  std::unordered_map<std::string, std::shared_ptr<BoundedExecutor>>
      name_to_thread_pool_index_;

  // Map from the FunctionDescriptors to their corresponding thread pools.
  std::unordered_map<std::string, std::shared_ptr<BoundedExecutor>>
      functions_to_thread_pool_index_;

  // The thread pool for default concurrency group. It's nullptr if its max concurrency
  // is 1.
  std::shared_ptr<BoundedExecutor> default_thread_pool_ = nullptr;
};

}  // namespace core
}  // namespace ray
