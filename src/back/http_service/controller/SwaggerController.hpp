#ifndef SwaggerController_hpp
#define SwaggerController_hpp

#include <math.h>
#include <mutex>
#include <regex>

#include "../../utils.h"
#include "../dto/InternalStateDto.hpp"
#include "../dto/RespDto.hpp"
#include <oatpp/core/macro/codegen.hpp>
#include <oatpp/core/macro/component.hpp>
#include <oatpp/core/utils/ConversionUtils.hpp>
#include <oatpp/parser/json/mapping/ObjectMapper.hpp>
#include <oatpp/web/protocol/http/Http.hpp>
#include <oatpp/web/server/api/ApiController.hpp>

#include OATPP_CODEGEN_BEGIN(ApiController) //<- Begin Codegen

class SwaggerController : public oatpp::web::server::api::ApiController {
public:
  SwaggerController(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>,
                                    objectMapper))
      : oatpp::web::server::api::ApiController(objectMapper) {}

public:
  static std::shared_ptr<SwaggerController> createShared(OATPP_COMPONENT(
      std::shared_ptr<ObjectMapper>,
      objectMapper) // Inject objectMapper component here as default parameter
  ) {
    return std::make_shared<SwaggerController>(objectMapper);
  }

  ENDPOINT_INFO(getInternalState) {
    info->summary = "get internal state of rack daemon";
    info->addResponse<Object<InternalStateDto>>(Status::CODE_200,
                                                "application/json");
    info->addResponse<Object<RespDto>>(Status::CODE_404, "application/json");
    info->addResponse<Object<RespDto>>(Status::CODE_500, "application/json");
  }
  ENDPOINT("GET", "getInternalState/", getInternalState) {
    std::vector<std::string> _model_ids;
    auto resp = InternalStateDto::createShared();
    resp->int_sensor_paths = List<String>::createShared();
    resp->ext_sensor_paths = List<String>::createShared();
    resp->int_temps = List<Int32>::createShared();
    resp->ext_temps = List<Int32>::createShared();
    for (size_t i = 0; i < pl.num_int_sensors; ++i) {
      resp->int_sensor_paths->push_back(pl.int_sensor_paths[i]);
      resp->int_temps->push_back(pl.int_temps[i]);
    }
    for (size_t i = 0; i < pl.num_ext_sensors; ++i) {
      resp->ext_sensor_paths->push_back(pl.ext_sensor_paths[i]);
      resp->ext_temps->push_back(pl.ext_temps[i]);
    }
    return createDtoResponse(Status::CODE_200, resp);
  }
};

#include OATPP_CODEGEN_END(ApiController) //<- End Codegen

#endif /* SwaggerController_hpp */
