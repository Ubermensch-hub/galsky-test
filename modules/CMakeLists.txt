set(TRACKER_MODULES
    gps
    can_bus
    script_interpreter
    geofence_engine
    archive
    uplink
    sensors
    power_mgmt
    config_store
    diagnostics
)

foreach(tracker_module ${TRACKER_MODULES})
    add_subdirectory(${tracker_module})
endforeach()

# Aggregate target so app/ and tests/ can depend on "all modules" without listing each one.
add_library(modules INTERFACE)
target_link_libraries(modules INTERFACE ${TRACKER_MODULES})
