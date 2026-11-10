cmake_minimum_required(VERSION 3.16)
project(SmartVehicleRental VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /EHsc)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Core library: all domain objects, services and persistence. No console I/O.
add_library(rental_core STATIC
    src/Money.cpp
    src/Date.cpp
    src/Enums.cpp
    src/TextUtil.cpp
    src/VehicleModel.cpp
    src/Vehicle.cpp
    src/VehicleTypes.cpp
    src/Customer.cpp
)
target_include_directories(rental_core PUBLIC include)

# Console application.
add_executable(rental_app
    app/main.cpp
)
target_link_libraries(rental_app PRIVATE rental_core)

# Automated tests (no external framework), registered with CTest.
enable_testing()
add_executable(rental_tests
    tests/test_main.cpp
    tests/test_domain.cpp
)
target_link_libraries(rental_tests PRIVATE rental_core)
add_test(NAME rental_tests COMMAND rental_tests)
