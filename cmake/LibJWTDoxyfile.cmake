# Doxygen config. Only things that differ from the default

set(DOXYGEN_PROJECT_LOGO "images/LibJWT.svg height=100")
set(DOXYGEN_PROJECT_ICON "images/favicon.ico")
set(DOXYGEN_STRIP_FROM_PATH ${CMAKE_SOURCE_DIR})
set(DOXYGEN_OPTIMIZE_OUTPUT_FOR_C "YES")
set(DOXYGEN_TYPEDEF_HIDES_STRUCT "YES")
set(DOXYGEN_EXTRACT_ALL "YES")
set(DOXYGEN_SHOW_HEADERFILE "NO")
set(DOXYGEN_SHOW_INCLUDE_FILES "NO")
set(DOXYGEN_SHOW_USED_FILES "NO")
set(DOXYGEN_SHOW_FILES "NO")
set(DOXYGEN_SHOW_NAMESPACES "NO")
set(DOXYGEN_QUIET "YES")
set(DOXYGEN_WARN_NO_PARAMDOC "YES")
set(DOXYGEN_WARN_IF_UNDOC_ENUM_VAL "YES")
set(DOXYGEN_EXAMPLE_PATH "doxygen")
set(DOXYGEN_EXAMPLE_PATTERNS "*.dox *.c")
set(DOXYGEN_IMAGE_PATH "images")
set(DOXYGEN_VERBATIM_HEADERS "NO")
set(DOXYGEN_HTML_HEADER "doxygen/header.html")
set(DOXYGEN_HTML_FOOTER "doxygen/footer.html")
set(DOXYGEN_HTML_EXTRA_STYLESHEET "doxygen/doxygen-awesome.css \\\n\tdoxygen/libjwt.css")
set(DOXYGEN_HTML_COLORSTYLE "LIGHT")
set(DOXYGEN_HTML_COPY_CLIPBOARD "NO")
set(DOXYGEN_GENERATE_DOCSET "YES")
set(DOXYGEN_DOCSET_FEEDNAME "Docs provided by maClara, LLC")
set(DOXYGEN_DOCSET_BUNDLE_ID "com.maclara-llc.LibJWT")
set(DOXYGEN_DOCSET_PUBLISHER_ID "com.maclara-llc.DocPublish")
set(DOXYGEN_DOCSET_PUBLISHER_NAME "maClara, LLC")
set(DOXYGEN_GENERATE_QHP "YES")
set(DOXYGEN_QCH_FILE "LibJWT.qch")
set(DOXYGEN_QHP_NAMESPACE "com.maclara-llc.LibJWT")
set(DOXYGEN_DISABLE_INDEX "YES")
set(DOXYGEN_GENERATE_TREEVIEW "YES")
set(DOXYGEN_GENERATE_LATEX "YES")
set(DOXYGEN_PAPER_TYPE "letter")
set(DOXYGEN_LATEX_BATCHMODE "YES")
set(DOXYGEN_GENERATE_MAN "YES")
set(DOXYGEN_MAN_LINKS "YES")
set(DOXYGEN_GENERATE_DOCBOOK "YES")
set(DOXYGEN_MACRO_EXPANSION "YES")
set(DOXYGEN_EXPAND_ONLY_PREDEF "YES")
set(DOXYGEN_PREDEFINED "_DOXYGEN JWT_EXPORT= JWT_NO_EXPORT= JWT_DEPRECATED=")
string(APPEND DOXYGEN_PREDEFINED " \\\n\tJWT_DEPRECATED_EXPORT= JWT_DEPRECATED_NO_EXPORT=")
string(APPEND DOXYGEN_PREDEFINED " \\\n\tJWT_CONSTRUCTOR= __attribute__(x)= __declspec(x)=")
set(DOXYGEN_GENERATE_TAGFILE "${DOXYGEN_OUTPUT_DIRECTORY}/LibJWT.tag")
set(DOXYGEN_DOT_IMAGE_FORMAT "svg")
set(DOXYGEN_INTERACTIVE_SVG "YES")
set(DOXYGEN_SEARCHENGINE "NO")

# List of extra files we need for a nice theme
set(DOXYGEN_HTML_EXTRA_FILES "doxygen/doxygen-awesome-paragraph-link.js")
string(APPEND DOXYGEN_HTML_EXTRA_FILES " \\\n\tdoxygen/doxygen-awesome-fragment-copy-button.js")
string(APPEND DOXYGEN_HTML_EXTRA_FILES " \\\n\tdoxygen/doxygen-awesome-interactive-toc.js")

# Base templates for ALIASES
set(rfc_url "https://datatracker.ietf.org/doc/html/rfc")
set(fa_i "<i class=\\\"fa-regular fa-file-lines\\\"></i>")
set(a_pre "<a target=\\\"_blank\\\" href=\\\"${rfc_url}")
set(a_pre_b "<a type=\\\"button\\\" class=\\\"button\\\" target=\\\"_blank\\\" href=\\\"${rfc_url}")

# Used for easily linking to RFC and RFC sections
set(DOXYGEN_ALIASES "rfc{1}=\"${a_pre}\\1\\\"> ${fa_i} RFC-\\1</a>\"")
string(APPEND DOXYGEN_ALIASES " \\\n\trfc{2}=\"${a_pre_b}\\1#section-\\2\\\"> ${fa_i} RFC-\\1 Sec \\2</a>\"")
string(APPEND DOXYGEN_ALIASES " \\\n\trfc_t{2}=\"${a_pre}\\1#section-\\2\\\"> ${fa_i} RFC-\\1 Sec \\2</a>\"")
string(APPEND DOXYGEN_ALIASES " \\\n\tfa{1}=\"<i class=\\\"fa-regular fa-\\1\\\"></i>\"")
string(APPEND DOXYGEN_ALIASES " \\\n\tfa{2}=\"<i class=\\\"fa-\\1 fa-\\2\\\"></i>\"")

set(DOXYGEN_VERBATIM_VARS DOXYGEN_EXAMPLE_PATTERNS DOXYGEN_PREDEFINED
	DOXYGEN_HTML_EXTRA_STYLESHEET DOXYGEN_HTML_EXTRA_FILES
	DOXYGEN_ALIASES)
