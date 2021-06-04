//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2021
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "td/telegram/BackgroundType.h"

#include "td/utils/HttpUrl.h"
#include "td/utils/logging.h"
#include "td/utils/misc.h"
#include "td/utils/Slice.h"
#include "td/utils/SliceBuilder.h"

namespace td {

static string get_color_hex_string(int32 color) {
  string result;
  for (int i = 20; i >= 0; i -= 4) {
    result += "0123456789abcdef"[(color >> i) & 0xF];
  }
  return result;
}

static bool is_valid_color(int32 color) {
  return 0 <= color && color <= 0xFFFFFF;
}

static bool is_valid_rotation_angle(int32 rotation_angle) {
  return 0 <= rotation_angle && rotation_angle < 360 && rotation_angle % 45 == 0;
}

BackgroundFill::BackgroundFill(const telegram_api::wallPaperSettings *settings) {
  if (settings == nullptr) {
    return;
  }

  auto flags = settings->flags_;
  if ((flags & telegram_api::wallPaperSettings::BACKGROUND_COLOR_MASK) != 0) {
    top_color = settings->background_color_;
    if (!is_valid_color(top_color)) {
      LOG(ERROR) << "Receive " << to_string(*settings);
      top_color = 0;
    }
  }
  if ((flags & telegram_api::wallPaperSettings::FOURTH_BACKGROUND_COLOR_MASK) != 0 ||
      (flags & telegram_api::wallPaperSettings::THIRD_BACKGROUND_COLOR_MASK) != 0) {
    bottom_color = settings->second_background_color_;
    if (!is_valid_color(bottom_color)) {
      LOG(ERROR) << "Receive " << to_string(*settings);
      bottom_color = 0;
    }
    third_color = settings->third_background_color_;
    if (!is_valid_color(third_color)) {
      LOG(ERROR) << "Receive " << to_string(*settings);
      third_color = 0;
    }
    if ((flags & telegram_api::wallPaperSettings::FOURTH_BACKGROUND_COLOR_MASK) != 0) {
      fourth_color = settings->fourth_background_color_;
      if (!is_valid_color(fourth_color)) {
        LOG(ERROR) << "Receive " << to_string(*settings);
        fourth_color = 0;
      }
    }
  } else if ((flags & telegram_api::wallPaperSettings::SECOND_BACKGROUND_COLOR_MASK) != 0) {
    bottom_color = settings->second_background_color_;
    if (!is_valid_color(bottom_color)) {
      LOG(ERROR) << "Receive " << to_string(*settings);
      bottom_color = 0;
    }

    rotation_angle = settings->rotation_;
    if (!is_valid_rotation_angle(rotation_angle)) {
      LOG(ERROR) << "Receive " << to_string(*settings);
      rotation_angle = 0;
    }
  }
}

static Result<BackgroundFill> get_background_fill(const td_api::BackgroundFill *fill) {
  if (fill == nullptr) {
    return Status::Error(400, "Background fill info must be non-empty");
  }
  switch (fill->get_id()) {
    case td_api::backgroundFillSolid::ID: {
      auto solid = static_cast<const td_api::backgroundFillSolid *>(fill);
      if (!is_valid_color(solid->color_)) {
        return Status::Error(400, "Invalid solid fill color value");
      }
      return BackgroundFill(solid->color_);
    }
    case td_api::backgroundFillGradient::ID: {
      auto gradient = static_cast<const td_api::backgroundFillGradient *>(fill);
      if (!is_valid_color(gradient->top_color_)) {
        return Status::Error(400, "Invalid top gradient color value");
      }
      if (!is_valid_color(gradient->bottom_color_)) {
        return Status::Error(400, "Invalid bottom gradient color value");
      }
      if (!is_valid_rotation_angle(gradient->rotation_angle_)) {
        return Status::Error(400, "Invalid rotation angle value");
      }
      return BackgroundFill(gradient->top_color_, gradient->bottom_color_, gradient->rotation_angle_);
    }
    case td_api::backgroundFillFreeformGradient::ID: {
      auto freeform = static_cast<const td_api::backgroundFillFreeformGradient *>(fill);
      if (freeform->colors_.size() != 3 && freeform->colors_.size() != 4) {
        return Status::Error(400, "Wrong number of gradient colors");
      }
      for (auto &color : freeform->colors_) {
        if (!is_valid_color(color)) {
          return Status::Error(400, "Invalid freeform gradient color value");
        }
      }
      return BackgroundFill(freeform->colors_[0], freeform->colors_[1], freeform->colors_[2],
                            freeform->colors_.size() == 3 ? -1 : freeform->colors_[3]);
    }
    default:
      UNREACHABLE();
      return {};
  }
}

Result<BackgroundFill> BackgroundFill::get_background_fill(Slice name) {
  name = name.substr(0, name.find('#'));

  Slice parameters;
  auto parameters_pos = name.find('?');
  if (parameters_pos != Slice::npos) {
    parameters = name.substr(parameters_pos + 1);
    name = name.substr(0, parameters_pos);
  }

  auto get_color = [](Slice color_string) -> Result<int32> {
    auto r_color = hex_to_integer_safe<uint32>(color_string);
    if (r_color.is_error() || color_string.size() > 6) {
      return Status::Error(400, "WALLPAPER_INVALID");
    }
    return static_cast<int32>(r_color.ok());
  };

  size_t hyphen_pos = name.find('-');
  if (name.find('~') < name.size()) {
    vector<Slice> color_strings = full_split(name, '~');
    CHECK(color_strings.size() >= 2);
    if (color_strings.size() == 2) {
      hyphen_pos = color_strings[0].size();
    } else {
      if (color_strings.size() > 4) {
        return Status::Error(400, "WALLPAPER_INVALID");
      }

      TRY_RESULT(first_color, get_color(color_strings[0]));
      TRY_RESULT(second_color, get_color(color_strings[1]));
      TRY_RESULT(third_color, get_color(color_strings[2]));
      int32 fourth_color = -1;
      if (color_strings.size() == 4) {
        TRY_RESULT_ASSIGN(fourth_color, get_color(color_strings[3]));
      }
      return BackgroundFill(first_color, second_color, third_color, fourth_color);
    }
  }

  if (hyphen_pos < name.size()) {
    TRY_RESULT(top_color, get_color(name.substr(0, hyphen_pos)));
    TRY_RESULT(bottom_color, get_color(name.substr(hyphen_pos + 1)));
    int32 rotation_angle = 0;

    Slice prefix("rotation=");
    if (begins_with(parameters, prefix)) {
      rotation_angle = to_integer<int32>(parameters.substr(prefix.size()));
      if (!is_valid_rotation_angle(rotation_angle)) {
        rotation_angle = 0;
      }
    }

    return BackgroundFill(top_color, bottom_color, rotation_angle);
  }

  TRY_RESULT(color, get_color(name));
  return BackgroundFill(color);
}

static string get_background_fill_color_hex_string(const BackgroundFill &fill, bool is_first) {
  switch (fill.get_type()) {
    case BackgroundFill::Type::Solid:
      return get_color_hex_string(fill.top_color);
    case BackgroundFill::Type::Gradient:
      return PSTRING() << get_color_hex_string(fill.top_color) << '-' << get_color_hex_string(fill.bottom_color)
                       << (is_first ? '?' : '&') << "rotation=" << fill.rotation_angle;
    case BackgroundFill::Type::FreeformGradient: {
      SliceBuilder sb;
      sb << get_color_hex_string(fill.top_color) << '~' << get_color_hex_string(fill.bottom_color) << '~'
         << get_color_hex_string(fill.third_color);
      if (fill.fourth_color != -1) {
        sb << '~' << get_color_hex_string(fill.fourth_color);
      }
      return sb.as_cslice().str();
    }
    default:
      UNREACHABLE();
      return string();
  }
}

static bool is_valid_intensity(int32 intensity) {
  return -100 <= intensity && intensity <= 100;
}

int64 BackgroundFill::get_id() const {
  CHECK(is_valid_color(top_color));
  CHECK(is_valid_color(bottom_color));
  switch (get_type()) {
    case Type::Solid:
      return static_cast<int64>(top_color) + 1;
    case Type::Gradient:
      CHECK(is_valid_rotation_angle(rotation_angle));
      return (rotation_angle / 45) * 0x1000001000001 + (static_cast<int64>(top_color) << 24) + bottom_color +
             (1 << 24) + 1;
    case Type::FreeformGradient: {
      CHECK(is_valid_color(third_color));
      CHECK(fourth_color == -1 || is_valid_color(fourth_color));
      const uint64 mul = 123456789;
      uint64 result = static_cast<uint64>(top_color);
      result = result * mul + static_cast<uint64>(bottom_color);
      result = result * mul + static_cast<uint64>(third_color);
      result = result * mul + static_cast<uint64>(fourth_color);
      return 0x8000008000008 + static_cast<int64>(result % 0x8000008000008);
    }
    default:
      UNREACHABLE();
      return 0;
  }
}

bool BackgroundFill::is_dark() const {
  switch (get_type()) {
    case Type::Solid:
      return (top_color & 0x808080) == 0;
    case Type::Gradient:
      return (top_color & 0x808080) == 0 && (bottom_color & 0x808080) == 0;
    case Type::FreeformGradient:
      return (top_color & 0x808080) == 0 && (bottom_color & 0x808080) == 0 && (third_color & 0x808080) == 0 &&
             (fourth_color == -1 || (fourth_color & 0x808080) == 0);
    default:
      UNREACHABLE();
      return 0;
  }
}

bool BackgroundFill::is_valid_id(int64 id) {
  return 0 < id && id < 0x8000008000008 * 2;
}

bool operator==(const BackgroundFill &lhs, const BackgroundFill &rhs) {
  return lhs.top_color == rhs.top_color && lhs.bottom_color == rhs.bottom_color &&
         lhs.rotation_angle == rhs.rotation_angle && lhs.third_color == rhs.third_color &&
         lhs.fourth_color == rhs.fourth_color;
}

void BackgroundType::apply_parameters_from_link(Slice name) {
  const auto query = parse_url_query(name);

  is_blurred = false;
  is_moving = false;
  auto modes = full_split(query.get_arg("mode"), ' ');
  for (auto &mode : modes) {
    if (type != Type::Pattern && to_lower(mode) == "blur") {
      is_blurred = true;
    }
    if (to_lower(mode) == "motion") {
      is_moving = true;
    }
  }

  if (type == Type::Pattern) {
    intensity = -101;
    auto intensity_arg = query.get_arg("intensity");
    if (!intensity_arg.empty()) {
      intensity = to_integer<int32>(intensity_arg);
    }
    if (!is_valid_intensity(intensity)) {
      intensity = 50;
    }

    auto bg_color = query.get_arg("bg_color");
    if (!bg_color.empty()) {
      auto r_fill = BackgroundFill::get_background_fill(
          PSLICE() << url_encode(bg_color) << "?rotation=" << url_encode(query.get_arg("rotation")));
      if (r_fill.is_ok()) {
        fill = r_fill.move_as_ok();
      }
    }
  }
}

string BackgroundType::get_link() const {
  string mode;
  if (is_blurred) {
    mode = "blur";
  }
  if (is_moving) {
    if (!mode.empty()) {
      mode += '+';
    }
    mode += "motion";
  }

  switch (type) {
    case BackgroundType::Type::Wallpaper: {
      if (!mode.empty()) {
        return PSTRING() << "mode=" << mode;
      }
      return string();
    }
    case BackgroundType::Type::Pattern: {
      string link = PSTRING() << "intensity=" << intensity
                              << "&bg_color=" << get_background_fill_color_hex_string(fill, false);
      if (!mode.empty()) {
        link += "&mode=";
        link += mode;
      }
      return link;
    }
    case BackgroundType::Type::Fill:
      return get_background_fill_color_hex_string(fill, true);
    default:
      UNREACHABLE();
      return string();
  }
}

bool operator==(const BackgroundType &lhs, const BackgroundType &rhs) {
  return lhs.type == rhs.type && lhs.is_blurred == rhs.is_blurred && lhs.is_moving == rhs.is_moving &&
         lhs.intensity == rhs.intensity && lhs.fill == rhs.fill;
}

static StringBuilder &operator<<(StringBuilder &string_builder, const BackgroundType::Type &type) {
  switch (type) {
    case BackgroundType::Type::Wallpaper:
      return string_builder << "Wallpaper";
    case BackgroundType::Type::Pattern:
      return string_builder << "Pattern";
    case BackgroundType::Type::Fill:
      return string_builder << "Fill";
    default:
      UNREACHABLE();
      return string_builder;
  }
}

StringBuilder &operator<<(StringBuilder &string_builder, const BackgroundType &type) {
  return string_builder << "type " << type.type << '[' << type.get_link() << ']';
}

Result<BackgroundType> get_background_type(const td_api::BackgroundType *type) {
  if (type == nullptr) {
    return Status::Error(400, "Type must be non-empty");
  }

  BackgroundType result;
  switch (type->get_id()) {
    case td_api::backgroundTypeWallpaper::ID: {
      auto wallpaper = static_cast<const td_api::backgroundTypeWallpaper *>(type);
      result = BackgroundType(wallpaper->is_blurred_, wallpaper->is_moving_);
      break;
    }
    case td_api::backgroundTypePattern::ID: {
      auto pattern = static_cast<const td_api::backgroundTypePattern *>(type);
      TRY_RESULT(background_fill, get_background_fill(pattern->fill_.get()));
      if (!is_valid_intensity(pattern->intensity_)) {
        return Status::Error(400, "Wrong intensity value");
      }
      result = BackgroundType(pattern->is_moving_, std::move(background_fill), pattern->intensity_);
      break;
    }
    case td_api::backgroundTypeFill::ID: {
      auto fill = static_cast<const td_api::backgroundTypeFill *>(type);
      TRY_RESULT(background_fill, get_background_fill(fill->fill_.get()));
      result = BackgroundType(std::move(background_fill));
      break;
    }
    default:
      UNREACHABLE();
  }
  return result;
}

BackgroundType get_background_type(bool is_pattern,
                                   telegram_api::object_ptr<telegram_api::wallPaperSettings> settings) {
  bool is_blurred = false;
  bool is_moving = false;
  BackgroundFill fill(settings.get());
  int32 intensity = 0;
  if (settings) {
    auto flags = settings->flags_;
    is_blurred = (flags & telegram_api::wallPaperSettings::BLUR_MASK) != 0;
    is_moving = (flags & telegram_api::wallPaperSettings::MOTION_MASK) != 0;

    if ((flags & telegram_api::wallPaperSettings::INTENSITY_MASK) != 0) {
      intensity = settings->intensity_;
      if (!is_valid_intensity(intensity)) {
        LOG(ERROR) << "Receive " << to_string(settings);
        intensity = 50;
      }
    }
  }
  if (is_pattern) {
    return BackgroundType(is_moving, fill, intensity);
  } else {
    return BackgroundType(is_blurred, is_moving);
  }
}

static td_api::object_ptr<td_api::BackgroundFill> get_background_fill_object(const BackgroundFill &fill) {
  switch (fill.get_type()) {
    case BackgroundFill::Type::Solid:
      return td_api::make_object<td_api::backgroundFillSolid>(fill.top_color);
    case BackgroundFill::Type::Gradient:
      return td_api::make_object<td_api::backgroundFillGradient>(fill.top_color, fill.bottom_color,
                                                                 fill.rotation_angle);
    case BackgroundFill::Type::FreeformGradient: {
      vector<int32> colors{fill.top_color, fill.bottom_color, fill.third_color, fill.fourth_color};
      if (colors.back() == -1) {
        colors.pop_back();
      }
      return td_api::make_object<td_api::backgroundFillFreeformGradient>(std::move(colors));
    }
    default:
      UNREACHABLE();
      return nullptr;
  }
}

td_api::object_ptr<td_api::BackgroundType> get_background_type_object(const BackgroundType &type) {
  switch (type.type) {
    case BackgroundType::Type::Wallpaper:
      return td_api::make_object<td_api::backgroundTypeWallpaper>(type.is_blurred, type.is_moving);
    case BackgroundType::Type::Pattern:
      return td_api::make_object<td_api::backgroundTypePattern>(get_background_fill_object(type.fill), type.intensity,
                                                                type.is_moving);
    case BackgroundType::Type::Fill:
      return td_api::make_object<td_api::backgroundTypeFill>(get_background_fill_object(type.fill));
    default:
      UNREACHABLE();
      return nullptr;
  }
}

telegram_api::object_ptr<telegram_api::wallPaperSettings> get_input_wallpaper_settings(const BackgroundType &type) {
  CHECK(type.is_server());

  int32 flags = 0;
  if (type.is_blurred) {
    flags |= telegram_api::wallPaperSettings::BLUR_MASK;
  }
  if (type.is_moving) {
    flags |= telegram_api::wallPaperSettings::MOTION_MASK;
  }
  switch (type.fill.get_type()) {
    case BackgroundFill::Type::FreeformGradient:
      if (type.fill.fourth_color != -1) {
        flags |= telegram_api::wallPaperSettings::FOURTH_BACKGROUND_COLOR_MASK;
      }
      flags |= telegram_api::wallPaperSettings::THIRD_BACKGROUND_COLOR_MASK;
      // fallthrough
    case BackgroundFill::Type::Gradient:
      flags |= telegram_api::wallPaperSettings::SECOND_BACKGROUND_COLOR_MASK;
      // fallthrough
    case BackgroundFill::Type::Solid:
      flags |= telegram_api::wallPaperSettings::BACKGROUND_COLOR_MASK;
      break;
    default:
      UNREACHABLE();
  }
  if (type.intensity != 0) {
    flags |= telegram_api::wallPaperSettings::INTENSITY_MASK;
  }
  return telegram_api::make_object<telegram_api::wallPaperSettings>(
      flags, false /*ignored*/, false /*ignored*/, type.fill.top_color, type.fill.bottom_color, type.fill.third_color,
      type.fill.fourth_color, type.intensity, type.fill.rotation_angle);
}

}  // namespace td
