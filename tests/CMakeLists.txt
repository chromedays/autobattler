# Tests that need no browser. Emscripten builds them for Node, which ctest runs them with; on Windows they are plain executables.

# The auto-battler's rules and definition files (src/battle.c, defs.c): no GPU. It reads the repository's data/ directly:
# Node reads it from the disk with NODERAWFS, a native executable just opens it.
add_executable(battle_test battle_test.c ${PROJECT_SOURCE_DIR}/src/battle.c ${PROJECT_SOURCE_DIR}/src/defs.c
    ${PROJECT_SOURCE_DIR}/engine/engine/log.c ${PROJECT_SOURCE_DIR}/engine/engine/text.c)
target_include_directories(battle_test PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/engine)
target_compile_definitions(battle_test PRIVATE BATTLE_DATA_DIR="${PROJECT_SOURCE_DIR}/data")
if(NV_WEB)
    target_link_options(battle_test PRIVATE -sENVIRONMENT=node -sNODERAWFS=1)
endif()
add_test(NAME battle_test COMMAND battle_test)

# Node scripts: the Korean UI strings (the engine's tests/strings_test.mjs, run on src/: every wrapped English text has a row,
# and rows keep their printf conversions) and the build dashboard's state logic (tools/dashboard/src/state.mjs). A build with
# no Node skips them (the web build's CI runs them).
if(NV_WEB)
    set(node ${CMAKE_CROSSCOMPILING_EMULATOR})
else()
    find_program(NV_NODE node)
    set(node ${NV_NODE})
endif()
if(node)
    # strings_test.mjs takes folders relative to the engine's repository.
    add_test(NAME strings_test COMMAND ${node} ${PROJECT_SOURCE_DIR}/engine/tests/strings_test.mjs ../src)
    add_test(NAME dashboard_test COMMAND ${node} ${CMAKE_CURRENT_SOURCE_DIR}/dashboard_test.mjs)
else()
    message(WARNING "No Node: strings_test and dashboard_test are skipped (the web build's CI runs them)")
endif()
