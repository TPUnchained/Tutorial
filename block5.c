/** Licensed under AGPL 3.0. (C) 2010 David Moreno Montero. http://coralbits.com */
#include "onion/onion.h"
#include "onion/log.h"
#include <signal.h>
#include <netdb.h>
#include <stdio.h>
#include "favicon.h"
#include "cJSON.h"
#include <math.h>

#include "hash_datenbank.h" // our hash functions

onion *o=NULL;

int favicon(void *p, onion_request *req, onion_response *res){

	onion_response_set_header(res, "Server", "Super Rechnernetze Server T07G05 v0.1");
	onion_response_set_header(res, "Content-Type", "image/x-icon");

	if (onion_response_write_headers(res)==OR_SKIP_CONTENT) // Maybe it was HEAD.
		return OCS_PROCESSED;

	onion_response_write(res, (const char *)favicon_ico, favicon_ico_len);

	return OCS_PROCESSED;
}


int get_hash(void *p, onion_request *req, onion_response *res, struct entry *data){

	// data mining
	data = hash_get( (char*) onion_request_get_query(req, "1") );
	if(data == NULL) return 404; 

	return 200;
}


int put_hash(void *p, onion_request *req, onion_response *res){
	const char* query = onion_request_get_query(req, "1");
	const char* data  = onion_request_get_put(req, query);

	if((query == NULL) || (strcmp(query, "") == 0)) return 404;

	if(data == NULL) return 204;

	struct entry* ntry = (struct entry*)  calloc(1,sizeof(struct entry));
	ntry->key          = (unsigned char*) calloc(strlen(query),sizeof(char));
	ntry->key_length   = strlen(query);
	strncpy(ntry->key, query, ntry->key_length);
	
	ntry->value        = (unsigned char*) calloc(strlen(data),sizeof(char));
	ntry->value_length = strlen(data);
	strncpy(ntry->value, data, ntry->value_length);

	int check = hash_add(ntry);
	if(check == -1){
		hash_delete(ntry->key);
		check = hash_add(ntry);
	}

	printf("Added Value: %s\n\r", ntry->value);
	
	if(check == 0) return 201;
	else return 404;

}


int post_hash(void *p, onion_request *req, onion_response *res){
	const char* query = onion_request_get_query(req, "1");
	const char* data  = onion_request_get_put(req, query);

	if((query == NULL) || (strcmp(query, "") == 0)) return 404;
	if(data == NULL) return 204;

	struct entry* ntry = hash_get((char*) query);

	// Eintrag existiert noch nicht -> erzeuge einen Eintrag
	if(ntry == NULL){
		return put_hash(p, req, res);
	}
	// Eintrag existiert -> update
	else{
		cJSON* entry_value = cJSON_Parse(ntry->value);
		cJSON* req_value   = cJSON_Parse(data);

		printf("Send data %s\n", data);

		int check = 0;
		if(req_value == NULL){
			printf("Send data is not of type JSON.\n");
			check = -1;
		}

		if(entry_value == NULL){
			printf("Stored data is not of type JSON.\n");
			check = -1;
		}

		if(check == 0){
			if( cJSON_HasObjectItem(entry_value, req_value->child->string) ){
				char* tmp = calloc(50,sizeof(char));
				strcpy(tmp,req_value->child->string);
				cJSON_ReplaceItemInObject(entry_value, req_value->child->string, req_value->child); // hierbei geht der Name verloren...
				strcpy(req_value->child->string,tmp);
				free(tmp);
			}
				
			else{
				//cJSON_AddItemToObject(entry_value, req_value->child->string, req_value->child); // hierbei geht der Name verloren...
				cJSON* tmp = entry_value->child;
				while(tmp->next) tmp = tmp->next;
				tmp->next = req_value->child;
				req_value->child->prev = tmp;
				req_value->child = NULL;
			}

			if(ntry->value != NULL) free(ntry->value);
			ntry->value = cJSON_Print(entry_value); //parseValues(entry_value, req_value);
			cJSON_Minify(ntry->value);
			ntry->value_length = (ntry->value != NULL) ? strlen(ntry->value) : 0;

			cJSON_Delete(entry_value);
		}



		return ((check == 0) && (ntry->value != NULL)) ? 200 : 404;
	}
}


