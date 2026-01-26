cmake_minimum_required(VERSION 3.16)
project(EasyHttpLib
  VERSION 1.0.1
  LANGUAGES CXX
)
add_compile_definitions(EasyHttpLib_VERSION="${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE SRC_FILES
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
)

add_library(EasyHttpLib STATIC ${SRC_FILES})

target_include_directories(EasyHttpLib
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_definitions(EasyHttpLib PRIVATE $<$<COMPILE_LANGUAGE:CXX>:LOG_NAME="EasyHttp">)

# ### BOOST ####
set(Boost_USE_STATIC_LIBS ON)

find_package(OpenSSL REQUIRED)
find_package(Boost 1.75 REQUIRED COMPONENTS system thread)

target_include_directories(EasyHttpLib PRIVATE
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(EasyHttpLib PRIVATE
  Boost::system
  Boost::thread
  OpenSSL::SSL
  OpenSSL::Crypto
)

set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
set_target_properties(EasyHttpLib PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ### BOOST ####