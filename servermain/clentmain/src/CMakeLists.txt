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