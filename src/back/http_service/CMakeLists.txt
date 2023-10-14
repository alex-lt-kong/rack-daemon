add_library(
    http
    controller/StaticController.hpp
    controller/SwaggerController.hpp
    AppComponent.hpp
    SwaggerComponent.hpp
    ErrorHandler.cpp
    ErrorHandler.hpp
)

target_include_directories(http PUBLIC swagger)

## link libs
find_package(oatpp          1.3.0 REQUIRED)
find_package(oatpp-swagger  1.3.0 REQUIRED)

target_link_libraries(http
    # Oat++
    PUBLIC oatpp::oatpp
    PUBLIC oatpp::oatpp-swagger
)

add_definitions(
    ## define path to swagger-ui static resources folder
    -DOATPP_SWAGGER_RES_PATH="${oatpp-swagger_INCLUDE_DIRS}/../bin/oatpp-swagger/res"
)

if(CMAKE_SYSTEM_NAME MATCHES Linux)
    find_package(Threads REQUIRED)
    target_link_libraries(http INTERFACE Threads::Threads ${CMAKE_DL_LIBS})
endif()

add_library(httpService HttpService.cpp)
target_link_libraries(httpService http)
