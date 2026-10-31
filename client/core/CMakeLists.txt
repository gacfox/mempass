find_package(Qt6 REQUIRED COMPONENTS Network)
find_package(unofficial-sodium CONFIG REQUIRED)

qt_add_library(mempass_client_core STATIC
    include/mempass/crypto.h
    include/mempass/vault.h
    include/mempass/sync_service.h
    src/crypto.cpp
    src/vault.cpp
    src/sync_service.cpp
)

target_include_directories(mempass_client_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(mempass_client_core
    PUBLIC
        mempass_common
        Qt6::Core
        Qt6::Network
        unofficial-sodium::sodium
)

if(WIN32)
    target_compile_definitions(mempass_client_core PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
