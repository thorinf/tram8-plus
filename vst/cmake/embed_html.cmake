file(READ "${INPUT}" html_content)
file(WRITE "${OUTPUT}"
  "#pragma once\n\nstatic const char kUIHTML[] = R\"html(${html_content})html\";\n"
)
