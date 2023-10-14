#ifdef __cplusplus
#define CEXTERN extern "C"
#else
#define CEXTERN
#endif
CEXTERN void initialize_http_service(const char *host, const int port,
                                     const char *advertised_host);

CEXTERN void finalize_http_service();
