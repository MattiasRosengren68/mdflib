/*
 * Copyright 2021 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */

#include <cstdint>

#include "ce3block.h"

#include "mdf/mdfhelper.h"

namespace mdf::detail {
uint64_t Ce3Block::Read(std::streambuf& buffer) {
  uint64_t bytes = ReadHeader3(buffer);
  bytes += ReadNumber(buffer, type_);
  switch (type_) {
    case 2:
      bytes += ReadNumber(buffer, nof_module_);
      bytes += ReadNumber(buffer, address_);
      bytes += ReadStr(buffer, description_, 80);
      bytes += ReadStr(buffer, ecu_, 32);
      break;

    case 19:
      bytes += ReadNumber(buffer, message_id_);
      bytes += ReadNumber(buffer, index_);
      bytes += ReadStr(buffer, message_, 36);
      bytes += ReadStr(buffer, sender_, 36);
      break;

    default:
      break;
  }

  return bytes;
}

void Ce3Block::GetBlockProperty(BlockPropertyList &dest) const {
  MdfBlock::GetBlockProperty(dest);

  dest.emplace_back("Information", "", "", BlockItemType::HeaderItem);
  switch (type_) {
    case 2:
      dest.emplace_back("Extension Type", "DIM");
      dest.emplace_back("Number of Modules",
        std::to_string(nof_module_));
      dest.emplace_back("Address",
        MdfHelper::FormatHex(address_));
      dest.emplace_back("Description", description_);
      dest.emplace_back("ECU", ecu_);
      break;

    case 19:
      dest.emplace_back("Extension Type", "Vector CAN");
      dest.emplace_back("Message ID",
        MdfHelper::FormatHex(message_id_));
      dest.emplace_back("Channel",std::to_string(index_));
      dest.emplace_back("Message Name", message_);
      dest.emplace_back("Sender", sender_);
      break;

    default:
      dest.emplace_back("Extension Type", std::to_string(type_));
      break;
  }
}

}  // namespace mdf::detail
