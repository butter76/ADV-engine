cmake_minimum_required(VERSION 3.24)
project(adv_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# The engine is headers. -ffp-contract=off: where Showdown multiplies by a
# fraction the engine does the same arithmetic in doubles, and a fused
# multiply-add would round differently. Whatever includes the engine must be
# built with it, so it goes with the target.
add_library(adv INTERFACE)
target_include_directories(adv INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(adv INTERFACE -ffp-contract=off)

# A project that takes the engine in (add_subdirectory, FetchContent) wants
# the target and none of what follows.
if(NOT PROJECT_IS_TOP_LEVEL)
  return()
endif()

# The engine's own warnings, for its tests and tools.
add_library(adv_strict INTERFACE)
target_link_libraries(adv_strict INTERFACE adv)
target_compile_options(adv_strict INTERFACE -Wall -Wextra -Wpedantic -Werror)

enable_testing()

# The PRNG on a fixed script of calls; the line is what tools/prng_ref.cjs
# prints from Showdown's own PRNG.
add_executable(prng_test tests/prng_test.cpp)
target_link_libraries(prng_test PRIVATE adv_strict)
add_test(NAME prng COMMAND prng_test)
set_tests_properties(prng PROPERTIES PASS_REGULAR_EXPRESSION "2000000 calls, hash 4dc30e29d5dc4fbe")

# The generated tables against the facts tools/tables_ref.cjs read from Showdown.
add_executable(data_test tests/data_test.cpp)
target_link_libraries(data_test PRIVATE adv_strict)
add_test(NAME data COMMAND data_test ${CMAKE_SOURCE_DIR}/tests/fixtures/tables.txt)

# Teams in Showdown's export text against what Showdown packs of the same
# text (tools/teams_ref.cjs).
add_executable(export_test tests/export_test.cpp)
target_link_libraries(export_test PRIVATE adv_strict)
add_test(NAME export COMMAND export_test ${CMAKE_SOURCE_DIR}/tests/teams/ou/sample-teams.txt
         ${CMAKE_SOURCE_DIR}/tests/fixtures/sample-teams.packed)

# Every draw of recorded Showdown games, replayed from the seed.
add_executable(trace_draws tests/trace_draws.cpp)
target_link_libraries(trace_draws PRIVATE adv_strict)
add_test(NAME trace_draws COMMAND trace_draws ${CMAKE_SOURCE_DIR}/tests/fixtures/ou-50.brief.trace)

# The engine on recorded games, line by line: every draw, choice, log line
# and request. A game that reaches something not yet ported stops there,
# after everything before it was compared.
add_executable(replay tests/replay.cpp)
target_link_libraries(replay PRIVATE adv_strict)
# 200 games up to the first choice.
add_test(NAME replay_openings COMMAND replay ${CMAKE_SOURCE_DIR}/tests/fixtures/ou-200.opening.trace --complete)
# 30 games of 30 turns in which both players switch whenever they can.
add_test(NAME replay_switches COMMAND replay ${CMAKE_SOURCE_DIR}/tests/fixtures/switch-30.trace --complete)
# 20 games of 40 turns in which both players use only plain damaging moves, or switch.
add_test(NAME replay_plain_moves COMMAND replay ${CMAKE_SOURCE_DIR}/tests/fixtures/plain-20.trace --complete)
# 60 games of 12 turns with any choice at all; most stop at a move not ported yet.
add_test(NAME replay_any COMMAND replay ${CMAKE_SOURCE_DIR}/tests/fixtures/any-60.trace --complete)
# Whole games that once differed: a fainted Pokémon's status, and a faint
# resolved after an effect that only counts down.
add_test(NAME replay_regressions COMMAND replay ${CMAKE_SOURCE_DIR}/tests/fixtures/regressions.trace --complete)
# Games between teams written to use particular moves (tests/teams/targeted),
# and between teams of anything Gen 3 has, legal or not.
add_test(NAME replay_targeted COMMAND replay ${CMAKE_SOURCE_DIR}/tests/fixtures/targeted-16.trace --complete)
add_test(NAME replay_random COMMAND replay ${CMAKE_SOURCE_DIR}/tests/fixtures/random-24.trace --complete)

# The same checker built so that every event verifies the masks it is about
# to trust (ADV_CHECK_MASKS): a mask kept on a Pokémon or on the battle that
# has fallen behind what it covers stops the game.
add_executable(replay_masks tests/replay.cpp)
target_link_libraries(replay_masks PRIVATE adv_strict)
target_compile_definitions(replay_masks PRIVATE ADV_CHECK_MASKS)
add_test(NAME masks_targeted COMMAND replay_masks ${CMAKE_SOURCE_DIR}/tests/fixtures/targeted-16.trace --complete)
add_test(NAME masks_random COMMAND replay_masks ${CMAKE_SOURCE_DIR}/tests/fixtures/random-24.trace --complete)
add_test(NAME masks_regressions COMMAND replay_masks ${CMAKE_SOURCE_DIR}/tests/fixtures/regressions.trace --complete)

# Not a test: what dispatch costs on the measured mix of events.
add_executable(dispatch_bench smoke/dispatch_bench.cpp)
target_link_libraries(dispatch_bench PRIVATE adv_strict)

# Not a test: how fast the engine plays the choices of a recorded trace.
add_executable(engine_bench smoke/engine_bench.cpp)
target_link_libraries(engine_bench PRIVATE adv_strict)
