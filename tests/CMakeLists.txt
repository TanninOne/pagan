set(TEST_NAME "${PROJECT_NAME}_tests")

add_definitions(-DNOMINMAX)

find_package(Catch2 CONFIG REQUIRED)

set(SOURCES
  dynobject.cpp 
  expr.cpp
  flexi_cast.cpp
  iowrap.cpp
  typeregistry.cpp
  typespec.cpp
)

add_executable(${TEST_NAME} ${SOURCES})

target_link_libraries(${TEST_NAME} PUBLIC Catch2::Catch2WithMain pagan::pagan)

target_include_directories(${TEST_NAME} PRIVATE ${Catch2_INCLUDE_DIRS})

target_include_directories(${TEST_NAME} PRIVATE ${Catch2_INCLUDE_DIRS})
