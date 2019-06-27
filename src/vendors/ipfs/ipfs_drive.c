#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>

#include <crystal.h>
#include <cjson/cJSON.h>

#include "ipfs_drive.h"
#include "ipfs_file.h"
#include "ipfs_token.h"
#include "ipfs_constants.h"
#include "ipfs_utils.h"
#include "http_client.h"
#include "hive_error.h"
#include "hive_client.h"

typedef struct IPFSDrive {
    HiveDrive base;
    ipfs_token_t *token;
} IPFSDrive;

static int ipfs_drive_get_info(HiveDrive *base, HiveDriveInfo *info)
{
    int rc;

    assert(base);
    assert(info);

    (void)base;

    // TODO:
    rc = snprintf(info->driveid, sizeof(info->driveid), "");
    if (rc < 0 || rc >= sizeof(info->driveid))
        return HIVE_GENERAL_ERROR(HIVEERR_BUFFER_TOO_SMALL);

    return 0;
}

static int ipfs_drive_stat_file(HiveDrive *base, const char *path,
                                HiveFileInfo *info)
{
    IPFSDrive *drive = (IPFSDrive *)base;
    char buf[MAX_URL_LEN] = {0};
    http_client_t *httpc;
    long resp_code = 0;
    cJSON *json;
    cJSON *item;
    char *p;
    int rc;

    assert(drive->token);
    assert(path);
    assert(info);

    rc = ipfs_token_check_reachable(drive->token);
    if (rc < 0)
        return rc;

    sprintf(buf, "http://%s:%d/api/v0/files/stat",
            ipfs_token_get_current_node(drive->token), NODE_API_PORT);

    httpc = http_client_new();
    if (!httpc)
        return HIVE_GENERAL_ERROR(HIVEERR_OUT_OF_MEMORY);

    http_client_set_url(httpc, buf);
    http_client_set_query(httpc, "uid", ipfs_token_get_uid(drive->token));
    http_client_set_query(httpc, "path", path);
    http_client_set_method(httpc, HTTP_METHOD_POST);
    http_client_set_request_body_instant(httpc, NULL, 0);
    http_client_enable_response_body(httpc);

    rc = http_client_request(httpc);
    if (rc) {
        rc = HIVE_HTTPC_ERROR(rc);
        if (RC_NODE_UNREACHABLE(rc)) {
            rc = HIVE_GENERAL_ERROR(HIVEERR_TRY_AGAIN);
            ipfs_token_mark_node_unreachable(drive->token);
        }
        goto error_exit;
    }

    rc = http_client_get_response_code(httpc, &resp_code);
    if (rc) {
        rc = HIVE_HTTPC_ERROR(rc);
        goto error_exit;
    }

    if (resp_code != 200) {
        rc = HIVE_HTTP_STATUS_ERROR(resp_code);
        goto error_exit;
    }

    p = http_client_move_response_body(httpc, NULL);
    http_client_close(httpc);

    if (!p)
        return HIVE_GENERAL_ERROR(HIVEERR_OUT_OF_MEMORY);

    json = cJSON_Parse(p);
    free(p);

    if (!json)
        return HIVE_GENERAL_ERROR(HIVEERR_OUT_OF_MEMORY);

    item = cJSON_GetObjectItem(json, "Hash");
    if (!item ||
        !cJSON_IsString(item) ||
        !item->valuestring ||
        !*item->valuestring ||
        strlen(item->valuestring) >= sizeof(info->fileid)) {
        cJSON_Delete(json);
        return HIVE_GENERAL_ERROR(HIVEERR_BAD_JSON_FORMAT);
    }

    rc = snprintf(info->fileid, sizeof(info->fileid),
                  "/ipfs/%s", item->valuestring);
    if (rc < 0 || rc >= sizeof(info->fileid)) {
        cJSON_Delete(json);
        return HIVE_GENERAL_ERROR(HIVEERR_BUFFER_TOO_SMALL);
    }

    item = cJSON_GetObjectItem(json, "Type");
    if (!item || !cJSON_IsString(item) || !item->valuestring || !*item->valuestring ||
        (strcmp(item->valuestring, "file") && strcmp(item->valuestring, "directory"))) {
        cJSON_Delete(json);
        return HIVE_GENERAL_ERROR(HIVEERR_BAD_JSON_FORMAT);
    }

    strcpy(info->type, item->valuestring);

    item = cJSON_GetObjectItem(json, "Size");
    if (!item || !cJSON_IsNumber(item)) {
        cJSON_Delete(json);
        return HIVE_GENERAL_ERROR(HIVEERR_BAD_JSON_FORMAT);
    }

    info->size = (size_t)item->valuedouble;
    cJSON_Delete(json);

    return 0;

error_exit:
    http_client_close(httpc);
    return rc;
}

