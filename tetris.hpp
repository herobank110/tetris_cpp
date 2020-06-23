#pragma once

struct Location
{
  int x = 0, y = 0;
  Location() noexcept = default;
  Location(int x, int y) noexcept
    : x(x)
    , y(y){};
};

struct Color
{
  Uint8 r = 0, g = 0, b = 0, a = std::numeric_limits<Uint8>::max();
  Color() noexcept = default;
  explicit Color(const Uint8 v) noexcept
    : r(v)
    , g(v)
    , b(v)
    , a(std::numeric_limits<Uint8>::max())
  {}
  Color(const Uint8 r, const Uint8 g, const Uint8 b) noexcept
    : r(r)
    , g(g)
    , b(b)
    , a(std::numeric_limits<Uint8>::max())
  {}
};

struct Piece
{
  explicit Piece(std::initializer_list<Location>&& t) noexcept
    : parts{ std::make_shared<std::vector<Location>>(std::move(t)) }
  {}

  [[nodiscard]] auto get_world_parts() const
  {
    std::vector<Location> vec{};
    vec.reserve(parts->size());
    std::transform(parts->begin(),
                   parts->end(),
                   std::back_inserter(vec),
                   [this](const Location& p) {
                     return Location{ p.x + location.x, p.y + location.y };
                   });
    return vec;
  }

  // Sweep to apply a position offset and stop if collision occurs.
  auto add_offset(Location offset) -> bool
  {
    const Location sweep{ offset.x < 0 ? -1 : offset.x > 0 ? 1 : 0,
                          offset.y < 0 ? -1 : offset.y > 0 ? 1 : 0 };

    while (!(offset.x == 0 && offset.y == 0)) {
      if (sweep.x != 0) {
        location.x += sweep.x;
        if (has_collision()) {
          location.x -= sweep.x;
          return false;
        }
        offset.x -= sweep.x;
      }

      if (sweep.y != 0) {
        location.y += sweep.y;
        if (has_collision()) {
          location.y -= sweep.y;
          return false;
        }
        offset.y -= sweep.y;
      }
    }
    return true;
  };

  [[nodiscard]] auto has_collision() const -> bool;

  std::shared_ptr<std::vector<Location>> parts;
  Location location{ 0, 0 };
};
