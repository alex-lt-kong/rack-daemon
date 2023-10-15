#ifndef HTTP_SERVICE_H
#define HTTP_SERVICE_H

#include <cjson/cJSON.h>
#include <microhttpd.h>

struct MHD_Daemon *init_mhd(const cJSON *RACK_DAEMON);

#endif /* HTTP_SERVICE_H */
