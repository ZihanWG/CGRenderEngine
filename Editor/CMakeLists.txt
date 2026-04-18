add_library(CGEngineEditor INTERFACE)
add_library(CGEngine::Editor ALIAS CGEngineEditor)
target_link_libraries(CGEngineEditor
    INTERFACE
        Engine
)
