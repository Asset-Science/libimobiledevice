#ifndef _MDMAGENT_H
#define _MDMAGENT_H

#include "libimobiledevice/libimobiledevice.h"
#include "libimobiledevice/lockdown.h"
#include "libimobiledevice/property_list_service.h"

#define MDM_SERVICE_NAME "com.apple.mobile.MCInstall"

#define SERVICE_CONSTRUCTOR(x) (int32_t (*)(idevice_t, lockdownd_service_descriptor_t, void**))(x)

/** Error Codes */
typedef enum {
	MDMAGENT_E_SUCCESS = 0,
	MDMAGENT_E_INVALID_ARG = -1,
	MDMAGENT_E_PLIST_ERROR = -2,
	MDMAGENT_E_CONN_FAILED = -3,
	MDMAGENT_E_REQUEST_FAILED = -4,
	MDMAGENT_E_UNKNOWN_ERROR = -256
} mdmagent_error_t;


typedef struct mdmagent_client_private mdmagent_client_private;
typedef mdmagent_client_private *mdmagent_client_t; /**< The client handle. */

#ifdef OSX
#define LIBIMOBILEDEVICE_API_MSC
#endif

LIBIMOBILEDEVICE_API_MSC mdmagent_error_t mdmagent_client_new(idevice_t device, lockdownd_service_descriptor_t service, mdmagent_client_t *client);

LIBIMOBILEDEVICE_API_MSC mdmagent_error_t mdmagent_client_start_service(idevice_t device, mdmagent_client_t* client, const char* label);

LIBIMOBILEDEVICE_API_MSC mdmagent_error_t mdmagent_client_free(mdmagent_client_t client);

LIBIMOBILEDEVICE_API_MSC mdmagent_error_t mdmagent_send(mdmagent_client_t client, plist_t dict);

LIBIMOBILEDEVICE_API_MSC mdmagent_error_t mdmagent_sendAndCheckStatus(mdmagent_client_t client, plist_t dict);

LIBIMOBILEDEVICE_API_MSC mdmagent_error_t mdmagent_sendAndReturnValue(mdmagent_client_t client, plist_t dict, plist_t* qValue);

#endif // !_MDMAGENT_H
