juce_add_console_app(SMStudioTests PRODUCT_NAME "SMStudioTests")

target_sources(SMStudioTests PRIVATE
    TestMain.cpp
    CoreTests.cpp
    EngineTests.cpp)

target_link_libraries(SMStudioTests PRIVATE sms_engine sms_core sms_juce)

add_test(NAME core_tests COMMAND SMStudioTests)
