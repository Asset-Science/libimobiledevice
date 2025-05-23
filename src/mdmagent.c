#include <string.h>
#include <stdlib.h>
#include <plist/plist.h>
#include <stdio.h>

#include "mdmagent.h"

static mdmagent_error_t mdmagent_error(property_list_service_error_t err)
{
	switch (err) {
	case PROPERTY_LIST_SERVICE_E_SUCCESS:
		return MDMAGENT_E_SUCCESS;
	case PROPERTY_LIST_SERVICE_E_INVALID_ARG:
		return MDMAGENT_E_INVALID_ARG;
	case PROPERTY_LIST_SERVICE_E_PLIST_ERROR:
		return MDMAGENT_E_PLIST_ERROR;
	case PROPERTY_LIST_SERVICE_E_MUX_ERROR:
		return MDMAGENT_E_CONN_FAILED;
	default:
		break;
	}
	return MDMAGENT_E_UNKNOWN_ERROR;
}


LIBIMOBILEDEVICE_API_MSC mdmagent_error_t mdmagent_client_new(idevice_t device, lockdownd_service_descriptor_t service, mdmagent_client_t *client)
{
	property_list_service_client_t plistclient = NULL;
	mdmagent_error_t err = mdmagent_error(property_list_service_client_new(device, service, &plistclient));
	if (err != MDMAGENT_E_SUCCESS) {
		return err;
	}

	mdmagent_client_t client_loc = (mdmagent_client_t)malloc(sizeof(struct mdmagent_client_private));
	client_loc->parent = plistclient;
	client_loc->last_error = 0;

	*client = client_loc;

	return MDMAGENT_E_SUCCESS;
}

LIBIMOBILEDEVICE_API_MSC mdmagent_error_t mdmagent_client_start_service(idevice_t device, mdmagent_client_t* client, const char* label)
{
	mdmagent_error_t err = MDMAGENT_E_UNKNOWN_ERROR;
	service_client_factory_start_service(device, MDM_SERVICE_NAME, (void**)client, label, SERVICE_CONSTRUCTOR(mdmagent_client_new), &err);
	return err;
}

LIBIMOBILEDEVICE_API_MSC mdmagent_error_t mdmagent_client_free(mdmagent_client_t client)
{
	if (!client)
		return MDMAGENT_E_INVALID_ARG;

	mdmagent_error_t err = MDMAGENT_E_SUCCESS;
	if (client->parent && client->parent->parent) {
		mdmagent_error(property_list_service_client_free(client->parent));
	}
	client->parent = NULL;
	free(client);

	return err;
}

LIBIMOBILEDEVICE_API_MSC mdmagent_error_t mdmagent_send(mdmagent_client_t client, plist_t dict)
{
	mdmagent_error_t res = mdmagent_error(property_list_service_send_xml_plist(client->parent, dict));
	if (res != MDMAGENT_E_SUCCESS) {
		printf("could not send plist, error %d", res);
	}
	return res;
}

LIBIMOBILEDEVICE_API_MSC mdmagent_error_t mdmagent_sendAndCheckStatus(mdmagent_client_t client, plist_t dict)
{	
	mdmagent_error_t res = mdmagent_error(property_list_service_send_binary_plist(client->parent, dict));
	if (res != MDMAGENT_E_SUCCESS) {
		printf("could not send plist, error %d", res);
		return res;
	}

	plist_free(dict);
	dict = NULL;
	res = mdmagent_error(property_list_service_receive_plist(client->parent, &dict));
	if (res != MDMAGENT_E_SUCCESS) {
		printf("could not receive response, error %d", res);
		return res;
	}
	if (!dict) {
		printf("could not get response plist");
		return MDMAGENT_E_UNKNOWN_ERROR;
	}

	if (plist_get_node_type(dict) != PLIST_DICT) {
		return MDMAGENT_E_PLIST_ERROR;
	}

	plist_t node = plist_dict_get_item(dict, "Status");
	if (!node || (plist_get_node_type(node) != PLIST_STRING)) {
		return MDMAGENT_E_PLIST_ERROR;
	}

	char *result = NULL;
	plist_get_string_val(node, &result);
	if (strcmp(result, "Acknowledged") != 0) {
		printf("mdmagent_sendAndCheckStatus failed: %s", result);
		res = MDMAGENT_E_REQUEST_FAILED;
	}

	if (result)
		free(result);

	plist_free(dict);

	return res;
}

LIBIMOBILEDEVICE_API_MSC mdmagent_error_t mdmagent_sendAndReturnValue(mdmagent_client_t client, plist_t dict, plist_t* qValue)
{
	mdmagent_error_t res = mdmagent_error(property_list_service_send_binary_plist(client->parent, dict));
	if (res != MDMAGENT_E_SUCCESS) {
		printf("could not send plist, error %d", res);
		return res;
	}

	plist_free(dict);
	dict = NULL;
	res = mdmagent_error(property_list_service_receive_plist(client->parent, qValue));
	if (res != MDMAGENT_E_SUCCESS) {
		printf("could not receive response, error %d", res);
		return res;
	}
	if (!*qValue) {
		printf("could not get response plist");
		return MDMAGENT_E_UNKNOWN_ERROR;
	}

	if (plist_get_node_type(*qValue) != PLIST_DICT) {
		return MDMAGENT_E_PLIST_ERROR;
	}

	plist_t node = plist_dict_get_item(*qValue, "Status");
	if (!node || (plist_get_node_type(node) != PLIST_STRING)) {
		return MDMAGENT_E_PLIST_ERROR;
	}

	char* result = NULL;
	plist_get_string_val(node, &result);
	if (strcmp(result, "Acknowledged") != 0) {
		printf("mdmagent_sendAndReturnValue failed: %s", result);
		res = MDMAGENT_E_REQUEST_FAILED;
	}

	if (result)
		free(result);

	return res;
}
