#ifndef __MDMAGENT_H
#define __MDMAGENT_H

#ifdef OSX
#include "mdmagent.h"
#else
#include <libimobiledevice/mdmagent.h>
#endif
#include "property_list_service.h"

struct mdmagent_client_private {
	property_list_service_client_t parent;
	int last_error;
};

#endif
