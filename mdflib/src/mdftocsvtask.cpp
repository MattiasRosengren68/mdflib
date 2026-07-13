/*
* Copyright 2026 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */



#include "mdftocsvtask.h"

#include <filesystem>
#include <sstream>
#include <set>

#include "mdf/mdflogstream.h"
#include "mdf/csvwriter.h"

using namespace std::filesystem;

namespace mdf {
void MdfToCsvTask::Run() {
  orig_dest_ = DestinationFile();
  try {

    Result(false);
    CheckSourceFile();
    ReadConfig();
    CreateCsvList();
    for (CsvReadGroup& read_group : csv_list_) {
      IDataGroup* data_group = read_group.data_group;
      if (data_group == nullptr || !reader_) {
        throw std::invalid_argument("Invalid data group or no reader.");
      }
      if (!reader_) {
        throw std::runtime_error("No MDF reader defined.");
      }

      const bool data = reader_->ReadData(*data_group);
      if (!data) {
        throw std::runtime_error("Read failure.");
      }
      for (CsvFileGroup& file_group : read_group.group_list) {
        DestinationFile(file_group.csv_filename);
        CheckDestinationFile();
        CreateDestinationTempFile();
        CreateCsvFile(file_group);
        CopyTempFile();
        DeleteTempFile();
      }
    }
    Error(false);
    Result(true);
  } catch (const std::exception& err) {
    std::ostringstream oss;
    oss << "Failed to run the MDF to CSV task. Error: " << err.what()
        << ", Source: " << SourceFile();
    MDF_ERROR() << oss.str();
    Error(true);
    SaveMessage(oss.str());
  }
  DestinationFile(orig_dest_);
}

void MdfToCsvTask::CreateCsvList() {
  csv_list_.clear();
  if (!reader_) {
    throw std::runtime_error("Reader is not initialized.");
  }

  const IHeader* header = reader_->GetHeader();
  if (header == nullptr) {
    MDF_INFO() << ". Skipping. No file header.";
    return;
  }
  const auto dg_list = header->DataGroups();
  if (dg_list.empty()) {
    // Nothing to convert
    MDF_INFO() << ". Skipping. No data groups.";
    return;
  }

  size_t measure_no = 0; // Used to set a unique CSV name.
  for (auto* dg_group : dg_list) {
    if (dg_group == nullptr) {
      continue;
    }
    ++measure_no;
    size_t group_no = 0; // Used if the CG doesn't have a name.

    // Create a internal list of CG items.
    // Remove empty groups (VLSD or no samples)
    std::vector<IChannelGroup*> cg_list;
    const auto temp_list = dg_group->ChannelGroups();
    for (auto* temp_group : temp_list) {
      if (temp_group == nullptr) {
        continue;
      }
      if ((temp_group->Flags() & CgFlag::VlsdChannel) != 0) {
        continue;
      }
      if (temp_group->NofSamples() == 0) {
        continue;
      }
      cg_list.emplace_back(temp_group);
    }
    if (cg_list.empty()) {
      continue;
    }

    CsvReadGroup read_group;
    read_group.data_group = dg_group;
    std::set<std::string> name_count;
    bool name_error = false;
    for (auto* cg_group : cg_list) {
      if (cg_group == nullptr) {
        continue;
      }
      const std::string name = cg_group->Name();
      if (name_count.find(name) != name_count.end()) {
        name_error = true;
        break;
      }
      name_count.insert(name);
    }

    for (auto* cg_group : cg_list) {
      if (cg_group == nullptr) {
        continue;
      }
      ++group_no;

      // Create a CSV name
      const path fullname(DestinationFile());
      const path dest_path = fullname.parent_path();
      const path stem = fullname.stem();
      const path ext = fullname.extension();

      path dest_file(dest_path);
      dest_file /= stem;

      if (dg_list.size() > 1) {
        dest_file += "_DG_";
        dest_file += std::to_string(measure_no);
      }
      const std::string cg_name = name_error ? std::string() : cg_group->Name();
      if (cg_list.size() > 1 && cg_name.empty()) {
        dest_file += "_CG_";
        dest_file +=  std::to_string(group_no);
      } else if (cg_list.size() > 1) {
        dest_file += "_";
        dest_file += cg_name;
      }
      dest_file += ext;

      CsvFileGroup group;
      group.csv_filename = dest_file.string();
      group.channel_group = cg_group;
      CreateChannelObserverForChannelGroup(*dg_group, *cg_group,
        group.observer_list);
      read_group.group_list.emplace_back(std::move(group));

    }
    csv_list_.emplace_back(std::move(read_group));
  }
  if (csv_list_.size() == 1 && csv_list_[0].group_list.size() == 1) {
    csv_list_[0].group_list[0].csv_filename = DestinationFile();
  }
}

void MdfToCsvTask::CreateCsvFile(CsvFileGroup& file_group) const {
  IChannelGroup* channel_group = file_group.channel_group;
  if (channel_group == nullptr) {
    throw std::invalid_argument("Invalid channel group");
  }
  const bool bus_log_file = (channel_group->Flags() & CgFlag::BusEvent) != 0;
  auto& observer_list = file_group.observer_list;

  if (bus_log_file) {
    for (auto itr = observer_list.begin();
      itr != observer_list.end();  /* No ++itr here */ ) {

      if (itr->get() == nullptr) {
        itr = observer_list.erase(itr);
        continue;
      }
      constexpr std::array<std::string_view, 4> exclude_list = {
        "CAN_DataFrame","CAN_RemoteFrame", "CAN_ErrorFrame", "CAN_OverloadFrame"
      };
      const auto& channel = itr->get()->Channel();
      const std::string channel_name = channel.Name();
      const bool exclude = std::any_of(exclude_list.cbegin(),
                                 exclude_list.cend(),
                                [&] (const std::string_view& name) ->bool {
                                         return channel_name == name;
                                 });
      if (exclude) {
        itr = observer_list.erase(itr);
      } else {
        ++itr;
      }
    }
  }

  if (observer_list.empty()) {
    return;
  }

  CsvWriter writer(TempFile());
  if (!writer.IsOk()) {
    throw std::runtime_error("CSV writer failure.");
  }
  writer.Convert(observer_list);
}

} // mdf