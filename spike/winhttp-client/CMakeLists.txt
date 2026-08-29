# Builds the WinHTTP transport against opentelemetry-cpp with the curl backend switched OFF.
#
# The point of the spike: WITH_HTTP_CLIENT_CURL=OFF is an upstream-supported configuration ("set OFF
# to supply a custom transport"), so if this links and the smoke test passes, the SDK is usable in
# a process that cannot take curl - which is what blocked it inside the game client, where curl's
# zlib collides with the zlib already inside d3dx8.lib.
cmake_minimum_required(VERSION 3.16)
project(otel_winhttp_spike CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(opentelemetry-cpp CONFIG REQUIRED)

add_library(winhttp_client STATIC winhttp_client.cpp)
target_include_directories(winhttp_client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# Deliberately no http_client_curl: that target does not exist in this configuration, which is the
# whole point of the exercise.
target_link_libraries(winhttp_client PUBLIC opentelemetry-cpp::api opentelemetry-cpp::sdk winhttp)

add_executable(smoke_test smoke_test.cpp)
# --periodic mode drives a real MeterProvider, so the metric exporter comes along too.
target_link_libraries(smoke_test PRIVATE winhttp_client opentelemetry-cpp::otlp_http_client
                      opentelemetry-cpp::otlp_http_metric_exporter opentelemetry-cpp::metrics winhttp)
