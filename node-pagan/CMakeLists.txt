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

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_JS_INC} ${yaml-cpp_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${CMAKE_JS_LIB} yaml-cpp::yaml-cpp cpptrace::cpptrace ${yaml-cpp_LIBRARIES})

add_compile_options("/std:c++latest")

# target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)

execute_process(COMMAND node -p "require('node-addon-api').include" WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE NODE_ADDON_API_DIR)

string(REGEX REPLACE "[\r\n\"]" "" NODE_ADDON_API_DIR ${NODE_ADDON_API_DIR})

message(STATUS ${CMAKE_JS_SRC} ${CMAKE_JS_INC})

add_definitions(-DNAPI_VERSION=4 -DNOLOG)

file(GLOB TEST_FILES "../tests/*.cpp")
add_executable(tests ${TEST_FILES} ../pagan/expr.cpp ../pagan/iowrap.cpp ../pagan/format.cc ../pagan/TypeSpec.cpp ../pagan/DynObject.cpp ../pagan/TypeRegistry.cpp ../pagan/typecast.cpp ../pagan/objectindex.cpp ../pagan/ObjectIndexTable.cpp ../pagan/StreamRegistry.cpp ../pagan/util.cpp)

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
