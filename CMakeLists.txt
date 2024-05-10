cmake_minimum_required(VERSION 3.10)
if (POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()
project(wspp_server VERSION 0.1.3)

#### option
option(WSPP_SERVER_USE_SSL "Using SSL" OFF)

#### compile setting
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/source-charset:utf-8>")

#### path setting
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(MYFRAME_INSTALL_SERVICE_DIR ${CMAKE_INSTALL_PREFIX}/service)
set(MYFRAME_INSTALL_LIB_DIR ${CMAKE_INSTALL_PREFIX}/lib)

#### dep 3rd lib
find_package(Threads REQUIRED)
find_package(websocketpp REQUIRED)
find_package(OpenSSL)
find_package(Boost COMPONENTS system thread)
find_package(Protobuf REQUIRED)
find_package(myframe REQUIRED)
find_package(myframe_pb REQUIRED)

#### include directory
include_directories(${Protobuf_INCLUDE_DIRS})

#### config
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/config.h)

#### lib
add_library(${PROJECT_NAME} SHARED
    ${PROJECT_NAME}.cpp
    wspp_server_impl.cpp
)
target_link_libraries(${PROJECT_NAME}
    Threads::Threads dl
    ssl crypto
    ${Boost_LIBRARIES}
    ${Protobuf_LIBRARIES}
    myframe
    myframe_pb
)

#### install
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${MYFRAME_INSTALL_LIB_DIR}
    ARCHIVE DESTINATION ${MYFRAME_INSTALL_LIB_DIR}
    RUNTIME DESTINATION ${MYFRAME_INSTALL_LIB_DIR}
)
# 通用组件不直接安装配置文件，而是在项目集成中安装定制化的配置文件
# FILE(GLOB conf_files "*.json")
# INSTALL(FILES
#     ${conf_files}
#     PERMISSIONS
#     OWNER_READ
#     GROUP_READ
#     WORLD_READ
#     DESTINATION ${MYFRAME_INSTALL_SERVICE_DIR}
# )
