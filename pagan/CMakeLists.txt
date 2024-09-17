set(CMAKE_DEBUG_POSTFIX "d")

add_definitions(-DNOMINMAX)

find_package(yaml-cpp REQUIRED)
find_package(cpptrace REQUIRED)
find_package(pagan-expr REQUIRED)

set(SOURCES
  constants.h
  DynObject.cpp
  DynObject.h
  expr.cpp
  expr.h
  flexi_cast.cpp
  flexi_cast.h
  iowrap.cpp
  iowrap.h
  IScriptQuery.h
  objectindex.cpp
  objectindex.h
  ObjectIndexTable.cpp
  ObjectIndexTable.h
  Parser.cpp
  Parser.h
  parserFromKSY.cpp
  parserFromKSY.h
  StreamRegistry.cpp
  StreamRegistry.h
  typecast.cpp
  typecast.h
  TypeProperty.h
  TypeRegistry.cpp
  TypeRegistry.h
  types.h
  Typespec.cpp
  TypeSpec.h
  util.cpp
  util.h
)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(pagan::pagan ALIAS ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} yaml-cpp::yaml-cpp cpptrace::cpptrace pagan::expr)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)

set_target_properties(${PROJECT_NAME} PROPERTIES EXPORT_NAME "pagan::pagan")
