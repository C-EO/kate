kate_add_plugin(lspclientplugin)
target_compile_definitions(lspclientplugin PRIVATE TRANSLATION_DOMAIN="lspclient")

target_link_libraries(
  lspclientplugin
  PRIVATE
    kateprivate
)

include(ECMQtDeclareLoggingCategory)
ecm_qt_declare_logging_category(
  DEBUG_SOURCES
  HEADER lspclient_debug.h
  IDENTIFIER LSPCLIENT
  CATEGORY_NAME "katelspclientplugin"
)
target_sources(lspclientplugin PRIVATE ${DEBUG_SOURCES})

ki18n_wrap_ui(lspclientplugin lspconfigwidget.ui)
target_sources(
  lspclientplugin
  PRIVATE
    lspclientcompletion.cpp
    lspclientconfigpage.cpp
    lspclienthover.cpp
    lspclientplugin.cpp
    lspclientpluginview.cpp
    lspclientserver.cpp
    lspclientservermanager.cpp
    lspclientsymbolhighlighter.cpp
    lspclientsymbolview.cpp
    lspclientutils.cpp
    lspsemantichighlighting.cpp
    semantic_tokens_legend.cpp
    gotosymboldialog.cpp
    inlayhints.cpp

    plugin.qrc
)

if (ENABLE_PCH)
    target_precompile_headers(lspclientplugin REUSE_FROM katepch)
endif()

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
