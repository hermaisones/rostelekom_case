cmake_minimum_required(VERSION 3.21)

project(RectangleLab LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql)

qt_add_executable(rectangle_lab
    main.cpp
    rectanglewindow.cpp
    rectanglewindow.h
)
set_target_properties(rectangle_lab PROPERTIES WIN32_EXECUTABLE TRUE)
target_compile_features(rectangle_lab PRIVATE cxx_std_17)
target_link_libraries(rectangle_lab PRIVATE Qt6::Widgets Qt6::Sql)

if(MSVC)
    target_compile_options(rectangle_lab PRIVATE /utf-8)
endif()