static cJSON *parse_list_files_response(const char *response)
{
    cJSON *json;
    cJSON *entries;
    cJSON *entry;

    assert(response);

    json = cJSON_Parse(response);
    if (!json) {
        hive_set_error(HIVE_GENERAL_ERROR(HIVEERR_OUT_OF_MEMORY));
        return NULL;
    }

    entries = cJSON_GetObjectItemCaseSensitive(json, "Entries");
    if (!entries || !cJSON_IsArray(entries)) {
        hive_set_error(HIVE_GENERAL_ERROR(HIVEERR_BAD_JSON_FORMAT));
        cJSON_Delete(json);
        return NULL;
    }

    cJSON_ArrayForEach(entry, entries) {
        cJSON *name;

        if (!cJSON_IsObject(entry)) {
            hive_set_error(HIVE_GENERAL_ERROR(HIVEERR_BAD_JSON_FORMAT));
            cJSON_Delete(json);
            return NULL;
        }

        name = cJSON_GetObjectItemCaseSensitive(entry, "Name");
        if (!name || !cJSON_IsString(name) || !name->valuestring ||
            !*name->valuestring) {
            hive_set_error(HIVE_GENERAL_ERROR(HIVEERR_BAD_JSON_FORMAT));
            cJSON_Delete(json);
            return NULL;
        }
    }

    return json;
}

static void notify_file_entries(cJSON *entries,
                                HiveFilesIterateCallback *callback,
                                void *context)
{
    cJSON *entry;

    assert(entries);
    assert(callback);

    cJSON_ArrayForEach(entry, entries) {
        cJSON *name;
        KeyValue properties[1];
        bool resume;

        name = cJSON_GetObjectItemCaseSensitive(entry, "Name");

        properties[0].key   = "name";
        properties[0].value = name->valuestring;

        resume = callback(properties, sizeof(properties) / sizeof(properties[0]),
                          context);
        if (!resume)
            return;
    }
    callback(NULL, 0, context);
}

static int ipfs_drive_list_files(HiveDrive *base, const char *path,
                                 HiveFilesIterateCallback *callback, void *context)
{
    IPFSDrive *drive = (IPFSDrive *)base;
    char url[MAX_URL_LEN] = {0};
    http_client_t *httpc;
    long resp_code;
    cJSON *response;
    char *p;
    int rc;

    rc = ipfs_token_check_reachable(drive->token);
    if (rc < 0)
        return rc;

    sprintf(url, "http://%s:%d/api/v0/files/ls",
           ipfs_token_get_current_node(drive->token), NODE_API_PORT);

    httpc = http_client_new();
    if (!httpc)
        return HIVE_GENERAL_ERROR(HIVEERR_OUT_OF_MEMORY);

    http_client_set_url(httpc, url);
    http_client_set_query(httpc, "uid", ipfs_token_get_uid(drive->token));
    http_client_set_query(httpc, "path", path);
    http_client_set_method(httpc, HTTP_METHOD_POST);
    http_client_set_request_body_instant(httpc, NULL, 0);
    http_client_enable_response_body(httpc);

    rc = http_client_request(httpc);
    if (rc) {
        rc = HIVE_HTTPC_ERROR(rc);
        if (RC_NODE_UNREACHABLE(rc)) {
            rc = HIVE_GENERAL_ERROR(HIVEERR_TRY_AGAIN);
            ipfs_token_mark_node_unreachable(drive->token);
        }
        goto error_exit;
    }

    rc = http_client_get_response_code(httpc, &resp_code);
    if (rc) {
        rc = HIVE_HTTPC_ERROR(rc);
        goto error_exit;
    }

    if (resp_code != 200) {
        rc = HIVE_HTTP_STATUS_ERROR(resp_code);
        goto error_exit;
    }

    p = http_client_move_response_body(httpc, NULL);
    http_client_close(httpc);

    if (!p)
        return HIVE_GENERAL_ERROR(HIVEERR_OUT_OF_MEMORY);

    response = parse_list_files_response(p);
    free(p);
    if (!response)
        return hive_get_error();

    notify_file_entries(cJSON_GetObjectItemCaseSensitive(response, "Entries"),
                        callback, context);
    cJSON_Delete(response);
    return 0;

error_exit:
    http_client_close(httpc);
    return rc;
}

