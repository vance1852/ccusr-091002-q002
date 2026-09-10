cmake_minimum_required(VERSION 3.16)
project(IndustrialInspection VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================
# MySQL Connector/C 查找
# ============================================
# Windows: 默认安装路径，可通过 -DMYSQL_DIR 覆盖
if(WIN32)
    set(MYSQL_DIR "C:/Program Files/MySQL/MySQL Server 8.0" CACHE PATH "MySQL installation directory")
    set(MYSQL_INCLUDE_DIR "${MYSQL_DIR}/include")
    set(MYSQL_LIB_DIR "${MYSQL_DIR}/lib")
    find_library(MYSQL_LIBRARY NAMES libmysql mysqlclient PATHS "${MYSQL_LIB_DIR}" NO_DEFAULT_PATH)
else()
    find_path(MYSQL_INCLUDE_DIR mysql.h
        PATHS /usr/include/mysql /usr/local/include/mysql /opt/homebrew/include/mysql)
    find_library(MYSQL_LIBRARY NAMES mysqlclient mysql
        PATHS /usr/lib /usr/local/lib /usr/lib64 /opt/homebrew/lib)
endif()

if(NOT MYSQL_LIBRARY)
    message(FATAL_ERROR "MySQL client library not found. Set -DMYSQL_DIR=<path>")
endif()

message(STATUS "MySQL include: ${MYSQL_INCLUDE_DIR}")
message(STATUS "MySQL library: ${MYSQL_LIBRARY}")

# ============================================
# 源文件收集 (排除测试文件)
# ============================================
file(GLOB_RECURSE MAIN_SOURCES
    "src/*.cpp"
    "src/*.h"
)
# 排除 test 目录
list(FILTER MAIN_SOURCES EXCLUDE REGEX "src/test/.*")

add_executable(${PROJECT_NAME} ${MAIN_SOURCES})

# ============================================
# 测试目标
# ============================================
file(GLOB_RECURSE TEST_SOURCES
    "src/test/*.cpp"
    "src/test/*.h"
)
# 收集非 main 的源文件 (DatabaseManager.cpp 等)
file(GLOB_RECURSE LIB_SOURCES
    "src/db/*.cpp"
)

add_executable(IndustrialInspectionTest ${TEST_SOURCES} ${LIB_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${MYSQL_INCLUDE_DIR}
)
target_link_libraries(${PROJECT_NAME} PRIVATE ${MYSQL_LIBRARY})

target_include_directories(IndustrialInspectionTest PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${MYSQL_INCLUDE_DIR}
)
target_link_libraries(IndustrialInspectionTest PRIVATE ${MYSQL_LIBRARY})

# CTest 支持
enable_testing()
add_test(NAME AllTests COMMAND IndustrialInspectionTest)

# Windows 下复制 DLL
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MYSQL_DIR}/lib/libmysql.dll"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMENT "Copying libmysql.dll to output directory"
    )
endif()
