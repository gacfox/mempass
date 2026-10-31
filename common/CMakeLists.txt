qt_add_library(mempass_common STATIC
    include/mempass/models.h
    include/mempass/sync_protocol.h
    src/models.cpp
    src/sync_protocol.cpp
)

target_include_directories(mempass_common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(mempass_common
    PUBLIC
        Qt6::Core
)

target_compile_definitions(mempass_common
    PRIVATE
        MEMPASS_COMMON_LIBRARY
)