static int ipfs_drive_make_dir(HiveDrive *base, const char *path)
{
    IPFSDrive *drive = (IPFSDrive *)base;
    char url[MAX_URL_LEN] = {0};
    http_client_t *httpc;
    long resp_code;
    int rc;

    rc = ipfs_token_check_reachable(drive->token);
    if (rc < 0)
        return rc;

    sprintf(url, "http://%s:%d/api/v0/files/mkdir",
            ipfs_token_get_current_node(drive->token), NODE_API_PORT);

    httpc = http_client_new();
    if (!httpc)
        return HIVE_GENERAL_ERROR(HIVEERR_OUT_OF_MEMORY);

    http_client_set_url(httpc, url);
    http_client_set_query(httpc, "uid", ipfs_token_get_uid(drive->token));
    http_client_set_query(httpc, "path", path);
    http_client_set_query(httpc, "parents", "true");
    http_client_set_method(httpc, HTTP_METHOD_POST);
    http_client_set_request_body_instant(httpc, NULL, 0);

    rc = http_client_request(httpc);
    if (rc) {
        rc = HIVE_HTTPC_ERROR(rc);
        if (RC_NODE_UNREACHABLE(rc)) {
            HIVE_GENERAL_ERROR(HIVEERR_TRY_AGAIN);
            ipfs_token_mark_node_unreachable(drive->token);
        }
        goto error_exit;
    }

    rc = http_client_get_response_code(httpc, &resp_code);
    http_client_close(httpc);
    if (rc)
        return HIVE_HTTPC_ERROR(rc);

    if (resp_code != 200)
        return HIVE_HTTP_STATUS_ERROR(resp_code);

    rc = publish_root_hash(drive->token, url, sizeof(url));
    if (rc < 0) {
        if (RC_NODE_UNREACHABLE(rc)) {
            rc = HIVE_GENERAL_ERROR(HIVEERR_TRY_AGAIN);
            ipfs_token_mark_node_unreachable(drive->token);
        }
        return rc;
    }

    return 0;

error_exit:
    http_client_close(httpc);
    return rc;
}

static int ipfs_drive_move_file(HiveDrive *base, const char *old, const char *new)
{
    IPFSDrive *drive = (IPFSDrive *)base;
    char url[MAX_URL_LEN] = {0};
    http_client_t *httpc;
    long resp_code;
    int rc;

    rc = ipfs_token_check_reachable(drive->token);
    if (rc < 0)
        return rc;

    sprintf(url, "http://%s:%d/api/v0/files/mv",
             ipfs_token_get_current_node(drive->token), NODE_API_PORT);

    httpc = http_client_new();
    if (!httpc)
        return HIVE_GENERAL_ERROR(HIVEERR_OUT_OF_MEMORY);

    http_client_set_url(httpc, url);
    http_client_set_query(httpc, "uid", ipfs_token_get_uid(drive->token));
    http_client_set_query(httpc, "source", old);
    http_client_set_query(httpc, "dest", new);
    http_client_set_method(httpc, HTTP_METHOD_POST);
    http_client_set_request_body_instant(httpc, NULL, 0);

    rc = http_client_request(httpc);
    if (rc) {
        rc = HIVE_HTTPC_ERROR(rc);
        if (RC_NODE_UNREACHABLE(rc)) {
            rc = HIVE_GENERAL_ERROR(HIVEERR_TRY_AGAIN);
            ipfs_token_mark_node_unreachable(drive->token);
        }
        goto error_exit;
    }

    rc = http_client_get_response_code(httpc, &resp_code);
    http_client_close(httpc);
    if (rc)
        return HIVE_HTTPC_ERROR(rc);

    if (resp_code != 200)
        return HIVE_HTTP_STATUS_ERROR(resp_code);

    rc = publish_root_hash(drive->token, url, sizeof(url));
    if (rc < 0) {
        if (RC_NODE_UNREACHABLE(rc)) {
            rc = HIVE_GENERAL_ERROR(HIVEERR_TRY_AGAIN);
            ipfs_token_mark_node_unreachable(drive->token);
        }
        return rc;
    }

    return 0;

error_exit:
    http_client_close(httpc);
    return rc;
}

