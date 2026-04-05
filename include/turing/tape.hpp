#pragma once

#include <concepts>
#include <deque>
#include <expected>
#include <format>
#include <ostream>
#include <ranges>
#include <string>
#include <string_view>

namespace rng = std::views;

namespace turing {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

/// Unicode "Symbol For Null" (U+2400) — displayed for blank cells
constexpr std::string_view kNullSymbol = "\u2400";

// ---------------------------------------------------------------------------
// TapeError
// ---------------------------------------------------------------------------

struct TapeError {
    enum class Kind {
        OutOfBounds,
        AllocationFail,
        InvalidWrite,
    };

    Kind        kind;
    std::string message;
};

// ---------------------------------------------------------------------------
// TapeLike concept
// ---------------------------------------------------------------------------

template <typename T>
concept TapeLike = requires(T t, const T ct, char c, std::ostream& os) {
    { ct.read() }      -> std::same_as<std::expected<char, TapeError>>;
    { t.write(c) }     -> std::same_as<std::expected<void, TapeError>>;
    { t.move_right() } -> std::same_as<std::expected<void, TapeError>>;
    { t.move_left() }  -> std::same_as<std::expected<void, TapeError>>;
    { ct.dump(os) }    -> std::same_as<void>;
};

// ---------------------------------------------------------------------------
// DequeTape — default TapeLike implementation
// ---------------------------------------------------------------------------

class DequeTape {
public:
    explicit DequeTape(std::string_view initial = "")
        : cells_(initial.begin(), initial.end())
        , head_{0}
    {
        // Ensure at least one cell exists (the blank tape)
        if (cells_.empty()) {
            cells_.push_back('\0');
        }
    }

    [[nodiscard]]
    auto read() const -> std::expected<char, TapeError>
    {
        return cells_[static_cast<std::size_t>(head_)];
    }

    auto write(char c) -> std::expected<void, TapeError>
    {
        cells_[static_cast<std::size_t>(head_)] = c;
        return {};
    }

    auto move_right() -> std::expected<void, TapeError>
    {
        ++head_;
        if (static_cast<std::size_t>(head_) >= cells_.size()) {
            try {
                cells_.push_back('\0');
            } catch (const std::bad_alloc&) {
                --head_;
                return std::unexpected(TapeError{
                    .kind    = TapeError::Kind::AllocationFail,
                    .message = "move_right: failed to extend tape",
                });
            }
        }
        return {};
    }

    auto move_left() -> std::expected<void, TapeError>
    {
        if (head_ == 0) {
            try {
                cells_.push_front('\0');
            } catch (const std::bad_alloc&) {
                return std::unexpected(TapeError{
                    .kind    = TapeError::Kind::AllocationFail,
                    .message = "move_left: failed to extend tape",
                });
            }
            // head_ stays at 0 — the new blank cell is now under the head
        } else {
            --head_;
        }
        return {};
    }

    void dump(std::ostream& os) const
    {
        auto cell_str = [](char c) -> std::string {
            return c == '\0' ? std::format("'{}'", kNullSymbol) : std::format("'{}'", c);
        };

        os << "[ ";
        for (char ch : cells_ | rng::transform(cell_str)
                              | rng::join_with(std::string_view{" | "})) {
            os << ch;
        }
        os << " ]\n";

        // All cells are exactly 3 display columns wide ('X' or '␀')
        // "[ " prefix = 2 cols, each cell+separator = 6 cols, center of cell = +1
        auto pos = 2 + static_cast<std::size_t>(head_) * 6 + 1;
        os << std::string(pos, ' ') << "^\n";
    }

private:
    std::deque<char> cells_;
    int              head_;
};

// Static assertion to verify DequeTape satisfies TapeLike
static_assert(TapeLike<DequeTape>, "DequeTape must satisfy TapeLike concept");

} // namespace turing
