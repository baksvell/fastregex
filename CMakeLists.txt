cmake_minimum_required(VERSION 3.20)
project(FastRegex
    LANGUAGES CXX
    VERSION 2.1.0
    DESCRIPTION "High-performance regex with JIT and SIMD optimizations"
)

# Настройка политик CMake
cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0074 NEW)
cmake_policy(SET CMP0063 NEW)
cmake_policy(SET CMP0069 NEW)

# Опции конфигурации
option(FASTREGEX_ENABLE_JIT "Enable JIT compilation" ON)
option(FASTREGEX_ENABLE_SIMD "Enable SIMD optimizations" ON)
option(FASTREGEX_ENABLE_TESTS "Build tests" OFF)
option(FASTREGEX_ENABLE_BENCHMARKS "Build benchmarks" OFF)

# Настройки компилятора
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Настройки выходных файлов
set(CMAKE_DEBUG_POSTFIX "_d")
set(CMAKE_RELWITHDEBINFO_POSTFIX "_rd")
set(CMAKE_MINSIZEREL_POSTFIX "_ms")

# Поиск Python
find_package(Python 3.10 REQUIRED COMPONENTS Interpreter Development.Module)
find_package(pybind11 REQUIRED CONFIG)

# Настройки для LLVM/JIT
if(FASTREGEX_ENABLE_JIT)
    find_package(LLVM 20.1 REQUIRED CONFIG)
    message(STATUS "JIT compilation enabled")

    llvm_map_components_to_libnames(LLVM_LIBS
        Core
        ExecutionEngine
        OrcJIT
        Support
        Target
        X86CodeGen
        X86AsmParser
        X86Desc
        X86Disassembler
    )
endif()

# Настройки для MinGW
set(MINGW_ROOT "C:/msys64/mingw64")
if(MINGW AND EXISTS "${MINGW_ROOT}/bin/g++.exe")
    message(STATUS "MinGW found at: ${MINGW_ROOT}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wa,-mbig-obj -D_GLIBCXX_USE_CXX11_ABI=1")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static -static-libgcc -static-libstdc++ -Wl,--allow-multiple-definition -Wl,--enable-auto-import -lssp")
endif()

# Глобальные флаги компиляции
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -fvisibility=hidden
    -fexceptions
    -fPIC
)

# SIMD оптимизации
if(FASTREGEX_ENABLE_SIMD AND CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|AMD64|i[3-6]86)")
    add_compile_options(-mavx2 -mbmi2 -mfma -msse4.2)
    message(STATUS "x86 SIMD optimizations enabled")
endif()

# Исходные файлы
set(SOURCE_FILES
    src/fastregex.cpp
    src/python/module.cpp
)

# Добавление SIMD файлов
if(FASTREGEX_ENABLE_SIMD)
    set(SIMD_FILE src/jit_engine/simd.cpp)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${SIMD_FILE})
        list(APPEND SOURCE_FILES ${SIMD_FILE})
        message(STATUS "Found SIMD file: ${SIMD_FILE}")
    else()
        message(WARNING "SIMD enabled but ${SIMD_FILE} not found - disabling SIMD")
        set(FASTREGEX_ENABLE_SIMD OFF)
    endif()
endif()

# Добавление JIT файлов
if(FASTREGEX_ENABLE_JIT)
    set(JIT_FILES
        src/jit_engine/compiler.cpp
        src/jit_engine/regex_impl.cpp
    )

    foreach(jit_file ${JIT_FILES})
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${jit_file})
            list(APPEND SOURCE_FILES ${jit_file})
            message(STATUS "Found JIT file: ${jit_file}")
        else()
            message(WARNING "JIT enabled but ${jit_file} not found - disabling JIT")
            set(FASTREGEX_ENABLE_JIT OFF)
            break()
        endif()
    endforeach()
endif()

# Создание модуля
pybind11_add_module(fastregex ${SOURCE_FILES})

# Функция для копирования DLL (должна быть объявлена ПОСЛЕ создания таргета)
function(copy_dll_to_build dll_path)
    if(EXISTS "${dll_path}")
        get_filename_component(dll_name "${dll_path}" NAME)
        add_custom_command(TARGET fastregex POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${dll_path}"
                "${CMAKE_BINARY_DIR}/${dll_name}"
            COMMENT "Copying ${dll_name} to build directory"
        )
        message(STATUS "Configured DLL: ${dll_path}")
    else()
        message(WARNING "DLL not found: ${dll_path}")
    endif()
endfunction()

