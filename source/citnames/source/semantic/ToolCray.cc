/*  Copyright (C) 2012-2024 by László Nagy
    This file is part of Bear.

 Bear is a tool to generate compilation database for clang tooling.

 Bear is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Bear is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ToolCray.h"
#include "Common.h"
#include "Parsers.h"

#include <cstdlib>
#include <regex>
#include <string_view>

using namespace cs::semantic;

namespace {
    const FlagsByName CRAY_FLAG_DEFINITION = {
        { "-fcray", { MatchInstruction::EXACTLY, CompilerFlagType::OTHER } },
        { "-fno-cray", { MatchInstruction::EXACTLY, CompilerFlagType::OTHER } },
        { "-fenhanced-asm", { MatchInstruction::EXACTLY_WITH_1_OPT_GLUED_WITH_EQ, CompilerFlagType::OTHER } },
        { "-fenhanced-ir", { MatchInstruction::EXACTLY_WITH_1_OPT_GLUED_WITH_EQ, CompilerFlagType::OTHER } },
        { "-ffp", { MatchInstruction::EXACTLY_WITH_1_OPT_GLUED_WITH_EQ, CompilerFlagType::OTHER } },
        { "-fcray-mallopt", { MatchInstruction::EXACTLY, CompilerFlagType::OTHER } },
        { "-fno-cray-mallopt", { MatchInstruction::EXACTLY, CompilerFlagType::OTHER } },
        { "-fivdep", { MatchInstruction::EXACTLY, CompilerFlagType::OTHER } },
        { "-fno-ivdep", { MatchInstruction::EXACTLY, CompilerFlagType::OTHER } },
        { "-flocal-restrict", { MatchInstruction::EXACTLY, CompilerFlagType::OTHER } },
        { "-fno-local-restrict", { MatchInstruction::EXACTLY, CompilerFlagType::OTHER } },
        { "-floop-trips", { MatchInstruction::EXACTLY_WITH_1_OPT_GLUED_WITH_EQ, CompilerFlagType::OTHER } },
        { "-fsave-decompile", { MatchInstruction::EXACTLY, CompilerFlagType::OTHER } },
        { "-fsave-loopmark", { MatchInstruction::EXACTLY, CompilerFlagType::OTHER } },
        { "-floopmark-style", { MatchInstruction::EXACTLY_WITH_1_OPT_GLUED_WITH_EQ, CompilerFlagType::OTHER } },
        { "-finstrument-loops", { MatchInstruction::EXACTLY, CompilerFlagType::OTHER } },
        { "-finstrument-openmp", { MatchInstruction::EXACTLY, CompilerFlagType::OTHER } },
        { "-fno-instrument-openmp", { MatchInstruction::EXACTLY, CompilerFlagType::OTHER } },
        { "-fcray-program-library-path", { MatchInstruction::EXACTLY_WITH_1_OPT_GLUED_WITH_EQ_OR_SEP, CompilerFlagType::OTHER } },
        { "-fcray-trapping-math", { MatchInstruction::EXACTLY, CompilerFlagType::OTHER } },
        { "-fno-cray-trapping-math", { MatchInstruction::EXACTLY, CompilerFlagType::OTHER } },
        { "-funinitialized-heap-ints", { MatchInstruction::EXACTLY_WITH_1_OPT_GLUED_WITH_EQ, CompilerFlagType::OTHER } },
        { "-funinitialized-heap-floats", { MatchInstruction::EXACTLY_WITH_1_OPT_GLUED_WITH_EQ, CompilerFlagType::OTHER } },
        { "-funinitialized-stack-ints", { MatchInstruction::EXACTLY_WITH_1_OPT_GLUED_WITH_EQ, CompilerFlagType::OTHER } },
        { "-funinitialized-stack-floats", { MatchInstruction::EXACTLY_WITH_1_OPT_GLUED_WITH_EQ, CompilerFlagType::OTHER } },
        { "-funinitialized-static-floats", { MatchInstruction::EXACTLY_WITH_1_OPT_GLUED_WITH_EQ, CompilerFlagType::OTHER } },
        { "-hupc", { MatchInstruction::EXACTLY, CompilerFlagType::OTHER } },
        { "-hdefault", { MatchInstruction::EXACTLY, CompilerFlagType::OTHER } },
        { "-fupc-auto-amo", { MatchInstruction::EXACTLY, CompilerFlagType::OTHER } },
        { "-fno-upc-auto-amo", { MatchInstruction::EXACTLY, CompilerFlagType::OTHER } },
        { "-fupc-buffered-async", { MatchInstruction::EXACTLY, CompilerFlagType::OTHER } },
        { "-fno-upc-buffered-async", { MatchInstruction::EXACTLY, CompilerFlagType::OTHER } },
        { "-fupc-pattern", { MatchInstruction::EXACTLY, CompilerFlagType::OTHER } },
        { "-fno-upc-pattern", { MatchInstruction::EXACTLY, CompilerFlagType::OTHER } },
        { "-fupc-threads", { MatchInstruction::EXACTLY_WITH_1_OPT_GLUED_WITH_EQ, CompilerFlagType::OTHER } },
        { "-ffortran-byte-swap-io", { MatchInstruction::EXACTLY, CompilerFlagType::OTHER } },
    };

    FlagsByName cray_flags(const FlagsByName& base)
    {
        FlagsByName flags(base);
        flags.insert(CRAY_FLAG_DEFINITION.begin(), CRAY_FLAG_DEFINITION.end());
        return flags;
    }
}

namespace cs::semantic {

    ToolCray::ToolCray() noexcept
            : cray_flag_definition(cray_flags(flag_definition))
    {
    }

    rust::Result<SemanticPtr> ToolCray::recognize(const Execution& execution) const
    {
        if (is_compiler_call(execution.executable)) {
            return ToolGcc::compilation(cray_flag_definition, execution);
        }
        return rust::Ok(SemanticPtr());
    }

    bool ToolCray::is_compiler_call(const fs::path& program) const
    {
        static const auto patternCCIsCray = std::regex(
            R"(^([^-]*-)*(cray)?(cc|CC|ftn)(-?\d+(\.\d+){0,2})?$)");
        static const auto patternCCIsNotCray = std::regex(
            R"(^([^-]*-)*(craycc|crayCC|(cray)?ftn)(-?\d+(\.\d+){0,2})?$)");

        std::cmatch m;
        return std::regex_match(program.filename().c_str(), m, std::getenv(CC_IS_CRAY_ENV_VAR.data()) ? patternCCIsCray : patternCCIsNotCray);
    }
}
