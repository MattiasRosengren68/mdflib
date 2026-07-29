/*
* Copyright 2026 Ingemar Hedvall
* SPDX-License-Identifier: MIT
*/

#pragma once
#include <gtest/gtest.h>

#include "mdf/mdftask.h"

namespace mdf::test {

struct TestDataParameter {
  bool mandatory_only = false;
  bool compress_data = false;
  MdfStorageType storage_type = MdfStorageType::FixedLengthStorage;
};

class TestSortingData : public ::testing::TestWithParam<TestDataParameter> {
public:
  static void SetUpTestSuite();
  static void TearDownTestSuite();

protected:
  static void TestDataFile(const std::wstring& filename,
    bool sorted, size_t max_sample,
    const std::vector<uint8_t>& sample_data);
};


}  // namespace mdf::test
