add_executable(BespokeAudioBridgeTests
    StreamingAudioResamplerTest.cpp
    ../Source/StreamingAudioResampler.cpp)
target_include_directories(BespokeAudioBridgeTests PRIVATE ../Source)
target_link_libraries(BespokeAudioBridgeTests PRIVATE SampleRate::samplerate)
add_test(NAME BespokeAudioBridgeTests COMMAND BespokeAudioBridgeTests)

add_executable(ArrangerSnapshotSelectionTests ArrangerSnapshotSelectionTest.cpp)
target_include_directories(ArrangerSnapshotSelectionTests PRIVATE ../Source)
add_test(NAME ArrangerSnapshotSelectionTests COMMAND ArrangerSnapshotSelectionTests)

add_executable(ArrangerClipTimingTests ArrangerClipTimingTest.cpp)
target_include_directories(ArrangerClipTimingTests PRIVATE ../Source)
add_test(NAME ArrangerClipTimingTests COMMAND ArrangerClipTimingTests)
