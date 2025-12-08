# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(SRCS ${app_sources} INCLUDE_DIRS "." 
                        EMBED_TXTFILES certs/ca.crt certs/server.key certs/server.crt certs/server_https.crt certs/server_https.key certs/client.crt certs/client.key
                        PRIV_REQUIRES esp_http_server)