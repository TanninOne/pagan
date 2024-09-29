set(CMAKE_DEBUG_POSTFIX "d")

add_definitions(-DNOMINMAX)

option(NOLOG "Disable logging" OFF)

find_package(yaml-cpp REQUIRED)
find_package(cpptrace REQUIRED)
find_package(pagan-expr CONFIG REQUIRED)

set(HEADERS
  constants.h
  DynObject.h
  flexi_cast.h
  iowrap.h
  IScriptQuery.h
  objectindex.h
  ObjectIndexTable.h
  Parser.h
  parserFromKSY.h
  StreamRegistry.h
  TypeProperty.h
  TypeRegistry.h
  typecast.h
  types.h
  util.h
  )

set(SOURCES
  DynObject.cpp
  expr.cpp
  expr.h
  flexi_cast.cpp
  iowrap.cpp
  objectindex.cpp
  ObjectIndexTable.cpp
  Parser.cpp
  parserFromKSY.cpp
  StreamRegistry.cpp
  typecast.cpp
  TypeRegistry.cpp
  Typespec.cpp
  TypeSpec.h
  util.cpp
)

if (MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

if (NOLOG)
  add_definitions(-DNOLOG)
endif()

add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(pagan::pagan ALIAS ${PROJECT_NAME})

target_sources(${PROJECT_NAME}
  PUBLIC FILE_SET HEADERS FILES ${HEADERS}
)

target_link_libraries(${PROJECT_NAME} yaml-cpp::yaml-cpp cpptrace::cpptrace pagan::expr)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)

set_target_properties(${PROJECT_NAME} PROPERTIES EXPORT_NAME "pagan::pagan")
