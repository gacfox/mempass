find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(mempass_client
    main.cpp
    mainwindow.h
    mainwindow.cpp
    MainWindow.ui
    categorydialog.h
    categorydialog.cpp
    CategoryDialog.ui
    accountdialog.h
    accountdialog.cpp
    AccountDialog.ui
    passwordgeneratordialog.h
    passwordgeneratordialog.cpp
    PasswordGeneratorDialog.ui
    changepassworddialog.h
    changepassworddialog.cpp
    ChangePasswordDialog.ui
    syncsettingsdialog.h
    syncsettingsdialog.cpp
    SyncSettingsDialog.ui
    createvaultdialog.h
    createvaultdialog.cpp
    CreateVaultDialog.ui
    unlockdialog.h
    unlockdialog.cpp
    UnlockDialog.ui
    aboutdialog.h
    aboutdialog.cpp
    AboutDialog.ui
    app.rc
    resources.qrc
)

target_link_libraries(mempass_client
    PRIVATE
        mempass_client_core
        Qt6::Widgets
)

target_include_directories(mempass_client
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(mempass_client PROPERTIES
    OUTPUT_NAME mempass
    WIN32_EXECUTABLE TRUE
)

install(TARGETS mempass_client
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
