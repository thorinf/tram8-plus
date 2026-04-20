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
endif()
