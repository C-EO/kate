add_executable(lsptestapp "")
target_include_directories(lsptestapp PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/..)
target_link_libraries(lsptestapp PRIVATE kateprivate KF6::TextEditor)

target_sources(
  lsptestapp
  PRIVATE
    lsptestapp.cpp
    ../lspclientserver.cpp
    ../lspsemantichighlighting.cpp
    ../semantic_tokens_legend.cpp
    ${DEBUG_SOURCES}
)

if(ENABLE_PCH)
    target_precompile_headers(lsptestapp REUSE_FROM katepch_tests)
endif()
