cmake_minimum_required(VERSION 3.28)
project(HeliosCVScript LANGUAGES CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Helios CV C++ scripts require Windows x64.")
endif()

if(NOT HELIOS_SDK_DIR)
    get_filename_component(HELIOS_SDK_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()

find_package(OpenCV 5.0.0 EXACT REQUIRED COMPONENTS core)

add_library(CVTemplate SHARED CVTemplate.cpp)
add_library(CVTest SHARED CVTest.cpp)

foreach(target IN ITEMS CVTemplate CVTest)
    target_compile_features(${target} PRIVATE cxx_std_23)
    target_include_directories(${target} PRIVATE
        "${HELIOS_SDK_DIR}/include"
        ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(${target} PRIVATE ${OpenCV_LIBS})
endforeach()

if(MSVC)
    foreach(target IN ITEMS CVTemplate CVTest)
        set_property(TARGET ${target} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            $<$<CONFIG:Release>:/O2>
            $<$<CONFIG:Release>:/fp:fast>
        )
    endforeach()
endif()
