########################################################################
# Build Soapy SDR support module for Audio Devices
########################################################################
cmake_minimum_required(VERSION 2.8.7)
project(SoapyAudio CXX)

find_package(SoapySDR "0.4.0" NO_MODULE REQUIRED)
if (NOT SoapySDR_FOUND)
    message(FATAL_ERROR "Soapy SDR development files not found...")
endif ()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

set(USE_HAMLIB ON CACHE BOOL "Support hamlib for radio control functions.")

if (USE_HAMLIB)
    find_package(hamlib REQUIRED)
    
    if (NOT hamlib_FOUND)
        message(FATAL_ERROR "hamlib development files not found...")
    endif ()
    
    include_directories(${hamlib_INCLUDE_DIRS})
    if (${hamlib_STATIC_FOUND})
        link_libraries(${hamlib_STATIC_LIBRARIES})
    else()
        link_libraries(${hamlib_LIBRARIES})
    endif()

	ADD_DEFINITIONS(-DUSE_HAMLIB)	    
endif ()

# list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/rtaudio)

#enable c++11 features
if(CMAKE_COMPILER_IS_GNUCXX)

    #C++11 is a required language feature for this project
    include(CheckCXXCompilerFlag)
    CHECK_CXX_COMPILER_FLAG("-std=c++11" HAS_STD_CXX11)
    if(HAS_STD_CXX11)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    else(HAS_STD_CXX11)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
    endif()

    #Thread support enabled (not the same as -lpthread)
    list(APPEND ${AUDIO_LIBS} -pthread)

    #disable warnings for unused parameters
    add_definitions(-Wno-unused-parameter)

endif(CMAKE_COMPILER_IS_GNUCXX)

if (APPLE)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wc++11-extensions")
endif(APPLE)

IF (WIN32)
	set(USE_AUDIO_DS ON CACHE BOOL "Support DirectSound Audio")
	set(USE_AUDIO_WASAPI OFF CACHE BOOL "Support WASAPI Audio")
	# TODO:
	# set(USE_AUDIO_ASIO OFF CACHE BOOL "Include support for ASIO Audio")

	# WASAPI
	IF(USE_AUDIO_WASAPI)
		ADD_DEFINITIONS(-D__WINDOWS_WASAPI__)	
		IF (NOT MSVC)	
			SET(AUDIO_LIBS ${AUDIO_LIBS} -luuid -lksuser)
		ENDIF(NOT MSVC)
	ENDIF(USE_AUDIO_WASAPI)

	# DirectSound
	IF (USE_AUDIO_DS)	
		ADD_DEFINITIONS(-D__WINDOWS_DS__)	
		IF (MSVC)	
			SET(AUDIO_LIBS ${AUDIO_LIBS} dsound.lib)
		ELSE (MSVC)
			SET(AUDIO_LIBS ${AUDIO_LIBS} -ldsound)
		ENDIF (MSVC)
	ENDIF(USE_AUDIO_DS)    
ENDIF (WIN32)

IF (UNIX AND NOT APPLE)
    SET(USE_AUDIO_PULSE ON CACHE BOOL "Support Pulse Audio")
    SET(USE_AUDIO_JACK OFF CACHE BOOL "Support Jack Audio")
    SET(USE_AUDIO_ALSA OFF CACHE BOOL "Support ALSA Audio")
    SET(USE_AUDIO_OSS OFF CACHE BOOL "Support OSS Audio")
  
    IF(USE_AUDIO_PULSE)
       SET (AUDIO_LIBS ${AUDIO_LIBS} pulse-simple pulse)
       ADD_DEFINITIONS(
            -D__LINUX_PULSE__
       )
    ENDIF(USE_AUDIO_PULSE)

    IF(USE_AUDIO_JACK)
       find_package(Jack)
       SET (AUDIO_LIBS ${AUDIO_LIBS} ${JACK_LIBRARIES})
       ADD_DEFINITIONS(
            -D__UNIX_JACK__
       )
       include_directories(${JACK_INCLUDE_DIRS})
    ENDIF(USE_AUDIO_JACK)

    IF(USE_AUDIO_ALSA)
       SET (AUDIO_LIBS ${AUDIO_LIBS} asound)
       set(ALSA_INCLUDE_DIR "/usr/include" CACHE FILEPATH "ALSA include path")
       include_directories(${ALSA_INCLUDE_DIR})
       set(ALSA_LIB_DIR "/usr/lib" CACHE FILEPATH "ALSA lib path")
       link_directories(${ALSA_LIB_DIR})
       ADD_DEFINITIONS(
           -D__LINUX_ALSA__
       )
    ENDIF(USE_AUDIO_ALSA)

    IF(USE_AUDIO_OSS)
       SET (AUDIO_LIBS ${AUDIO_LIBS} oss)
       ADD_DEFINITIONS(
            -D__LINUX_OSS__
       )
    ENDIF(USE_AUDIO_OSS)
ENDIF(UNIX AND NOT APPLE)

IF (APPLE)
 ADD_DEFINITIONS(
   -D__MACOSX_CORE__
 )    

FIND_LIBRARY(COREAUDIO_LIBRARY CoreAudio)
SET (AUDIO_LIBS ${COREAUDIO_LIBRARY} ${AUDIO_LIBS})
ENDIF (APPLE)

IF (USE_HAMLIB)
    SET (
        HAMLIB_SOURCES 
        RigThread.cpp
        RigThread.h
    )
ENDIF()

SOAPY_SDR_MODULE_UTIL(
    TARGET audioSupport
    SOURCES
        SoapyAudio.hpp
        Registation.cpp
        Settings.cpp
        Streaming.cpp
        RtAudio/RtAudio.cpp
        RtAudio/RtAudio.h        
        ${HAMLIB_SOURCES}
    LIBRARIES
        ${AUDIO_LIBS}
)
