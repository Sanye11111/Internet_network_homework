cmake_minimum_required(VERSION 3.10)
project(MessageSequenceDemo)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(message_sequence
    src/main.cpp
    src/multiplex.cpp
    src/modulation.cpp
)
