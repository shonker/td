//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2023
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "td/telegram/StoryInteractionInfo.h"

#include "td/utils/common.h"
#include "td/utils/tl_helpers.h"

namespace td {

template <class StorerT>
void StoryInteractionInfo::store(StorerT &storer) const {
  using td::store;
  bool has_recent_viewer_user_ids = !recent_viewer_user_ids_.empty();
  bool has_reaction_count = reaction_count_ > 0;
  BEGIN_STORE_FLAGS();
  STORE_FLAG(has_recent_viewer_user_ids);
  STORE_FLAG(has_reaction_count);
  END_STORE_FLAGS();
  store(view_count_, storer);
  if (has_recent_viewer_user_ids) {
    store(recent_viewer_user_ids_, storer);
  }
  if (has_reaction_count) {
    store(reaction_count_, storer);
  }
}

template <class ParserT>
void StoryInteractionInfo::parse(ParserT &parser) {
  using td::parse;
  bool has_recent_viewer_user_ids;
  bool has_reaction_count;
  BEGIN_PARSE_FLAGS();
  PARSE_FLAG(has_recent_viewer_user_ids);
  PARSE_FLAG(has_reaction_count);
  END_PARSE_FLAGS();
  parse(view_count_, parser);
  if (has_recent_viewer_user_ids) {
    parse(recent_viewer_user_ids_, parser);
  }
  if (has_reaction_count) {
    parse(reaction_count_, parser);
  }
}

}  // namespace td
