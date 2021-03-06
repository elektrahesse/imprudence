# -*- cmake -*-

project(llmedia)

include(00-Common)
include(LLAudio)
include(LLCommon)
include(LLImage)
include(LLMath)
include(LLMedia)
include(LLMessage)
include(LLWindow)
include(Mozlib)

include_directories(
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_PLUGINS_BASE_INCLUDE_DIRS}
    ${GSTREAMER_VIDEO_INCLUDE_DIRS}
    ${LLAUDIO_INCLUDE_DIRS}
    ${LLCOMMON_INCLUDE_DIRS}
    ${LLIMAGE_INCLUDE_DIRS}
    ${LLMATH_INCLUDE_DIRS}
    ${LLMESSAGE_INCLUDE_DIRS}
    ${LLWINDOW_INCLUDE_DIRS}
    )

set(llmedia_SOURCE_FILES
    llmediaimplcommon.cpp
    llmediaimplexample1.cpp
    llmediaimplexample2.cpp
    llmediaimplfactory.cpp
    llmediamanager.cpp
    llmediaimplgstreamer.cpp
    llmediaimplgstreamervidplug.cpp
    llgstplaythread.cpp
    )
    
set(llmedia_HEADER_FILES
    CMakeLists.txt

    llmediabase.h
    llmediaemitter.h
    llmediaimplcommon.h
    llmediaimplexample1.h
    llmediaimplexample2.h
    llmediaimplfactory.h
    llmediaimplregister.h
    llmediamanager.h
    llmediaobserver.h
    llmediaimplgstreamer.h
    llmediaimplgstreamervidplug.h
    llgstplaythread.h
    )

  # Work around a bad interaction between broken gstreamer headers and
  # g++ 4.3's increased strictness.

  if (${CXX_VERSION} MATCHES "4.[23]")
    set_source_files_properties(llmediaimplgstreamervidplug.cpp PROPERTIES
                                COMPILE_FLAGS -Wno-error=write-strings)
  endif (${CXX_VERSION} MATCHES "4.[23]")

if (MOZLIB)
  list(APPEND llmedia_SOURCE_FILES llmediaimplllmozlib.cpp)

  list(APPEND llmedia_HEADER_FILES llmediaimplllmozlib.h)
endif (MOZLIB)

set_source_files_properties(${llmedia_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND llmedia_SOURCE_FILES ${llmedia_HEADER_FILES})

add_library (llmedia ${llmedia_SOURCE_FILES})
target_link_libraries(
    llmedia
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_PLUGINS_BASE_LIBRARIES}
    ${GSTREAMER_VIDEO_LIBRARIES}
    ${QUICKTIME_LIBRARY}
    )
