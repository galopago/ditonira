cmake_minimum_required(VERSION 3.16)
project(Ditonira VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets)

find_package(Qt5SerialPort QUIET)
if(NOT Qt5SerialPort_FOUND)
  # Fallback: headers vendored under .deps (no system -dev package) + system runtime .so
  set(_SP_ROOT "${CMAKE_SOURCE_DIR}/.deps/qt5serialport-dev/usr")
  set(_SP_INC "${_SP_ROOT}/include/x86_64-linux-gnu/qt5")
  set(_SP_LIB "/usr/lib/x86_64-linux-gnu/libQt5SerialPort.so.5")
  if(NOT EXISTS "${_SP_INC}/QtSerialPort/qserialport.h")
    message(FATAL_ERROR
      "Qt5SerialPort not found. Install libqt5serialport5-dev, or extract it under "
      ".deps/qt5serialport-dev (see README).")
  endif()
  if(NOT EXISTS "${_SP_LIB}")
    message(FATAL_ERROR "libQt5SerialPort.so.5 not found at ${_SP_LIB}")
  endif()
  add_library(Qt5::SerialPort SHARED IMPORTED)
  set_target_properties(Qt5::SerialPort PROPERTIES
    IMPORTED_LOCATION "${_SP_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${_SP_INC};${_SP_INC}/QtSerialPort"
    INTERFACE_COMPILE_DEFINITIONS "QT_SERIALPORT_LIB"
    INTERFACE_LINK_LIBRARIES "Qt5::Core"
  )
  message(STATUS "Using local Qt5SerialPort headers from ${_SP_ROOT}")
endif()

add_executable(ditonira
  src/main.cpp
  src/MainWindow.cpp
  src/MainWindow.h
  src/MorseEncoder.cpp
  src/MorseEncoder.h
  src/KeyerEngine.cpp
  src/KeyerEngine.h
  src/SerialKeyLine.cpp
  src/SerialKeyLine.h
)

target_include_directories(ditonira PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(ditonira PRIVATE
  Qt5::Widgets
  Qt5::SerialPort
)

# Relative RPATH for AppImage / relocatable install layout
set_target_properties(ditonira PROPERTIES
  INSTALL_RPATH "$ORIGIN/../lib"
  BUILD_WITH_INSTALL_RPATH FALSE
)

include(GNUInstallDirs)
install(TARGETS ditonira RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES resources/ditonira.desktop DESTINATION share/applications)
install(FILES resources/icons/ditonira.png DESTINATION share/icons/hicolor/256x256/apps)
