#include <format>
#include <iostream>
#include <string_view>

#include <turing/machine.hpp>

using turing::TuringMachine;

// ---------------------------------------------------------------------------
// Helper: create, run, and dump a program — or print validation error
// ---------------------------------------------------------------------------

static void demo(std::string_view title,
                 std::string      program,
                 std::string_view tape_init = "")
{
    std::cout << std::format("=== {} ===\n", title);
    std::cout << std::format("  Program:    \"{}\"\n", program);
    std::cout << std::format("  Tape init:  \"{}\"\n\n", tape_init);

    auto result = TuringMachine<>::create(std::move(program), tape_init);
    if (!result) {
        std::cout << std::format("  VALIDATION ERROR: {}\n\n", result.error().message);
        return;
    }

    auto& m = *result;
    m.run();

    m.dump(std::cout);
    std::cout << '\n';
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    // 1. Copy cell 0 to cell 1: %>!
    //    Initial tape: ['A'], head at 0
    //    Expected:     ['A', 'A'], head at 1
    demo("Copy cell 0 to cell 1", "%>!", "A");

    // 2. Zero cell 0 using a blank cell: >>%<<!
    //    Initial tape: ['X', 'Y'], head at 0
    //    Expected:     ['\0', 'Y', '\0'], head at 0
    demo("Zero cell 0 using blank cell", ">>%<<!", "XY");

    // 3. Terminating loop: clear cell then try loop
    //    Program: >>%<<![>]
    //    Initial tape: ['X', 'Y'], head at 0
    //    The >>%<<! zeros cell 0, then [ enters loop, > moves right,
    //    ] checks cell 1='Y' != '\0' -> seeks back to [, > moves right
    //    again to cell 2 (blank), ] checks '\0' -> falls through.
    demo("Zero + loop that terminates", ">>%<<![>]", "XY");

    // 4. Empty program — halts immediately
    demo("Empty program (immediate halt)", "");

    // 5. Validation error: unknown instruction
    demo("Unknown instruction error", "%>!x<");

    // 6. Validation error: unmatched ]
    demo("Unmatched ] error", "%>]!");

    // 7. Validation error: unmatched [
    demo("Unmatched [ error", "[%>!");

    return 0;
}
