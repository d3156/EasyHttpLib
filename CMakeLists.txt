cmake_minimum_required(VERSION 3.16)
project(EasyHttpLib VERSION 1.0.1 LANGUAGES CXX)
include("${CMAKE_CURRENT_SOURCE_DIR}/../../tools/workspace.cmake")
create_target(library)

find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)
target_link_libraries(EasyHttpLib PUBLIC OpenSSL::SSL OpenSSL::Crypto)

find_package(Boost 1.8 REQUIRED COMPONENTS system thread)
target_link_libraries(EasyHttpLib PUBLIC Boost::system Boost::thread)
