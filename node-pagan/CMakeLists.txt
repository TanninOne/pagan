cmake_minimum_required(VERSION 3.17)

project(node-pagan)
include_directories(${CMAKE_JS_INC} "../pagan")
link_directories("../pagan/src")
file(GLOB HEADER_FILES "src/*.h")
file(GLOB SOURCE_FILES "src/*.cpp")
file(GLOB HEADER_FILES_PAGAN "../pagan/*.h")
file(GLOB SOURCE_FILES_PAGAN "../pagan/*.cpp")

# list(REMOVE_ITEM SOURCE_FILES_PAGAN "*/../pagan/mainesp.cpp")
list(FILTER SOURCE_FILES_PAGAN EXCLUDE REGEX ".*/mainesp.cpp")

set(EXTERN "${CMAKE_SOURCE_DIR}/../extern")
set(CMAKE_DEBUG_POSTFIX "d")

message(status ${SOURCE_FILES})
message(status ${SOURCE_FILES_PAGAN})
message(status "Build config $<CONFIG:foobar> - ${CMAKE_BUILD_TYPE} - ${CMAKE_DEBUG_POSTFIX}")

set(SOURCES ${SOURCE_FILES} ${SOURCE_FILES_PAGAN} ../pagan/format.cc)
message(status ${SOURCES})

add_library(${PROJECT_NAME} SHARED ${HEADER_FILES} ${HEADER_FILES_PAGAN} ${SOURCES} ${CMAKE_JS_SRC})

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")

target_link_libraries(${PROJECT_NAME} ${CMAKE_JS_LIB})

target_link_libraries(${PROJECT_NAME}
    debug ${EXTERN}/yaml-cpp/build/Debug/yaml-cppd.lib
    optimized ${EXTERN}/yaml-cpp/build/Release/yaml-cpp.lib
)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

execute_process(COMMAND node -p "require('node-addon-api').include" WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE NODE_ADDON_API_DIR)

string(REGEX REPLACE "[\r\n\"]" "" NODE_ADDON_API_DIR ${NODE_ADDON_API_DIR})

target_include_directories(${PROJECT_NAME} PRIVATE ${NODE_ADDON_API_DIR} ${EXTERN}/PEGTL/include ${EXTERN}/yaml-cpp/include)

add_definitions(-DNAPI_VERSION=3 -DNOLOG)

Include(FetchContent)

FetchContent_Declare(
 Catch2
 GIT_REPOSITORY https://github.com/catchorg/Catch2.git
 GIT_TAG        v2.13.1)

FetchContent_MakeAvailable(Catch2)
FetchContent_GetProperties(Catch2)
if (NOT Catch2_POPULATED)
  message(status ${Catch2_SOURCE_DIR} -- ${Catch2_BINARY_DIR})
  # add_subdirectory(${Catch2_SOURCE_DIR}/include)
endif()

file(GLOB TEST_FILES "../tests/*.cpp")
add_executable(tests ${TEST_FILES} ../pagan/expr.cpp ../pagan/iowrap.cpp ../pagan/format.cc ../pagan/TypeSpec.cpp ../pagan/DynObject.cpp ../pagan/TypeRegistry.cpp ../pagan/typecast.cpp ../pagan/objectindex.cpp ../pagan/ObjectIndexTable.cpp ../pagan/StreamRegistry.cpp)
target_include_directories(tests PRIVATE ${Catch2_SOURCE_DIR}/single_include/catch2)
target_include_directories(tests PRIVATE ${EXTERN}/PEGTL/include ${EXTERN}/yaml-cpp/include)
target_link_libraries(tests PRIVATE Catch2::Catch2)
target_compile_features(tests PRIVATE cxx_std_17)

# generate pdbs in release builds because cmake-js doesn't support RelWithDebInfo as a build target
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
