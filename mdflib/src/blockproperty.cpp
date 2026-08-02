/*
 * Copyright 2021 Ingemar Hedvall
 * SPDX-License-Identifier: MIT
 */
#include "blockproperty.h"

#include <sstream>
#include <utility>

namespace mdf::detail {

BlockProperty::BlockProperty(std::string label, std::string value,
                             std::string desc, BlockItemType type)
    : label_(std::move(label)), value_(std::move(value)),
      description_(std::move(desc)), type_(type) {}
/// Converts the the string value to a file position of the block.
/// It assumes the value is formatted as an hex value so this function
/// is only valid for link property types.
/// \return File position of the referenced block
int64_t BlockProperty::Link() const {
  if (Type() != BlockItemType::LinkItem) {
    return 0;
  }
  // Skip any initial 0x prefix
  if (value_.size() <= 2) {
    return 0;
  }
  int64_t file_pos = 0;
  try {
    std::istringstream temp(value_.substr(2));
    temp.imbue(std::locale::classic());
    temp >> std::hex >> file_pos;
  } catch (const std::exception &) {
    return 0;
  }
  return file_pos;
}

}  // namespace mdf::detail