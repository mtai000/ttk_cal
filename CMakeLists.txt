cmake_minimum_required(VERSION 3.22)
project(ttk_cal)

add_executable(${PROJECT_NAME} src/main.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND  ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/data.csv $<TARGET_FILE_DIR:${PROJECT_NAME}>/data.csv)