# Поиск и копирование Python DLL
find_file(PYTHON_DLL
    NAMES "python${Python_VERSION_MAJOR}${Python_VERSION_MINOR}.dll"
    PATHS
        "${Python_RUNTIME_LIBRARY_DIRS}"
        "${Python_ROOT_DIR}"
        "$ENV{USERPROFILE}/AppData/Local/Programs/Python/Python${Python_VERSION_MAJOR}${Python_VERSION_MINOR}"
    NO_DEFAULT_PATH
)
if(PYTHON_DLL)
    copy_dll_to_build("${PYTHON_DLL}")
else()
    message(FATAL_ERROR "Python DLL not found! Searched in: ${Python_RUNTIME_LIBRARY_DIRS} and ${Python_ROOT_DIR}")
endif()

# Копирование всех необходимых MinGW DLL
if(MINGW)
    set(MINGW_DLLS
        "${MINGW_ROOT}/bin/libstdc++-6.dll"
        "${MINGW_ROOT}/bin/libgcc_s_seh-1.dll"
        "${MINGW_ROOT}/bin/libwinpthread-1.dll"
        "${MINGW_ROOT}/bin/zlib1.dll"
        "${MINGW_ROOT}/bin/libzstd.dll"
    )

    foreach(DLL_PATH IN LISTS MINGW_DLLS)
        if(EXISTS "${DLL_PATH}")
            get_filename_component(DLL_NAME "${DLL_PATH}" NAME)
            add_custom_command(TARGET fastregex POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${DLL_PATH}"
                    "${CMAKE_BINARY_DIR}/${DLL_NAME}"
                COMMENT "Copying ${DLL_NAME} to build directory"
            )
            message(STATUS "Configured MinGW DLL: ${DLL_NAME}")
        else()
            message(WARNING "MinGW DLL not found: ${DLL_PATH}")
        endif()
    endforeach()
endif()

# Настройки include
target_include_directories(fastregex PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/jit_engine
    ${Python_INCLUDE_DIRS}
)

if(FASTREGEX_ENABLE_JIT)
    target_include_directories(fastregex PRIVATE
        ${LLVM_INCLUDE_DIRS}
    )
    target_compile_definitions(fastregex PRIVATE
        ${LLVM_DEFINITIONS}
    )
endif()

# Определения компилятора
target_compile_definitions(fastregex PRIVATE
    FASTREGEX_EXPORTS
    $<$<BOOL:${FASTREGEX_ENABLE_JIT}>:ENABLE_JIT=1>
    $<$<BOOL:${FASTREGEX_ENABLE_SIMD}>:ENABLE_SIMD=1>
)

# Опции оптимизации
target_compile_options(fastregex PRIVATE
    $<$<CONFIG:Release>:-O3 -DNDEBUG -fomit-frame-pointer>
    $<$<CONFIG:Debug>:-O0 -g3 -DDEBUG -fno-omit-frame-pointer>
)

# Настройки линковки
target_link_libraries(fastregex PRIVATE
    ${Python_LIBRARIES}
    pybind11::module
    $<$<BOOL:${FASTREGEX_ENABLE_JIT}>:${LLVM_LIBS}>
    $<$<BOOL:${MINGW}>:psapi shell32 ole32 advapi32>
)

# Windows-specific settings
if(WIN32)
    set_target_properties(fastregex PROPERTIES
        SUFFIX ".pyd"
        PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    # Создание скрипта для настройки окружения
    set(SETUP_ENV_CONTENT "@echo off\n")
    string(APPEND SETUP_ENV_CONTENT "set PATH=${Python_RUNTIME_LIBRARY_DIRS};${MINGW_ROOT}/bin;%PATH%\n")
    string(APPEND SETUP_ENV_CONTENT "echo Environment configured for FastRegex\n")
    file(WRITE ${CMAKE_BINARY_DIR}/setup_env.bat "${SETUP_ENV_CONTENT}")

    # Проверка после сборки
    add_custom_command(TARGET fastregex POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "=== Build completed ==="
        COMMAND ${CMAKE_COMMAND} -E echo "To run the module:"
        COMMAND ${CMAKE_COMMAND} -E echo "1. Call setup_env.bat to configure environment"
        COMMAND ${CMAKE_COMMAND} -E echo "2. Run your Python script"
    )
endif()

# Вывод итоговой конфигурации
message(STATUS "\n=== FastRegex Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Python: ${Python_EXECUTABLE}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "JIT support: ${FASTREGEX_ENABLE_JIT}")
message(STATUS "SIMD support: ${FASTREGEX_ENABLE_SIMD}")
message(STATUS "================================")