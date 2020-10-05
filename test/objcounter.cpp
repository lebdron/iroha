/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "obj_counter.hpp"

const char *const kObjCounterOutDir = std::getenv("OBJ_COUNTER_OUT_DIR");
size_t obj_counter_out_iteration = 0;
void printCountedObjectsStats(int s) {
  auto file_path_format = fmt::format(
      "{}/{:>08}_{{}}", kObjCounterOutDir, obj_counter_out_iteration);
  AllCountedStats::getAllStats(file_path_format);
  ++obj_counter_out_iteration;
}

std::mutex AllCountedStats::mu_;
std::vector<AllCountedStats::GetStatsFn> AllCountedStats::get_stats_;
