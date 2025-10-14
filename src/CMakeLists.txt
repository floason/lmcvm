add_executable(lmcvm main.c)
target_link_libraries(lmcvm PUBLIC lmcvm_interface)

install(TARGETS lmcvm DESTINATION bin)