find_package(Qt6 REQUIRED COMPONENTS HttpServer)

qt_add_executable(mempass_server
    src/config.h
    src/config.cpp
    src/database.h
    src/database.cpp
    src/sync_server.h
    src/sync_server.cpp
    src/main.cpp
)

target_link_libraries(mempass_server
    PRIVATE
        mempass_common
        Qt6::Core
        Qt6::Network
        Qt6::HttpServer
        Qt6::Sql
)

install(TARGETS mempass_server
    RUNTIME DESTINATION bin
)
