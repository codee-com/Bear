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

#pragma once

#include "Semantic.h"
#include "Parsers.h"

#include "libresult/Result.h"

namespace cs::semantic {
    // Both 'cc' and 'CC' are ambiguous compiler binaries since on system's with
    // using the Cray compiler suite, they do not point to GCC.
    // To deal with that edge case, one can set this environment variable to
    // avoid recognizing cc and CC as gcc and g++ respectively.
    static constexpr std::string_view CC_IS_CRAY_ENV_VAR = "BEAR_CC_IS_CRAY";

    rust::Result<SemanticPtr> compilation_impl(const FlagsByName& flags, const Execution& execution,
        std::function<Arguments(const Execution&)> create_argument_list_func,
        std::function<bool(const CompilerFlags&)> is_preprocessor_func);
}
