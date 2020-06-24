#pragma once

struct Location
{
  int x = 0, y = 0;
  Location() noexcept = default;
  Location(int x, int y) noexcept : x(x), y(y){};
};

struct Color
{
  Uint8 r = 0, g = 0, b = 0, a = std::numeric_limits<Uint8>::max();
  Color() noexcept = default;
  explicit Color(const Uint8 v) noexcept
      : r(v), g(v), b(v), a(std::numeric_limits<Uint8>::max())
  {
  }
  Color(const Uint8 r, const Uint8 g, const Uint8 b) noexcept
      : r(r), g(g), b(b), a(std::numeric_limits<Uint8>::max())
  {
  }
};

struct Piece
{
  explicit Piece(std::initializer_list<Location> &&t) noexcept;
  [[nodiscard]] auto get_world_parts() const;
  // Sweep to apply a position offset and stop if collision occurs.
  auto add_offset(Location offset) -> bool;
  [[nodiscard]] auto has_collision() const -> bool;

  std::shared_ptr<std::vector<Location>> parts;
  Location location{0, 0};
};

class Actor
{
public:
  void tick(float delta_time);
};

class Board : Actor
{
public:
  Board();
  auto try_eliminate_rows() -> int;
  void tick(float delta_time);
  void stamp_values(const Piece &piece, const bool new_value);
  [[nodiscard]] static auto location_to_index(const Location &loc)
      -> std::size_t;
  [[nodiscard]] auto get_value_at(const Location &world_location) const
      -> std::optional<bool>;
  auto set_value_at(const Location &world_location, const bool new_value)
      -> bool;

  constexpr static const int width = 10;
  constexpr static const int height = 21;
  constexpr static const int grid_size = 22;
  constexpr static const int padding = 2;
  constexpr static const int margin = 4;
  std::vector<bool> data;
};

class GameMode : Actor
{
public:
  void choose_new_piece();
  auto can_end_match() const -> bool;
  void tick_new_piece(float delta_time);
  void tick_current_piece(float delta_time);
  void tick(float delta_time);

  constexpr const static float fall_delay = 1.F;
  constexpr const static float new_piece_delay = 1.5F;
  float current_fall_time = 0.F;
  float current_new_piece_time = 0.F;
  std::optional<Piece> player_piece;
  bool is_match_over = false;
};