if(WIN32)
    add_executable(RelionSofta WIN32
        Application.cpp
        ChatView.cpp
        Client.cpp
        ClientSession.cpp
        FileTransferManager.cpp
        JoinRoomView.cpp
        main.cpp
        MainMenuView.cpp
        Renderer.cpp
        UIManager.cpp
        Window.cpp
    )
else()
    add_executable(RelionSofta
        Application.cpp
        ChatView.cpp
        Client.cpp
        ClientSession.cpp
        FileTransferManager.cpp
        JoinRoomView.cpp
        main.cpp
        MainMenuView.cpp
        Renderer.cpp
        UIManager.cpp
        Window.cpp
    )
endif()

target_include_directories(RelionSofta PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(RelionSofta PRIVATE
    imgui
    asio
    SDL3::SDL3-static
    opengl32
    glad
    nfd
)