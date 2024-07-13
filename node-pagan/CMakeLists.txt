cmake_minimum_required(VERSION 3.25)

project(node-pagan)

include_directories(${CMAKE_JS_INC} "../pagan")
link_directories("../pagan/src")
file(GLOB HEADER_FILES "src/*.h")
file(GLOB SOURCE_FILES "src/*.cpp")
file(GLOB HEADER_FILES_PAGAN "../pagan/*.h")
file(GLOB HEADER_FILES_SYEXPR "../pagan/syexpr/*.h")
file(GLOB SOURCE_FILES_PAGAN "../pagan/*.cpp")
file(GLOB SOURCE_FILES_SYEXPR "../pagan/syexpr/*.cpp")

# list(REMOVE_ITEM SOURCE_FILES_PAGAN "*/../pagan/mainesp.cpp")
list(FILTER SOURCE_FILES_PAGAN EXCLUDE REGEX ".*/mainesp.cpp")

set(EXTERN "${CMAKE_SOURCE_DIR}/../extern")
set(CMAKE_DEBUG_POSTFIX "d")

add_definitions(-DNOMINMAX)

find_package(yaml-cpp REQUIRED)
find_package(Catch2 REQUIRED)
find_package(cpptrace REQUIRED)


if(MSVC AND CMAKE_JS_NODELIB_DEF AND CMAKE_JS_NODELIB_TARGET)
  # Generate node.lib
  execute_process(COMMAND ${CMAKE_AR} /def:${CMAKE_JS_NODELIB_DEF} /out:${CMAKE_JS_NODELIB_TARGET} ${CMAKE_STATIC_LINKER_FLAGS})
endif()

set(SOURCES ${SOURCE_FILES} ${SOURCE_FILES_PAGAN} ${SOURCE_FILES_SYEXPR} ../pagan/format.cc)

add_library(${PROJECT_NAME} SHARED ${HEADER_FILES} ${HEADER_FILES_PAGAN} ${HEADER_FILES_SYEXPR} ${SOURCES} ${CMAKE_JS_SRC})

# set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_JS_INC} ${yaml-cpp_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${CMAKE_JS_LIB} yaml-cpp::yaml-cpp cpptrace::cpptrace ${yaml-cpp_LIBRARIES})

# target_link_libraries(${PROJECT_NAME}
#     debug ${EXTERN}/yaml-cpp/build/Debug/yaml-cppd.lib
#     optimized ${EXTERN}/yaml-cpp/build/Release/yaml-cpp.lib
# )

add_compile_options("/std:c++latest")

# target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)

execute_process(COMMAND node -p "require('node-addon-api').include" WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE NODE_ADDON_API_DIR)

string(REGEX REPLACE "[\r\n\"]" "" NODE_ADDON_API_DIR ${NODE_ADDON_API_DIR})

message(STATUS ${CMAKE_JS_SRC} ${CMAKE_JS_INC})
# target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_JS_INC} ${NODE_ADDON_API_DIR} ${EXTERN}/yaml-cpp/include)
# target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_JS_INC} ${NODE_ADDON_API_DIR})
# target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_JS_INC})

add_definitions(-DNAPI_VERSION=4 -DNOLOG)
# add_definitions(-DNAPI_VERSION=4)

# Include(FetchContent)
# 
# FetchContent_Declare(
#  Catch2
#  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
#  GIT_TAG        v3.6.0)
# 
# FetchContent_MakeAvailable(Catch2)
# FetchContent_GetProperties(Catch2)
# if (NOT Catch2_POPULATED)
#   message(status ${Catch2_SOURCE_DIR} -- ${Catch2_BINARY_DIR})
#   # add_subdirectory(${Catch2_SOURCE_DIR}/include)
# endif()

file(GLOB TEST_FILES "../tests/*.cpp")
add_executable(tests ${TEST_FILES} ../pagan/expr.cpp ../pagan/iowrap.cpp ../pagan/format.cc ../pagan/TypeSpec.cpp ../pagan/DynObject.cpp ../pagan/TypeRegistry.cpp ../pagan/typecast.cpp ../pagan/objectindex.cpp ../pagan/ObjectIndexTable.cpp ../pagan/StreamRegistry.cpp ../pagan/util.cpp ../pagan/syexpr/shunting_yard.cpp ../pagan/syexpr/evaluate.cpp ../pagan/syexpr/token.cpp)
# target_include_directories(tests PRIVATE ${Catch2_SOURCE_DIR}/single_include/catch2)
# target_include_directories(tests PRIVATE ${CMAKE_JS_INC} ${EXTERN}/PEGTL/include ${EXTERN}/yaml-cpp/include ${EXTERN}/StackWalker/Main/StackWalker)
# target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)

# catch2_Catch2_Catch2WithMain
target_include_directories(tests PRIVATE ${Catch2_INCLUDE_DIRS})

target_link_libraries(tests
  PUBLIC
    Catch2::Catch2WithMain
    yaml-cpp::yaml-cpp
    cpptrace::cpptrace)
set_property(TARGET tests PROPERTY CXX_STANDARD 20)

# generate pdbs in release builds because cmake-js doesn't support RelWithDebInfo as a build target
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
