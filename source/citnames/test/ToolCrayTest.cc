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

#include "gtest/gtest.h"

#include "semantic/Common.h"
#include "semantic/Tool.h"
#include "semantic/ToolCray.h"

#include <cstdlib>

using namespace cs::semantic;

namespace {

    TEST(ToolCray, is_compiler_call) {
        struct Expose : public ToolCray {
            using ToolCray::is_compiler_call;
        };
        Expose sut;

        const char* originalValue = nullptr;
        if ((originalValue = std::getenv(CC_IS_CRAY_ENV_VAR.data()))) {
            unsetenv(CC_IS_CRAY_ENV_VAR.data());
        }

        EXPECT_FALSE(sut.is_compiler_call("cc"));
        EXPECT_FALSE(sut.is_compiler_call("/usr/bin/cc"));
        EXPECT_TRUE(sut.is_compiler_call("craycc"));
        EXPECT_TRUE(sut.is_compiler_call("/usr/bin/craycc"));
        EXPECT_FALSE(sut.is_compiler_call("CC"));
        EXPECT_FALSE(sut.is_compiler_call("/usr/bin/CC"));
        EXPECT_TRUE(sut.is_compiler_call("crayCC"));
        EXPECT_TRUE(sut.is_compiler_call("/usr/bin/crayCC"));
        EXPECT_TRUE(sut.is_compiler_call("arm-none-eabi-crayCC"));
        EXPECT_TRUE(sut.is_compiler_call("/usr/bin/arm-none-eabi-crayCC"));
        EXPECT_TRUE(sut.is_compiler_call("craycc-6"));
        EXPECT_TRUE(sut.is_compiler_call("/usr/bin/craycc-6"));
        EXPECT_TRUE(sut.is_compiler_call("crayftn"));
        EXPECT_TRUE(sut.is_compiler_call("ftn"));

        setenv(CC_IS_CRAY_ENV_VAR.data(), "1", /*replace=*/1);
        EXPECT_TRUE(sut.is_compiler_call("cc"));
        EXPECT_TRUE(sut.is_compiler_call("/usr/bin/cc"));
        EXPECT_TRUE(sut.is_compiler_call("CC"));
        EXPECT_TRUE(sut.is_compiler_call("/usr/bin/CC"));

        if (originalValue) {
            setenv(CC_IS_CRAY_ENV_VAR.data(), originalValue, /*replace=*/1);
        } else {
            unsetenv(CC_IS_CRAY_ENV_VAR.data());
        }
    }

    TEST(ToolCray, fails_on_empty) {
        Execution input = {};

        ToolCray sut;

        EXPECT_TRUE(Tool::not_recognized(sut.recognize(input)));
    }

    TEST(ToolCray, simple) {
        Execution input = {
            "/usr/bin/craycc",
            {"craycc", "-c", "-o", "source.o", "source.c"},
            "/home/user/project",
            {},
            };
        SemanticPtr expected = SemanticPtr(
            new Compile(
                input.working_dir,
                input.executable,
                {"-c"},
                {fs::path("source.c")},
                {fs::path("source.o")})
            );

        ToolCray sut({});

        auto result = sut.recognize(input);
        EXPECT_TRUE(Tool::recognized_ok(result));
        EXPECT_PRED2([](auto lhs, auto rhs) { return lhs->operator==(*rhs); }, expected, result.unwrap());
    }

    TEST(ToolCray, linker_flag_filtered) {
        Execution input = {
            "/usr/bin/craycc",
            {"craycc", "-L.", "-lthing", "-o", "exe", "source.c"},
            "/home/user/project",
            {},
            };
        SemanticPtr expected = SemanticPtr(
            new Compile(
                input.working_dir,
                input.executable,
                {"-c"},
                {fs::path("source.c")},
                {fs::path("exe")}
                )
            );

        ToolCray sut({});

        auto result = sut.recognize(input);
        EXPECT_TRUE(Tool::recognized_ok(result));
        EXPECT_PRED2([](auto lhs, auto rhs) { return lhs->operator==(*rhs); }, expected, result.unwrap());
    }

    TEST(ToolCray, pass_on_help) {
        Execution input = {
            "/usr/bin/craycc",
            {"craycc", "--version"},
            "/home/user/project",
            {},
            };
        SemanticPtr expected = SemanticPtr(new QueryCompiler());

        ToolCray sut({});

        auto result = sut.recognize(input);
        EXPECT_TRUE(result.is_ok());
        EXPECT_PRED2([](auto lhs, auto rhs) { return lhs->operator==(*rhs); }, expected, result.unwrap());
    }
}