int delete_hash(void *p, onion_request *req, onion_response *res){

	int check = hash_delete( (char*) onion_request_get_query(req, "1") );
	if(check == -1) return 404;
	else return 200;	
}


int get_order(void *p, onion_request *req, onion_response *res){
	int check = 404;
	struct entry *data;
	onion_response_set_header(res, "Content-Type", "application/json");

	switch(onion_request_get_flags(req)&OR_METHODS){
		case OR_GET:
			// check = get_hash(p,req,res,data);

			data = hash_get( (char*) onion_request_get_query(req, "1") );
			check = (data == NULL) ? 404 : 200;

			onion_response_set_code(res, check);
			if(check == 200) onion_response_printf(res, "%s\n", (data->value != NULL) ? data->value : "<no Value>");
			else onion_response_printf(res, "GET failed. No Value with given key found!\n");
			
			break;

		case OR_PUT: 
			check = put_hash(p,req,res);

			onion_response_set_code(res, check);
			if(check == 201) onion_response_printf(res, "PUT succeeded: /%s\n", (char*) onion_request_get_query(req, "1") );
			else if(check == 204) onion_response_printf(res, "PUT failed. No content found!\n");
			else onion_response_printf(res, "PUT failed. No ID/key found!\n");
			break;

		case OR_POST: 
			check = post_hash(p,req,res);

			onion_response_set_code(res, check);
			if(check == 200 || check == 201) onion_response_printf(res, "POST succeeded: /%s\n", (char*) onion_request_get_query(req, "1") );
			else if(check == 204) onion_response_printf(res, "POST failed. No content found!\n");
			else onion_response_printf(res, "POST failed. Internal Conflict found!\n");
			break;

		case OR_DELETE: 
			check = delete_hash(p,req,res);

			onion_response_set_code(res, check);
			if(check == 200) onion_response_printf(res, "Value with key %s deleted.\n", onion_request_get_query(req, "1"));
			else onion_response_printf(res, "Deleting failed. No Value with given key found!\n"); 
			break;

		default: return wrong_request(p, req, res);
	}
	
	return OCS_PROCESSED;
	/*
	onion_response_write0(res,"Hello world");
	if (onion_request_get_query(req, "1")){
		onion_response_printf(res, "<p>Path: %s", onion_request_get_query(req, "1"));
	}
	onion_response_printf(res,"<p>Client description: %s",onion_request_get_client_description(req));
	onion_response_set_header(res, "Server", "Super Rechnernetze Server TxxGyy v0.1");
	return OCS_PROCESSED;
	*/
}


int wrong_request(void *p, onion_request *req, onion_response *res){
	
	onion_response_set_header(res, "Content-Type", "application/json");
	onion_response_set_code(res, 404);
	onion_response_printf(res, "No Key found! Usage: <server>:<port>/<key>\n");
	return OCS_PROCESSED;
}


static void shutdown_server(int _){
	if (o)
		onion_listen_stop(o);
}

int main(int argc, char **argv){

	hash_init();

	signal(SIGINT,shutdown_server);
	signal(SIGTERM,shutdown_server);

	o=onion_new(O_POOL);
	onion_set_timeout(o, 5000);
	onion_set_hostname(o,"127.0.0.1");
	onion_set_port(o, "8080");
	onion_url *urls=onion_root_url(o);

	onion_url_add(urls, "favicon.ico", favicon);
	onion_url_add(urls, "", wrong_request);
	onion_url_add(urls, "^(.*)$", get_order);

	onion_listen(o);
	onion_free(o);
	return 0;
}
