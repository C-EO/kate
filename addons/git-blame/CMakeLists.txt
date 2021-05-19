kcoreaddons_add_plugin(kategitblameplugin INSTALL_NAMESPACE "ktexteditor")
target_compile_definitions(kategitblameplugin PRIVATE TRANSLATION_DOMAIN="kategitblameplugin")
target_link_libraries(kategitblameplugin PRIVATE KF5::I18n KF5::TextEditor)

target_sources(
  kategitblameplugin
  PRIVATE
    kategitblameplugin.cpp
    gitblametooltip.cpp
    plugin.qrc
)

kcoreaddons_desktop_to_json(kategitblameplugin kategitblameplugin.desktop)
