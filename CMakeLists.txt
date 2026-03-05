cmake_minimum_required (VERSION 3.8)

project("GigaLearnBot")

# Main Bot
file(GLOB FILES_SRC "src/*.cpp" "src/*.h" "src/*.hpp")
list(REMOVE_ITEM FILES_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/test_logic.cpp")
add_executable(GigaLearnBot ${FILES_SRC})

# Test Executable
add_executable(GigaLearnTest src/test_logic.cpp src/rewards.h src/GoalieState.h src/ShadowDefenseState.h)

# Set C++ version to 20 for both
set_target_properties(GigaLearnBot GigaLearnTest PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(GigaLearnBot GigaLearnTest PROPERTIES CXX_STANDARD 20)

# Make sure GigaLearnCPP is going to build in the same directory as us
# Otherwise, we won't be able to import it at runtime
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}")

# Include RLGymSim_PPO
add_subdirectory(GigaLearnCPP)
target_link_libraries(GigaLearnBot GigaLearnCPP)
target_link_libraries(GigaLearnTest GigaLearnCPP)

# Include RLBot
add_subdirectory(RLBotCPP)
target_link_libraries(GigaLearnBot RLBotCPP)

# --- Copy Collision Meshes ---
add_custom_command(TARGET GigaLearnBot POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/../collision_meshes"
    "$<TARGET_FILE_DIR:GigaLearnBot>/collision_meshes"
    COMMENT "Copying collision meshes..."
)