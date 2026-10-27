# flashtest - on-board test of the dmflash block device node.
set(DMOD_DMR_PATH ${CMAKE_CURRENT_SOURCE_DIR}/flashtest.dmr)

dmod_add_executable(flashtest ${DMOD_MODULE_VERSION}
    main.c
)

# dmdrvi: headers only (block ioctls, DMDRVI_IOCTL_CUSTOM_BASE)
dmod_link_modules(flashtest
    dmdrvi@>=2.0
)

target_include_directories(flashtest PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
