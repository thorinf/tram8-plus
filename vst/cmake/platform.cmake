if(SMTG_MAC)
  target_link_libraries(tram8-bridge
    PRIVATE
      "-framework CoreMIDI"
      "-framework CoreFoundation"
      "-framework Cocoa"
      "-framework WebKit"
  )
  smtg_target_set_bundle(tram8-bridge
    BUNDLE_IDENTIFIER com.thorinf.tram8bridge
    COMPANY_NAME "thorinf"
  )
  smtg_target_add_plugin_resources(tram8-bridge
    RESOURCES
      "${CMAKE_CURRENT_SOURCE_DIR}/resource/ui.html"
  )
endif()
