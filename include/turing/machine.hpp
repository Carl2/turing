#pragma once

#include <algorithm>
#include <array>
#include <boost/sml.hpp>
#include <cstddef>
#include <expected>
#include <format>
#include <memory>
#include <ostream>
#include <ranges>
#include <string>
#include <string_view>

#include <turing/interpreter.hpp>
#include <turing/tape.hpp>

namespace sml = boost::sml;

namespace rng = std::views;

namespace turing {

// ---------------------------------------------------------------------------
// TuringMachine — top-level wrapper
//
// Context is heap-allocated via unique_ptr so its address is stable
// across moves (std::expected may move/copy the TuringMachine).
// The SML sm stores a reference to the context via DI, so the context
// must not be relocated after sm construction.
// ---------------------------------------------------------------------------

template <TapeLike Tape = DequeTape>
class TuringMachine {
    using ctx_t = MachineContext<Tape>;
    using sm_t  = sml::sm<InterpreterSM<Tape>>;

public:
    /// Factory — validates program before constructing the machine.
    [[nodiscard]]
    static auto create(std::string program,
                       std::string_view tape_init = "")
        -> std::expected<TuringMachine, MachineError>
    {
        if (auto r = validate_instructions(program); !r) return std::unexpected(r.error());
        if (auto r = validate_brackets(program);     !r) return std::unexpected(r.error());

        return TuringMachine(std::move(program), tape_init);
    }

    /// Fire a single Tick event through the state machine.
    void step() {
        sm_->process_event(events::Tick{});
    }

    /// Run until halted (or an error is stored in context).
    void run() {
        while (!is_halted() && !ctx_->error) {
            step();
        }
    }

    /// True if the SML machine has reached the terminal (X) state.
    [[nodiscard]]
    auto is_halted() const -> bool {
        return sm_->is(sml::X);
    }

    /// True if an error occurred during execution.
    [[nodiscard]]
    auto has_error() const -> bool {
        return ctx_->error.has_value();
    }

    /// Access the error, if any.
    [[nodiscard]]
    auto error() const -> const std::optional<MachineError>& {
        return ctx_->error;
    }

    /// Access the register value.
    [[nodiscard]]
    auto reg() const -> char {
        return ctx_->reg;
    }

    /// Access the instruction pointer.
    [[nodiscard]]
    auto ip() const -> std::size_t {
        return ctx_->ip;
    }

    /// Access the program string.
    [[nodiscard]]
    auto program() const -> const std::string& {
        return ctx_->program;
    }

    /// Dump machine state to an output stream.
    void dump(std::ostream& os) const {
        os << "Program: " << ctx_->program << '\n';
        os << "IP:      " << ctx_->ip;
        if (ctx_->at_end()) {
            os << " (past end)\n";
        } else {
            os << " -> '" << ctx_->current_instr() << "'\n";
        }
        os << "Register: ";
        if (ctx_->reg == '\0') {
            os << kNullSymbol;
        } else {
            os << "'" << ctx_->reg << "'";
        }
        os << '\n';
        os << "Halted:  " << (is_halted() ? "yes" : "no") << '\n';
        if (ctx_->error) {
            os << "Error:   " << ctx_->error->message << '\n';
        }
        os << "Tape:\n";
        ctx_->tape.dump(os);
    }

private:
    /// Validate that all characters in the program are known instructions.
    [[nodiscard]]
    static auto validate_instructions(const std::string& program)
        -> std::expected<void, MachineError>
    {
        constexpr std::array kValidInstrs {
            instr::Right, instr::Left, instr::Write,
            instr::Read,  instr::Loop, instr::Back,
        };

        for (const auto& [i, c] : program | rng::enumerate) {
            if (!std::ranges::contains(kValidInstrs, c)) {
                return std::unexpected(MachineError{
                    .kind    = MachineError::Kind::UnknownInstruction,
                    .message = std::format(
                        "unknown instruction '{}' at position {}", c, i),
                });
            }
        }
        return {};
    }

    /// Validate that all brackets are properly matched.
    [[nodiscard]]
    static auto validate_brackets(const std::string& program)
        -> std::expected<void, MachineError>
    {
        int depth = 0;
        for (const auto& [i, c] : program | rng::enumerate) {
            if (c == instr::Loop) {
                ++depth;
            } else if (c == instr::Back) {
                --depth;
                if (depth < 0) {
                    return std::unexpected(MachineError{
                        .kind    = MachineError::Kind::UnmatchedBracket,
                        .message = std::format(
                            "unmatched ']' at position {}", i),
                    });
                }
            }
        }
        if (depth != 0) {
            return std::unexpected(MachineError{
                .kind    = MachineError::Kind::UnmatchedBracket,
                .message = "unmatched '[': more '[' than ']'",
            });
        }
        return {};
    }

    /// Private constructor — only reachable through create()
    explicit TuringMachine(std::string program, std::string_view tape_init)
        : ctx_{std::make_unique<ctx_t>(ctx_t{
              .program = std::move(program),
              .tape    = Tape(tape_init),
          })}
        , sm_{std::make_unique<sm_t>(*ctx_)}
    {}

    std::unique_ptr<ctx_t> ctx_;
    std::unique_ptr<sm_t>  sm_;
};

} // namespace turing
