/**
 * Copyright (c) 2016-present, Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CAFFE2_CORE_NET_ASYNC_TRACING_H_
#define CAFFE2_CORE_NET_ASYNC_TRACING_H_

#include "caffe2/core/common.h"
#include "caffe2/core/net_async_base.h"
#include "caffe2/core/operator.h"
#include "caffe2/core/timer.h"

namespace caffe2 {
namespace tracing {

struct TracerEvent {
  int op_id_ = -1;
  int task_id_ = -1;
  int stream_id_ = -1;
  const char* name_ = nullptr;
  const char* category_ = nullptr;
  long timestamp_ = -1.0;
  bool is_beginning_ = false;
  long thread_label_ = -1;
  std::thread::id tid_;
};

enum TracingField {
  TRACE_OP,
  TRACE_TASK,
  TRACE_STREAM,
  TRACE_THREAD,
  TRACE_NAME,
  TRACE_CATEGORY,
};

class Tracer {
 public:
  Tracer() {}

  void init(const AsyncNetBase* net, const std::string& filename) {
    net_ = net;
    filename_ = filename;
  }

  void recordEvent(const TracerEvent& event) {
    std::lock_guard<std::mutex> lock(tracer_mutex_);
    events_.push_back(event);
  }

  // Special handling of shard blob annotations
  std::string opTraceName(const OperatorBase* op) {
    const auto& op_def = op->debug_def();
    std::unordered_set<int> shards;
    const std::string kShard = "shard:";
    int shard = 0;
    for (const auto& input : op_def.input()) {
      auto pos = input.find(kShard);
      if (pos != std::string::npos) {
        shard = input[pos + kShard.length()] - '0';
        shards.insert(shard);
      }
    }
    for (const auto& output : op_def.output()) {
      auto pos = output.find(kShard);
      if (pos != std::string::npos) {
        shard = output[pos + kShard.length()] - '0';
        shards.insert(shard);
      }
    }
    if (shards.size() == 1) {
      return op->type() + ":" + caffe2::to_string(shard);
    } else {
      return op->type();
    }
  }

  std::string opBlobsInfo(const OperatorBase& op) {
    std::string blobs_info;
    if (op.has_debug_def()) {
      blobs_info += "I: ";
      const auto& op_def = op.debug_def();
      for (const auto& input : op_def.input()) {
        blobs_info += input + "; ";
      }
      blobs_info += "O: ";
      for (const auto& output : op_def.output()) {
        blobs_info += output + "; ";
      }
    }
    return blobs_info;
  }

  std::string serializeEvent(const TracerEvent& event) {
    std::stringstream serialized_event;
    serialized_event << std::fixed;
    serialized_event << "{\n";
    serialized_event << " \"ts\": " << event.timestamp_ << ",\n";
    serialized_event << " \"pid\": 0,\n"; // not using pid field
    if (event.thread_label_ >= 0) {
      serialized_event << " \"tid\": " << event.thread_label_ << ",\n";
    } else {
      serialized_event << " \"tid\": " << event.tid_ << ",\n";
    }

    if (event.is_beginning_) {
      std::unordered_map<std::string, int> int_args;
      std::unordered_map<std::string, std::string> string_args;
      if (event.name_) {
        serialized_event << " \"name\": \"" << event.name_ << "\",\n";
      } else if (event.op_id_ >= 0) {
        auto* op = net_->op(event.op_id_);
        serialized_event << " \"name\": \"" << opTraceName(op) << "\",\n";
      } else {
        serialized_event << " \"name\": \"n/a\",\n";
      }

      if (event.category_) {
        serialized_event << " \"cat\": \"" << event.category_ << "\",\n";
      } else {
        serialized_event << " \"cat\": \"net\",\n";
      }

      if (event.op_id_ >= 0) {
        auto* op = net_->op(event.op_id_);
        int_args["op_id"] = event.op_id_;
        int_args["device_type"] = op->device_option().device_type();
        int_args["device_id"] = DeviceId(op->device_option());
        string_args["blobs"] = opBlobsInfo(*op);
      }

      if (event.task_id_ >= 0) {
        int_args["task_id"] = event.task_id_;
      }

      if (event.stream_id_ >= 0) {
        int_args["stream_id"] = event.stream_id_;
      }

      serialized_event << " \"ph\": \"B\"";
      if (!int_args.empty() || !string_args.empty()) {
        serialized_event << ",\n \"args\": {\n";
        auto left_to_output = int_args.size() + string_args.size();
        for (const auto& kv : int_args) {
          serialized_event << "  \"" << kv.first << "\": " << kv.second;
          --left_to_output;
          if (left_to_output > 0) {
            serialized_event << ",\n";
          }
        }
        for (const auto& kv : string_args) {
          serialized_event << "  \"" << kv.first << "\": \"" << kv.second
                           << "\"";
          --left_to_output;
          if (left_to_output > 0) {
            serialized_event << ",\n";
          }
        }
        serialized_event << "\n }";
      }
    } else {
      serialized_event << " \"ph\": \"E\"\n";
    }
    serialized_event << "\n}";

    return serialized_event.str();
  }

  // fix occasional cases with zero duration events
  void linearizeEvents() {
    std::unordered_map<long, long> time_offsets;
    std::unordered_map<long, long> last_times;
    std::hash<std::thread::id> hasher;
    const long time_eps = 1; // us
    for (auto& event : events_) {
      long tid =
          (event.thread_label_ >= 0) ? event.thread_label_ : hasher(event.tid_);
      auto event_ts = event.timestamp_;
      if (last_times.count(tid)) {
        event_ts += time_offsets[tid];
        CAFFE_ENFORCE(event_ts >= last_times[tid]);
        if (event_ts <= last_times[tid] + time_eps) {
          event_ts += time_eps;
          time_offsets[tid] += time_eps;
        } else if (event_ts > last_times[tid] + 2 * time_eps) {
          long eps_len = (event_ts - last_times[tid]) / time_eps;
          if (time_offsets[tid] >= time_eps * (eps_len - 1)) {
            time_offsets[tid] -= time_eps * (eps_len - 1);
            event_ts -= time_eps * (eps_len - 1);
          } else {
            event_ts -= time_offsets[tid];
            time_offsets[tid] = 0;
          }
        }
        event.timestamp_ = event_ts;
        last_times[tid] = event_ts;
      } else {
        last_times[tid] = event_ts;
        time_offsets[tid] = 0;
      }
    }
  }

  void renameThreads() {
    std::unordered_map<long, int> tids;
    std::unordered_map<int, int> numa_counters;
    std::unordered_map<long, int> tid_to_numa;
    std::hash<std::thread::id> hasher;
    const long numa_multiplier = 10e9;
    for (auto& event : events_) {
      if (event.thread_label_ >= 0 || event.op_id_ < 0) {
        continue;
      }
      auto* op = net_->op(event.op_id_);
      int numa_node_id = DeviceId(op->device_option());
      if (numa_node_id < 0) {
        continue;
      }
      long tid = hasher(event.tid_);

      if (!tid_to_numa.count(tid)) {
        tid_to_numa[tid] = numa_node_id;
      } else {
        CAFFE_ENFORCE_EQ(tid_to_numa[tid], numa_node_id);
      }

      if (!numa_counters.count(numa_node_id)) {
        numa_counters[numa_node_id] = 1;
      }
      if (!tids.count(tid)) {
        tids[tid] = numa_counters[numa_node_id]++;
      }
      event.thread_label_ = numa_multiplier * (numa_node_id + 1) + tids[tid];
    }
  }

  virtual ~Tracer() {
    if (events_.empty() || filename_.empty()) {
      return;
    }
    linearizeEvents();
    renameThreads();
    std::stringstream serialized;
    serialized << "[\n";
    for (auto idx = 0; idx < events_.size(); ++idx) {
      serialized << serializeEvent(events_[idx]);
      if (idx != events_.size() - 1) {
        serialized << ",\n";
      }
    }
    serialized << "\n]\n";
    WriteStringToFile(serialized.str(), filename_.c_str());
  }

 private:
  const AsyncNetBase* net_ = nullptr;
  std::string filename_;
  std::vector<TracerEvent> events_;
  std::mutex tracer_mutex_;
};

class TracerGuard {
 public:
  TracerGuard() : enabled_(false) {}

  void initialize(Tracer* tracer, Timer* timer) {
    enabled_ = true;
    tracer_ = tracer;
    timer_ = timer;
  }

  void addArgument() {}

  void addArgument(TracingField field, const char* value) {
    switch (field) {
      case TRACE_NAME: {
        event_.name_ = value;
        break;
      }
      case TRACE_CATEGORY: {
        event_.category_ = value;
        break;
      }
      default: { CAFFE_THROW("Unexpected tracing string field ", field); }
    }
  }

  void addArgument(TracingField field, int value) {
    switch (field) {
      case TRACE_OP: {
        event_.op_id_ = value;
        break;
      }
      case TRACE_TASK: {
        event_.task_id_ = value;
        break;
      }
      case TRACE_STREAM: {
        event_.stream_id_ = value;
        break;
      }
      case TRACE_THREAD: {
        event_.thread_label_ = value;
        break;
      }
      default: { CAFFE_THROW("Unexpected tracing int field ", field); }
    }
  }

  template <typename T, typename... Args>
  void addArgument(TracingField field, const T& value, const Args&... args) {
    addArgument(field, value);
    addArgument(args...);
  }

  void recordEventStart() {
    if (enabled_) {
      if (event_.thread_label_ < 0) {
        event_.tid_ = std::this_thread::get_id();
      }
      event_.is_beginning_ = true;
      event_.timestamp_ = (long)std::round(timer_->MicroSeconds());
      tracer_->recordEvent(event_);
    }
  }

  virtual ~TracerGuard() {
    if (enabled_) {
      event_.is_beginning_ = false;
      event_.timestamp_ = (long)std::round(timer_->MicroSeconds());
      tracer_->recordEvent(event_);
    }
  }

 private:
  bool enabled_;
  TracerEvent event_;
  Tracer* tracer_;
  Timer* timer_;
};

} // namespace tracing

#define TRACE_NAME_CONCATENATE(s1, s2) s1##s2
#define TRACE_ANONYMOUS_NAME(str) TRACE_NAME_CONCATENATE(str, __LINE__)

#define TRACE_EVENT_INIT(...)                                           \
  TRACE_ANONYMOUS_NAME(trace_guard).initialize(tracer_.get(), &timer_); \
  TRACE_ANONYMOUS_NAME(trace_guard).addArgument(__VA_ARGS__);           \
  TRACE_ANONYMOUS_NAME(trace_guard).recordEventStart();

// Supposed to be used only once per scope in AsyncNetBase-derived nets
#define TRACE_EVENT(...)                                  \
  tracing::TracerGuard TRACE_ANONYMOUS_NAME(trace_guard); \
  if (trace_batch_) {                                     \
    TRACE_EVENT_INIT(__VA_ARGS__)                         \
  }

#define TRACE_EVENT_IF(cond, ...)                         \
  tracing::TracerGuard TRACE_ANONYMOUS_NAME(trace_guard); \
  if (trace_batch_ && (cond)) {                           \
    TRACE_EVENT_INIT(__VA_ARGS__)                         \
  }

} // namespace caffe2

#endif // CAFFE2_CORE_NET_ASYNC_TRACING_H_
