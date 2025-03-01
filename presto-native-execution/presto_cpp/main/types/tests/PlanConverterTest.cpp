/*
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
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <gtest/gtest.h>

#include "presto_cpp/main/common/tests/test_json.h"
#include "presto_cpp/main/types/PrestoToVeloxQueryPlan.h"
#include "presto_cpp/presto_protocol/Connectors.h"
#include "presto_cpp/presto_protocol/presto_protocol.h"

namespace fs = boost::filesystem;

using namespace facebook::presto;
using namespace facebook::velox;

namespace {
std::string getDataPath(const std::string& fileName) {
  std::string current_path = fs::current_path().c_str();
  if (boost::algorithm::ends_with(current_path, "fbcode")) {
    return current_path + "/presto_cpp/main/types/tests/data/" + fileName;
  }
  if (boost::algorithm::ends_with(current_path, "fbsource")) {
    return current_path + "/third-party/presto_cpp/main/types/tests/data/" +
        fileName;
  }
  return current_path + "/data/" + fileName;
}

std::shared_ptr<const core::PlanNode> assertToVeloxQueryPlan(
    const std::string& fileName) {
  std::string fragment = slurp(getDataPath(fileName));

  protocol::PlanFragment prestoPlan = json::parse(fragment);
  auto scopedPool = memory::getDefaultScopedMemoryPool();

  VeloxQueryPlanConverter converter(scopedPool.get());
  return converter
      .toVeloxQueryPlan(
          prestoPlan, nullptr, "20201107_130540_00011_wrpkw.1.2.3")
      .planNode;
}
} // namespace

// Leaf stage plan for select regionkey, sum(1) from nation group by 1
// Scan + Partial Agg + Repartitioning
TEST(PlanConverterTest, scanAgg) {
  protocol::registerConnector("hive", "hive");
  assertToVeloxQueryPlan("ScanAgg.json");

  protocol::registerConnector("hive-plus", "hive");
  assertToVeloxQueryPlan("ScanAggCustomConnectorId.json");
}

// Final Agg stage plan for select regionkey, sum(1) from nation group by 1
TEST(PlanConverterTest, finalAgg) {
  assertToVeloxQueryPlan("FinalAgg.json");
}

// Last stage (output) plan for select regionkey, sum(1) from nation group by 1
TEST(PlanConverterTest, output) {
  assertToVeloxQueryPlan("Output.json");
}

// Last stage plan for SELECT * FROM nation ORDER BY nationkey OFFSET 7 LIMIT 5.
TEST(PlanConverterTest, offsetLimit) {
  auto plan = assertToVeloxQueryPlan("OffsetLimit.json");

  // Look for Limit(offset = 7, count = 5) node
  bool foundLimit = false;
  auto node = plan;
  while (node) {
    node = node->sources()[0];
    if (auto limit = std::dynamic_pointer_cast<const LimitNode>(node)) {
      ASSERT_EQ(7, limit->offset());
      ASSERT_EQ(5, limit->count());
      foundLimit = true;
      break;
    }
  }

  ASSERT_TRUE(foundLimit);
}
