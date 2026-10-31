function(add_mempass_test name)
    qt_add_executable(${name}
        ${name}.cpp
    )
    target_link_libraries(${name}
        PRIVATE
            mempass_client_core
            Qt6::Test
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_mempass_test(common_test)
add_mempass_test(crypto_vault_test)

qt_add_executable(sync_integration_test
    sync_integration_test.cpp
)
target_link_libraries(sync_integration_test
    PRIVATE
        mempass_client_core
        Qt6::Test
)
target_compile_definitions(sync_integration_test
    PRIVATE
        MEMPASS_SERVER_EXE="$<TARGET_FILE:mempass_server>"
)
add_dependencies(sync_integration_test mempass_server)
add_test(NAME sync_integration_test COMMAND sync_integration_test)
