
add_executable(${PROJECT_NAME}
	main.cpp
)
target_link_libraries(${PROJECT_NAME}
	${LIBUSB_LIBRARY}
)

if(APPLE)
	target_link_libraries(${PROJECT_NAME} "-framework CoreFoundation" "-framework IOKit")
	set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-Wl,-F/Library/Frameworks")
endif()
