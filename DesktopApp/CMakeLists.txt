cmake_minimum_required(VERSION 3.10)
project("DesktopApp" VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Find MPI
find_package(MPI REQUIRED)

# 2. Define XGBoost Paths
set(XGB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/xgboost")
set(XGB_INCLUDE_DIR "${XGB_ROOT}/include")
set(XGB_LIB_PATH "${XGB_ROOT}/lib/xgboost.lib")
set(XGB_DLL_PATH "${XGB_ROOT}/lib/xgboost.dll")

# 3. Create Executable
add_executable(${PROJECT_NAME} "src/main.cpp")

# 4. Include Directories
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${MPI_CXX_INCLUDE_DIRS}
    ${XGB_INCLUDE_DIR}
)

# 5. Link Libraries
target_link_libraries(${PROJECT_NAME} PRIVATE 
    MPI::MPI_CXX
    "${XGB_LIB_PATH}"
)

# 6. Copy DLL to Output (So the .exe can actually run)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${XGB_DLL_PATH}"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>
)