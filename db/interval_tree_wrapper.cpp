//
// Created by Baoquan Zhang on 2.18
//
#include "interval_tree_wrapper.h"
#include "leveldb/db.h"

#include <chrono>
#include <thread>

void interval_tree_wrapper::add_interval(
    const std::string& start_key, const std::string& end_key,
    uint64_t sst, uint64_t offset, uint64_t block_size) {
  size_++;
  std::string start_str = start_key.substr(0, 16);
  std::string end_str = end_key.substr(0, 16);
  target cur_target(sst, offset, block_size);
  uint64_t current_reads = std::log((double)size_);
  std::this_thread::sleep_for(
      std::chrono::nanoseconds(NVM_read_latency_ * current_reads)
      + std::chrono::nanoseconds(NVM_write_latency_));
  mem_reads_ += current_reads;
  mem_writes_++;
  intervals.insert({start_str, end_str, cur_target});
  files.emplace(sst);
}


std::vector<target> interval_tree_wrapper::find_overlap(
    const std::string& start, const std::string& end) {
  //auto overlapped_intervals = intervals.intervals();
  auto overlapped_intervals = intervals.findOverlappingIntervals({start, end});
  std::vector<target> targets(overlapped_intervals.size());
  for (const auto& overlapped_interval : overlapped_intervals) {
    targets.push_back(overlapped_interval.value);
  }
  // std::cout << "Target Size: " << targets.size() << std::endl;
  return targets;
}

std::vector<target> interval_tree_wrapper::find_point(const std::string& key) {
  uint64_t current_reads = std::log((double) size_);
  mem_reads_ += current_reads;
  auto overlapped_intervals = intervals.findIntervalsContainPoint(key);
  std::vector<target> targets;
  targets.reserve(overlapped_intervals.size());
  for (const auto& overlapped_interval : overlapped_intervals) {
    mem_reads_++;
    uint64_t file_id = overlapped_interval.value.file_id_;
    if (leveldb::sst_filter.count(file_id) == 0
        || !(leveldb::sst_filter[file_id]->contains(key))) {
      continue;
    }
    targets.push_back(overlapped_interval.value);
  }
  std::this_thread::sleep_for(
      std::chrono::nanoseconds(NVM_read_latency_ * current_reads));
  // std::cout << "Target Size: " << targets.size() << std::endl;
  return targets;
}

void interval_tree_wrapper::clear() {
  files.clear();
  intervals.clear();
  size_ = 0;
  reset_mem_reads();
  reset_mem_writes();
  reset_overlap();
}

void interval_tree_wrapper::lock() {
  mutex_.lock();
}

void interval_tree_wrapper::unlock() {
  mutex_.unlock();
}

uint64_t interval_tree_wrapper::delete_by_file(const std::unordered_set<uint64_t>& files_to_delete) {
  uint64_t deleted_files = 0;
  auto all_intervals = intervals.intervals();
  for (const auto& cur_interval : all_intervals) {
    uint64_t file_id_in_target = cur_interval.value.file_id_;
    if (files_to_delete.count(file_id_in_target) > 0) {
      intervals.remove(cur_interval);
      size_--;
      deleted_files++;
    }
  }
  for (const auto& fileid : files_to_delete) {
    this->files.erase(fileid);
  }
  return deleted_files;
}

void interval_tree_wrapper::display_intervals() {
  std::cout << "######Start to display intervals:######" << std::endl;
  auto all_intervals = intervals.intervals();
  for(auto& interval : all_intervals) {
    std::cout << "<" << interval.low
              << "," << interval.high
              << ">" << ":";
    std::cout << interval.value.file_id_ << "," << interval.value.block_offset_ << std::endl;
  }
  std::cout << std::endl;
  std::cout << "######Finish to display intervals:######" << std::endl;
}

std::unordered_set<uint64_t> interval_tree_wrapper::get_files() {
  return files;
}