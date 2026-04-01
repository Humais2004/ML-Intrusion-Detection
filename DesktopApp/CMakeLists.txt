# 1. Qt6 requires CMake 3.16 or higher
cmake_minimum_required(VERSION 3.16)
project("DesktopApp" VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------
# 2. QT AUTOMATION (CRITICAL)
# These turn your .ui files into C++ code automatically
# ---------------------------------------------------------
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ---------------------------------------------------------
# 3. FIND PACKAGES (Updated for OpenMP!)
# ---------------------------------------------------------
# IF CMAKE FAILS TO FIND QT, UNCOMMENT THE LINE BELOW AND FIX THE PATH
set(CMAKE_PREFIX_PATH "C:/Qt/6.11.0/msvc2022_64" ${CMAKE_PREFIX_PATH})
find_package(Qt6 COMPONENTS Widgets REQUIRED)
find_package(MPI REQUIRED)
find_package(OpenMP REQUIRED) # <--- UNCOMMENTED: Enables the Preprocessor CPU cores

# ---------------------------------------------------------
# 4. XGBOOST PATHS
# ---------------------------------------------------------
set(XGB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/external/xgboost")
set(XGB_INCLUDE_DIR "${XGB_ROOT}/include")
set(XGB_LIB_PATH "${XGB_ROOT}/lib/xgboost.lib")
set(XGB_DLL_PATH "${XGB_ROOT}/lib/xgboost.dll")

# ---------------------------------------------------------
# 5. COLLECT ALL SOURCE FILES
# ---------------------------------------------------------
# GLOB_RECURSE automatically grabs mainwindow, mpi_engine, preprocessor, data_loader, etc.
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "src/*.h"
    "ui/*.ui"
)

# ---------------------------------------------------------
# 6. CREATE EXECUTABLE
# ---------------------------------------------------------
# Simplified: No manual lists needed since SOURCES caught everything
add_executable(${PROJECT_NAME} ${SOURCES})

# ---------------------------------------------------------
# 7. INCLUDE DIRECTORIES
# ---------------------------------------------------------
target_include_directories(${PROJECT_NAME} PRIVATE 
    "src" # <--- This lets you do #include "gui/mainwindow.h"
    ${MPI_CXX_INCLUDE_DIRS}
    ${XGB_INCLUDE_DIR}
)

# ---------------------------------------------------------
# 8. LINK LIBRARIES (The Glue)
# ---------------------------------------------------------
target_link_libraries(${PROJECT_NAME} PRIVATE 
    Qt6::Widgets       
    MPI::MPI_CXX
    OpenMP::OpenMP_CXX # <--- UNCOMMENTED: Links the OpenMP library
    "${XGB_LIB_PATH}"
)

# ---------------------------------------------------------
# 9. POST-BUILD COPIES
# ---------------------------------------------------------
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${XGB_DLL_PATH}"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>
)