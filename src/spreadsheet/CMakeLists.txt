
add_library(liborcus-spreadsheet-model-${ORCUS_API_VERSION} SHARED
    auto_filter.cpp
	config.cpp
	csv_dumper.cpp
	document.cpp
	dumper_global.cpp
	factory.cpp
	factory_pivot.cpp
	factory_sheet.cpp
	factory_styles.cpp
	factory_table.cpp
	flat_dumper.cpp
	formula_global.cpp
	global_settings.cpp
	html_dumper.cpp
	json_dumper.cpp
    number_format.cpp
	pivot.cpp
	shared_formula.cpp
	shared_strings.cpp
	sheet.cpp
	sheet_range.cpp
	styles.cpp
	view.cpp
)

target_link_libraries(liborcus-spreadsheet-model-${ORCUS_API_VERSION} liborcus-parser-${ORCUS_API_VERSION} liborcus-${ORCUS_API_VERSION} ${IXION_LIB})
target_compile_definitions(liborcus-spreadsheet-model-${ORCUS_API_VERSION} PRIVATE __ORCUS_SPM_BUILDING_DLL)

install(
    TARGETS
        liborcus-spreadsheet-model-${ORCUS_API_VERSION}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
