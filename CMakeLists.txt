cmake_minimum_required(VERSION 3.0.0)
project(demo VERSION 0.1.0)

include(CTest)
enable_testing()

set(LIBWEBRTC_INCLUDE_PATH "/home/lei/code/prebuilt/m87.4280.10.0/include")
set(LIBWEBRTC_BINARY_PATH "/home/lei/code/prebuilt/m87.4280.10.0/lib")


set(
    SOURCE_FILES_DATACHANNEL
    src/datachannel/main.cpp
)

set(
    SOUCE_FILES_VIDEO
    src/video-channel/main.cpp
    src/video-channel/i420_creator.cpp
    src/video-channel/i420_creator.h
)

set(
    SOUCE_FILES_AUDIO
    src/audio-channel/main.cpp
)


# datachannel
add_executable(demo-data "${SOURCE_FILES_DATACHANNEL}")
target_include_directories(
    demo-data PUBLIC
    "${LIBWEBRTC_INCLUDE_PATH}"
	"${LIBWEBRTC_INCLUDE_PATH}/third_party/abseil-cpp"
)
target_link_libraries(
    demo-data 
    ${LIBWEBRTC_BINARY_PATH}/libwebrtc${CMAKE_STATIC_LIBRARY_SUFFIX}
    pthread
    dl
)
target_compile_definitions(demo-data PUBLIC
	$<$<NOT:$<PLATFORM_ID:Windows>>:WEBRTC_POSIX>
	$<$<PLATFORM_ID:Windows>:WEBRTC_WIN>
	$<$<PLATFORM_ID:Darwin>:WEBRTC_MAC>
)

#video
add_executable(demo-video "${SOUCE_FILES_VIDEO}")
target_include_directories(
    demo-video PUBLIC
    "${LIBWEBRTC_INCLUDE_PATH}"
	"${LIBWEBRTC_INCLUDE_PATH}/third_party/abseil-cpp"
)
target_link_libraries(
    demo-video 
    ${LIBWEBRTC_BINARY_PATH}/libwebrtc${CMAKE_STATIC_LIBRARY_SUFFIX}
    pthread
    dl
)
target_compile_definitions(demo-video PUBLIC
	$<$<NOT:$<PLATFORM_ID:Windows>>:WEBRTC_POSIX>
	$<$<PLATFORM_ID:Windows>:WEBRTC_WIN>
	$<$<PLATFORM_ID:Darwin>:WEBRTC_MAC>
)


#audio
add_executable(demo-audio "${SOUCE_FILES_AUDIO}")
target_include_directories(
    demo-audio PUBLIC
    "${LIBWEBRTC_INCLUDE_PATH}"
	"${LIBWEBRTC_INCLUDE_PATH}/third_party/abseil-cpp"
)
target_link_libraries(
    demo-audio 
    ${LIBWEBRTC_BINARY_PATH}/libwebrtc${CMAKE_STATIC_LIBRARY_SUFFIX}
    pthread
    dl
)

# Compile definitions for libwebrtc.
target_compile_definitions(demo-audio PUBLIC
	$<$<NOT:$<PLATFORM_ID:Windows>>:WEBRTC_POSIX>
	$<$<PLATFORM_ID:Windows>:WEBRTC_WIN>
	$<$<PLATFORM_ID:Darwin>:WEBRTC_MAC>
)





# set(CPACK_PROJECT_NAME ${PROJECT_NAME})
# set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
# include(CPack)
