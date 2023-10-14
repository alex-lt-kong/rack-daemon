#ifndef InternalStateDto_hpp
#define InternalStateDto_hpp

#include "oatpp/core/Types.hpp"
#include "oatpp/core/macro/codegen.hpp"

#include OATPP_CODEGEN_BEGIN(DTO)

class InternalStateDto : public oatpp::DTO {

  DTO_INIT(InternalStateDto, DTO)
  DTO_FIELD(List<String>, int_sensor_paths, "internalSensorPaths");
  DTO_FIELD(List<String>, ext_sensor_paths, "externalSensorPaths");
  DTO_FIELD(List<Int32>, ext_temps, "externalTemperatures");
  DTO_FIELD(List<Int32>, int_temps, "internalTemperatures");
};

#include OATPP_CODEGEN_END(DTO)

#endif /* ModelInfoDto_hpp */
