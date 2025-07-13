# 只包含当前已存在的文件
set(SOURCES
    main.cpp
    MediaPlayer.cpp
    MediaPlayer.h
    VideoDecoder.h
    VideoDecoder.cpp
    AudioDecoder.h
    AudioDecoder.cpp
)

# 创建可执行文件
add_executable(MediaPlayer ${SOURCES})

# 包含目录
target_include_directories(MediaPlayer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FFMPEG_INCLUDE_DIR}
    ${SDL2_INCLUDE_DIR}
)

# 链接库
target_link_libraries(MediaPlayer PRIVATE
    ${FFMPEG_LIBS}
    ${SDL2_LIBS}
)

# Windows下的DLL复制
if(WIN32)
    # 复制FFmpeg DLL
    add_custom_command(TARGET MediaPlayer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${FFMPEG_DIR}/bin/"
        $<TARGET_FILE_DIR:MediaPlayer>
    )
    
    # 复制SDL2 DLL
    add_custom_command(TARGET MediaPlayer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        "${SDL2_DIR}/bin/SDL2.dll"
        $<TARGET_FILE_DIR:MediaPlayer>
    )
endif()