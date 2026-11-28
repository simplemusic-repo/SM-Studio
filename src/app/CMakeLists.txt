# The SM Studio application: UI, built-in modules and the extension API.

juce_add_gui_app(SMStudio
    PRODUCT_NAME "SM Studio"
    COMPANY_NAME "SimpleMusic"
    BUNDLE_ID "com.simplemusic.smstudio"
    VERSION ${PROJECT_VERSION}
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "SM Studio benötigt Zugriff auf Audioeingänge für Aufnahmen.")

target_sources(SMStudio PRIVATE
    Main.cpp
    SongHelpers.h
    SongHelpers.cpp
    extension/ExtensionApi.h
    extension/ExtensionRegistry.h
    extension/ExtensionRegistry.cpp
    modules/Modules.h
    modules/Modules.cpp
    modules/CoreModule.cpp
    modules/TransportModule.cpp
    modules/BrowserModule.cpp
    modules/PanelsModule.cpp
    modules/MidiModule.cpp
    modules/SettingsModule.cpp
    ui/Theme.h
    ui/Theme.cpp
    ui/Bars.h
    ui/Bars.cpp
    ui/ArrangeView.h
    ui/ArrangeView.cpp
    ui/Panels.h
    ui/Panels.cpp
    ui/MainComponent.h
    ui/MainComponent.cpp
    ui/PluginScanDialog.h
    ui/PluginScanDialog.cpp
    ui/PluginWindows.h
    ui/PluginWindows.cpp)

target_include_directories(SMStudio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(SMStudio PRIVATE
    SMS_VERSION_STRING="${PROJECT_VERSION}"
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:SMStudio,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:SMStudio,JUCE_VERSION>")

target_link_libraries(SMStudio PRIVATE sms_engine sms_core sms_juce)