static int ipfs_drive_copy_file(HiveDrive *base, const char *src_path, const char *dest_path)
{
    IPFSDrive *drive = (IPFSDrive *)base;
    char url[MAX_URL_LEN] = {0};
    http_client_t *httpc;
    long resp_code = 0;
    HiveFileInfo src_info;
    int rc;

    rc = ipfs_token_check_reachable(drive->token);
    if (rc < 0)
        return rc;

    rc = ipfs_drive_stat_file(base, src_path, &src_info);
    if (rc < 0)
        return rc;

    sprintf(url, "http://%s:%d/api/v0/files/cp",
             ipfs_token_get_current_node(drive->token), NODE_API_PORT);

    httpc = http_client_new();
    if (!httpc)
        return HIVE_GENERAL_ERROR(HIVEERR_OUT_OF_MEMORY);

    http_client_set_url(httpc, url);
    http_client_set_query(httpc, "uid", ipfs_token_get_uid(drive->token));
    http_client_set_query(httpc, "source", src_info.fileid);
    http_client_set_query(httpc, "dest", dest_path);
    http_client_set_method(httpc, HTTP_METHOD_POST);
    http_client_set_request_body_instant(httpc, NULL, 0);

    rc = http_client_request(httpc);
    if (rc) {
        rc = HIVE_HTTPC_ERROR(rc);
        if (RC_NODE_UNREACHABLE(rc)) {
            rc = HIVE_GENERAL_ERROR(HIVEERR_TRY_AGAIN);
            ipfs_token_mark_node_unreachable(drive->token);
        }
        goto error_exit;
    }

    rc = http_client_get_response_code(httpc, &resp_code);
    http_client_close(httpc);
    if (rc)
        return HIVE_HTTPC_ERROR(rc);

    if (resp_code != 200)
        return HIVE_HTTP_STATUS_ERROR(resp_code);

    rc = publish_root_hash(drive->token, url, sizeof(url));
    if (rc < 0) {
        if (RC_NODE_UNREACHABLE(rc)) {
            rc = HIVE_GENERAL_ERROR(HIVEERR_TRY_AGAIN);
            ipfs_token_mark_node_unreachable(drive->token);
        }
        return -1;
    }

    return 0;

error_exit:
    http_client_close(httpc);
    return rc;
}

static int ipfs_drive_delete_file(HiveDrive *base, const char *path)
{
    IPFSDrive *drive = (IPFSDrive *)base;
    char url[MAX_URL_LEN] = {0};
    http_client_t *httpc;
    long resp_code;
    int rc;

    rc = ipfs_token_check_reachable(drive->token);
    if (rc < 0)
        return rc;

    sprintf(url, "http://%s:%d/api/v0/files/rm",
             ipfs_token_get_current_node(drive->token), NODE_API_PORT);

    httpc = http_client_new();
    if (!httpc)
        return HIVE_GENERAL_ERROR(HIVEERR_OUT_OF_MEMORY);

    http_client_set_url(httpc, url);
    http_client_set_query(httpc, "uid", ipfs_token_get_uid(drive->token));
    http_client_set_query(httpc, "path", path);
    http_client_set_query(httpc, "recursive", "true");
    http_client_set_method(httpc, HTTP_METHOD_POST);
    http_client_set_request_body_instant(httpc, NULL, 0);

    rc = http_client_request(httpc);
    if (rc) {
        rc = HIVE_HTTPC_ERROR(rc);
        if (RC_NODE_UNREACHABLE(rc)) {
            rc = HIVE_GENERAL_ERROR(HIVEERR_TRY_AGAIN);
            ipfs_token_mark_node_unreachable(drive->token);
        }
        goto error_exit;
    }

    rc = http_client_get_response_code(httpc, &resp_code);
    http_client_close(httpc);
    if (rc)
        return HIVE_HTTPC_ERROR(rc);

    if (resp_code != 200)
        return HIVE_HTTP_STATUS_ERROR(resp_code);

    rc = publish_root_hash(drive->token, url, sizeof(url));
    if (rc < 0) {
        if (RC_NODE_UNREACHABLE(rc)) {
            rc = HIVE_GENERAL_ERROR(HIVEERR_TRY_AGAIN);
            ipfs_token_mark_node_unreachable(drive->token);
        }
        return rc;
    }

    return 0;

error_exit:
    http_client_close(httpc);
    return rc;
}

static void ipfs_drive_close(HiveDrive *obj)
{
    deref(obj);
}

static void ipfs_drive_destructor(void *obj)
{
    IPFSDrive *drive = (IPFSDrive *)obj;

    if (drive->token)
        ipfs_token_close(drive->token);
}

int ipfs_drive_open_file(HiveDrive *base, const char *path, int flags, HiveFile **file)
{
    IPFSDrive *drive = (IPFSDrive *)base;

    return ipfs_file_open(drive->token, path, flags, file);
}

HiveDrive *ipfs_drive_open(ipfs_token_t *token)
{
    IPFSDrive *drive;

    assert(token);

    drive = (IPFSDrive *)rc_zalloc(sizeof(IPFSDrive), ipfs_drive_destructor);
    if (!drive) {
        hive_set_error(HIVE_GENERAL_ERROR(HIVEERR_OUT_OF_MEMORY));
        return NULL;
    }

    drive->base.get_info    = &ipfs_drive_get_info;
    drive->base.stat_file   = &ipfs_drive_stat_file;
    drive->base.list_files  = &ipfs_drive_list_files;
    drive->base.make_dir    = &ipfs_drive_make_dir;
    drive->base.move_file   = &ipfs_drive_move_file;
    drive->base.copy_file   = &ipfs_drive_copy_file;
    drive->base.delete_file = &ipfs_drive_delete_file;
    drive->base.open_file   = &ipfs_drive_open_file;
    drive->base.close       = &ipfs_drive_close;

    drive->token            = ref(token);

    return &drive->base;
}
