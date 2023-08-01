//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2023
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "td/telegram/Location.h"
#include "td/telegram/MediaAreaCoordinates.h"
#include "td/telegram/td_api.h"
#include "td/telegram/telegram_api.h"
#include "td/telegram/Venue.h"

#include "td/utils/common.h"
#include "td/utils/StringBuilder.h"

namespace td {

class Td;

class MediaArea {
  enum class Type : int32 { None, Location, Venue };
  Type type_ = Type::None;
  MediaAreaCoordinates coordinates_;
  Location location_;
  Venue venue_;

  friend bool operator==(const MediaArea &lhs, const MediaArea &rhs);
  friend bool operator!=(const MediaArea &lhs, const MediaArea &rhs);

  friend StringBuilder &operator<<(StringBuilder &string_builder, const MediaArea &media_area);

 public:
  MediaArea() = default;

  MediaArea(Td *td, telegram_api::object_ptr<telegram_api::MediaArea> &&media_area_ptr);

  td_api::object_ptr<td_api::storyArea> get_story_area_object() const;

  bool is_valid() const {
    return type_ != Type::None;
  }

  template <class StorerT>
  void store(StorerT &storer) const;

  template <class ParserT>
  void parse(ParserT &parser);
};

bool operator==(const MediaArea &lhs, const MediaArea &rhs);
bool operator!=(const MediaArea &lhs, const MediaArea &rhs);

StringBuilder &operator<<(StringBuilder &string_builder, const MediaArea &media_area);

}  // namespace td
