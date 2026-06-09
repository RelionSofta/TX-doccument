add_executable(server 
    main.cpp
    Server.cpp
    Session.cpp
    Room.cpp
    RoomHandler.cpp
    Message.cpp
    FileTransfer.cpp
    protocol.cpp

)

target_include_directories(server PRIVATE 
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/external/asio/asio1342/include
)

#target_link_libraries(server PRIVATE pthread)