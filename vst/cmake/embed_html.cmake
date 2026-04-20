get_filename_component(output_dir "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${output_dir}")

file(READ "${INPUT}" html_content)
file(WRITE "${OUTPUT}"
  "#pragma once\n\nstatic const char kUIHTML[] = R\"tram8_html(${html_content})tram8_html\";\n"
)
