#pragma once

#include <boost/sml.hpp>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

#include <turing/tape.hpp>

namespace sml = boost::sml;

namespace turing {

// ---------------------------------------------------------------------------
// MachineError
// ---------------------------------------------------------------------------

struct MachineError {
    enum class Kind {
        UnknownInstruction,
        UnmatchedBracket,
        TapeFailure,
    };

    Kind                     kind;
    std::string              message;
    std::optional<TapeError> tape_error;
};

// ---------------------------------------------------------------------------
// Instruction set
// ---------------------------------------------------------------------------

namespace instr {
    constexpr char Right = '>';
    constexpr char Left  = '<';
    constexpr char Write = '!';
    constexpr char Read  = '%';
    constexpr char Loop  = '[';
    constexpr char Back  = ']';
} // namespace instr

// ---------------------------------------------------------------------------
// MachineContext — holds all mutable state the interpreter operates on
// ---------------------------------------------------------------------------

template <TapeLike Tape = DequeTape>
struct MachineContext {
    std::string program;
    Tape        tape;
    std::size_t ip       = 0;     // instruction pointer
    char        reg      = '\0';  // register
    int         depth    = 0;     // bracket depth counter for seek
    bool        seeking  = false; // set by ] when cell != '\0'

    /// The last error encountered, if any
    std::optional<MachineError> error;

    /// Check if IP is past the end of the program
    [[nodiscard]] auto at_end() const -> bool {
        return ip >= program.size();
    }

    /// Current instruction (must not be at_end)
    [[nodiscard]] auto current_instr() const -> char {
        return program[ip];
    }
};

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

namespace events {
    struct Tick {};
} // namespace events

// ---------------------------------------------------------------------------
// InterpreterSM — SML transition table with guards and actions
//
// Template on Tape so guards/actions use concrete MachineContext<Tape>&
// for SML dependency injection via function_traits.
// ---------------------------------------------------------------------------

template <TapeLike Tape = DequeTape>
struct InterpreterSM {
    using ctx_t = MachineContext<Tape>;

    constexpr static auto Idle = sml::state<class Idle>;
    constexpr static auto Run  = sml::state<class Run>;

    auto operator()() const {
        using namespace sml;

        // -- Guards --

        const auto is_seeking = [](ctx_t& ctx) -> bool {
            return ctx.seeking;
        };

        const auto should_execute = [](ctx_t& ctx) -> bool {
            return !ctx.seeking && !ctx.at_end();
        };

        const auto should_halt = [](ctx_t& ctx) -> bool {
            return !ctx.seeking && ctx.at_end();
        };

        // -- Actions --

        const auto do_execute = [](ctx_t& ctx) {
            using Action = std::function<void(ctx_t&)>;
            static const std::unordered_map<char, Action> dispatch {
                { instr::Right, [](ctx_t& ctx) {
                    auto r = ctx.tape.move_right();
                    if (!r) {
                        ctx.error = MachineError{
                            .kind       = MachineError::Kind::TapeFailure,
                            .message    = "move_right failed",
                            .tape_error = r.error(),
                        };
                    }
                }},
                { instr::Left, [](ctx_t& ctx) {
                    auto r = ctx.tape.move_left();
                    if (!r) {
                        ctx.error = MachineError{
                            .kind       = MachineError::Kind::TapeFailure,
                            .message    = "move_left failed",
                            .tape_error = r.error(),
                        };
                    }
                }},
                { instr::Write, [](ctx_t& ctx) {
                    auto r = ctx.tape.write(ctx.reg);
                    if (!r) {
                        ctx.error = MachineError{
                            .kind       = MachineError::Kind::TapeFailure,
                            .message    = "write failed",
                            .tape_error = r.error(),
                        };
                    }
                }},
                { instr::Read, [](ctx_t& ctx) {
                    auto r = ctx.tape.read();
                    if (!r) {
                        ctx.error = MachineError{
                            .kind       = MachineError::Kind::TapeFailure,
                            .message    = "read failed",
                            .tape_error = r.error(),
                        };
                    } else {
                        ctx.reg = *r;
                    }
                }},
                { instr::Loop, [](ctx_t&) {
                    // [ is just an anchor — do nothing, IP advances
                }},
                { instr::Back, [](ctx_t& ctx) {
                    // ] — conditional: jump back if current cell != '\0'
                    auto r = ctx.tape.read();
                    if (!r) {
                        ctx.error = MachineError{
                            .kind       = MachineError::Kind::TapeFailure,
                            .message    = "read failed during ] check",
                            .tape_error = r.error(),
                        };
                        return;
                    }
                    if (*r != '\0') {
                        ctx.depth   = 1;
                        ctx.seeking = true;
                    }
                }},
            };

            auto it = dispatch.find(ctx.current_instr());
            if (it != dispatch.end()) {
                it->second(ctx);
            }

            if (!ctx.seeking) {
                ++ctx.ip;
            }
        };

        const auto do_seek = [](ctx_t& ctx) {
            if (ctx.ip == 0) {
                ctx.error = MachineError{
                    .kind    = MachineError::Kind::UnmatchedBracket,
                    .message = "unmatched ] at runtime: no matching [ found",
                };
                ctx.seeking = false;
                ctx.depth   = 0;
                return;
            }

            --ctx.ip;
            char c = ctx.program[ctx.ip];

            if (c == instr::Back) {
                ++ctx.depth;
            } else if (c == instr::Loop) {
                --ctx.depth;
                if (ctx.depth == 0) {
                    ctx.seeking = false;
                }
            }
        };

        // clang-format off
        return make_transition_table(
            *Idle + event<events::Tick>                                    = Run,
             Run  + event<events::Tick> [is_seeking]       / do_seek       = Run,
             Run  + event<events::Tick> [should_execute]   / do_execute    = Run,
             Run  + event<events::Tick> [should_halt]                      = X
        );
        // clang-format on
    }
};

} // namespace turing
