// SPDX-License-Identifier: MIT
// Tiny test helpers with readable, coloured output (colours only on a terminal).
#ifndef TEST_UI_H
#define TEST_UI_H

#include <cstdio>
#include <unistd.h>

static int g_checks, g_failed, g_passed, g_section_failed;

static bool tty() { return isatty(1) != 0; }
static const char *tc_(const char *code) { return tty() ? code : ""; }

#define CHECK(cond)                                                                        \
    do {                                                                                   \
        g_checks++;                                                                        \
        if (!(cond)) {                                                                     \
            g_failed++;                                                                    \
            g_section_failed++;                                                            \
            std::printf("   %s✗ %s:%d: %s%s\n", tc_("\033[31m"), __FILE__, __LINE__, #cond,   \
                        tc_("\033[0m"));                                                     \
        }                                                                                  \
    } while (0)

static void banner(const char *title, const char *version)
{
    std::printf("\n%s┌─ %s  v%s%s\n", tc_("\033[1;35m"), title, version, tc_("\033[0m"));
}

static void SECTION(const char *name)
{
    std::printf("%s│%s\n%s├─ %s%s\n", tc_("\033[35m"), tc_("\033[0m"), tc_("\033[1;36m"), name, tc_("\033[0m"));
}

static void PASS(const char *what)
{
    if (g_section_failed == 0) {
        g_passed++;
        std::printf("%s│%s  %s✓%s %s\n", tc_("\033[35m"), tc_("\033[0m"), tc_("\033[32m"), tc_("\033[0m"), what);
    } else {
        std::printf("%s│%s  %s✗ %s%s\n", tc_("\033[35m"), tc_("\033[0m"), tc_("\033[1;31m"), what, tc_("\033[0m"));
    }
    g_section_failed = 0;
}

static int summary()
{
    std::printf("%s│%s\n", tc_("\033[35m"), tc_("\033[0m"));
    if (g_failed == 0)
        std::printf("%s└─ all good: %d cases, %d checks%s\n\n", tc_("\033[1;32m"), g_passed, g_checks,
                    tc_("\033[0m"));
    else
        std::printf("%s└─ FAILED: %d of %d checks%s\n\n", tc_("\033[1;31m"), g_failed, g_checks, tc_("\033[0m"));
    return g_failed ? 1 : 0;
}

#endif // TEST_UI_H
