/*
* Copyright 2026 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>
#include <deque>

#include "mdf/mdftask.h"
#include "mdf/idatagroup.h"
#include "mdf/ichannelgroup.h"

namespace mdf {

struct CsvFileGroup {
  IChannelGroup* channel_group = nullptr;
  std::string csv_filename;
  ChannelObserverList observer_list;
};

struct CsvReadGroup {
  IDataGroup* data_group = nullptr;
  std::deque<CsvFileGroup> group_list;
};

class MdfToCsvTask : public MdfTask {
public:
  MdfToCsvTask() = default;
  ~MdfToCsvTask() override = default;

  void Run() override;
private:
  std::string orig_dest_;
  std::deque<CsvReadGroup> csv_list_;
  void CreateCsvList();
  void CreateCsvFile(CsvFileGroup& group) const;

};

}  // namespace a2l

