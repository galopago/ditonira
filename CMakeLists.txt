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

# --- ONNX Runtime (DeepCW RX) ---
set(ONNXRUNTIME_ROOT "${CMAKE_SOURCE_DIR}/.deps/onnxruntime-linux-x64-1.19.2"
    CACHE PATH "ONNX Runtime root (headers + lib)")
if(NOT EXISTS "${ONNXRUNTIME_ROOT}/include/onnxruntime_cxx_api.h")
  message(FATAL_ERROR
    "ONNX Runtime not found at ${ONNXRUNTIME_ROOT}. "
    "Download onnxruntime-linux-x64 into .deps (see docs/ARCHITECTURE_RX_DEEPCW.md §8).")
endif()
add_library(onnxruntime SHARED IMPORTED)
set_target_properties(onnxruntime PROPERTIES
  IMPORTED_LOCATION "${ONNXRUNTIME_ROOT}/lib/libonnxruntime.so"
  INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_ROOT}/include"
)

# --- PortAudio (static, ALSA) ---
set(PORTAUDIO_ROOT "${CMAKE_SOURCE_DIR}/.deps/portaudio-install"
    CACHE PATH "PortAudio install prefix")
set(ALSA_LIB_DIR "${CMAKE_SOURCE_DIR}/.deps/alsa-lib"
    CACHE PATH "Directory containing libasound.so symlink")
if(NOT EXISTS "${PORTAUDIO_ROOT}/include/portaudio.h")
  message(FATAL_ERROR "PortAudio not found at ${PORTAUDIO_ROOT}")
endif()

set(DITONIRA_RX_SOURCES
  src/rx/Stft.cpp
  src/rx/Stft.h
  src/rx/BandpassFilter.cpp
  src/rx/BandpassFilter.h
  src/rx/SpectrogramBuilder.cpp
  src/rx/SpectrogramBuilder.h
  src/rx/CtcDecoder.cpp
  src/rx/CtcDecoder.h
  src/rx/OnnxCwModel.cpp
  src/rx/OnnxCwModel.h
  src/rx/WavIo.cpp
  src/rx/WavIo.h
  src/rx/DeepCwConstants.h
  src/rx/AudioRingBuffer.cpp
  src/rx/AudioRingBuffer.h
  src/rx/PortAudioHost.cpp
  src/rx/PortAudioHost.h
  src/rx/AudioCapture.cpp
  src/rx/AudioCapture.h
  src/rx/MonitorFifo.cpp
  src/rx/MonitorFifo.h
  src/rx/AudioMonitor.cpp
  src/rx/AudioMonitor.h
  src/rx/DecodeEngine.cpp
  src/rx/DecodeEngine.h
  src/rx/StreamingTranscript.cpp
  src/rx/StreamingTranscript.h
  src/rx/PileupTracker.cpp
  src/rx/PileupTracker.h
  src/rx/ModelPath.cpp
  src/rx/ModelPath.h
  src/rx/RxPanel.cpp
  src/rx/RxPanel.h
  src/rx/RxSpectrumWidget.cpp
  src/rx/RxSpectrumWidget.h
)

add_library(ditonira_rx STATIC ${DITONIRA_RX_SOURCES})
target_include_directories(ditonira_rx PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${PORTAUDIO_ROOT}/include"
)
target_link_libraries(ditonira_rx PUBLIC
  onnxruntime
  Qt5::Widgets
  "${PORTAUDIO_ROOT}/lib/libportaudio.a"
  pthread dl m rt
)
if(EXISTS "${ALSA_LIB_DIR}/libasound.so")
  target_link_libraries(ditonira_rx PUBLIC "${ALSA_LIB_DIR}/libasound.so")
else()
  target_link_libraries(ditonira_rx PUBLIC asound)
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
  ditonira_rx
)

add_executable(deepcw_wav_decode
  tools/deepcw_wav_decode.cpp
)
target_link_libraries(deepcw_wav_decode PRIVATE ditonira_rx)

add_executable(pileup_wav_smoke
  tools/pileup_wav_smoke.cpp
)
target_link_libraries(pileup_wav_smoke PRIVATE ditonira_rx)

set(_RPATH_LIBS "$ORIGIN/../lib;${ONNXRUNTIME_ROOT}/lib")
set_target_properties(ditonira PROPERTIES
  INSTALL_RPATH "$ORIGIN/../lib"
  BUILD_RPATH "${ONNXRUNTIME_ROOT}/lib"
  BUILD_WITH_INSTALL_RPATH FALSE
)
set_target_properties(deepcw_wav_decode PROPERTIES
  BUILD_RPATH "${ONNXRUNTIME_ROOT}/lib"
  INSTALL_RPATH "$ORIGIN/../lib"
)
set_target_properties(pileup_wav_smoke PROPERTIES
  BUILD_RPATH "${ONNXRUNTIME_ROOT}/lib"
  INSTALL_RPATH "$ORIGIN/../lib"
)

include(GNUInstallDirs)
install(TARGETS ditonira RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS deepcw_wav_decode RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES resources/ditonira.desktop DESTINATION share/applications)
install(FILES resources/icons/ditonira.png DESTINATION share/icons/hicolor/256x256/apps)
install(FILES
          resources/models/deepcw_en.onnx
          resources/models/deepcw_en_narrow.onnx
          resources/models/NOTICE.txt
        DESTINATION share/ditonira/models)
install(FILES "${ONNXRUNTIME_ROOT}/lib/libonnxruntime.so.1.19.2"
        DESTINATION ${CMAKE_INSTALL_LIBDIR})
# Symlinks for the loader
install(CODE "
  execute_process(COMMAND \"\${CMAKE_COMMAND}\" -E create_symlink
    libonnxruntime.so.1.19.2
    \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libonnxruntime.so.1\")
  execute_process(COMMAND \"\${CMAKE_COMMAND}\" -E create_symlink
    libonnxruntime.so.1
    \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libonnxruntime.so\")
")
