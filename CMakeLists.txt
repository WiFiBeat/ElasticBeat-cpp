cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(elasticbeat-cpp
        VERSION 0.1
        DESCRIPTION "Header-only C++ library to interact with ElasticSearch"
        HOMEPAGE_URL "https://github.com/WiFiBeat/elasticbeat-cpp"
        LANGUAGES CXX)

add_library(elasticbeat-cpp INTERFACE)
target_include_directories(elasticbeat-cpp INTERFACE .)