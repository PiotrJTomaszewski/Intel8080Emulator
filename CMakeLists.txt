cmake_minimum_required(VERSION 3.0.0)
project(Intel8080Emulator VERSION 0.1.0)

# include(CTest)
# enable_testing()

add_executable(Intel8080Emulator main.c cpu.c memory.c)
