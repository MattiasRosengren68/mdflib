/*
* Copyright 2026 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "mdf/samplerecordobserver.h"

namespace mdf {

class CopySampleObserver : public SampleRecordObserver {
public:
  CopySampleObserver() = delete;
  CopySampleObserver(const IDataGroup& source_data_group,
                     const IChannelGroup& source_channel_group,
                     uint64_t base_time,
                     MdfWriter& writer,
                     const IDataGroup& dest_data_group,
                     const IChannelGroup& dest_channel_group);

  void OnSampleRecord() override;

  void SetTimeRange(double min_time, double max_time);

private:
  MdfWriter& writer_;
  const IDataGroup& dest_data_group_;
  const IChannelGroup& dest_channel_group_;

  uint64_t min_time_ = 0; ///< Min absolute time (ns)
  uint64_t max_time_ = 0; ///< Max absolute time (ns)

  [[nodiscard]] bool IgnoreSample(uint64_t sample_time) const;
};

}  // namespace mdf

