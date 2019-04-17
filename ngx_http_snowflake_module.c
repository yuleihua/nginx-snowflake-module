// Copyright 2019 The huayulei_2003@hotmail.com Authors

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


#define SNOWFLAKE_EPOCH 1546272000 //2019/1/1
#define SNOWFLAKE_TIME_BITS 32
#define SNOWFLAKE_GROUPID_BITS 8
#define SNOWFLAKE_WORKERID_BITS 6
#define SNOWFLAKE_SEQUENCE_BITS 17

static ngx_int_t ngx_http_snowflake_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_snowflake_init(ngx_conf_t *cf);
static void *ngx_http_snowflake_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_snowflake_group_id(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t snowflake_id(ngx_int_t group_id, ngx_log_t *log);
static char *ngx_http_snowflake_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);


typedef struct  {
    // milliseconds since SNOWFLAKE_EPOCH
    ngx_int_t time;
    ngx_int_t seq_max;
    ngx_int_t worker_id;
    ngx_int_t group_id_mx;
    ngx_int_t seq;
    ngx_int_t time_shift_bits;
    ngx_int_t group_shift_bits;
    ngx_int_t worker_shift_bits;
} snowflake_t;

typedef struct
{
	ngx_int_t group_id;

}ngx_http_snowflake_loc_conf_t;

static void *
ngx_http_snowflake_create_loc_conf(ngx_conf_t *cf)
{
	ngx_http_snowflake_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_snowflake_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }
    conf->group_id = NGX_CONF_UNSET;
    return conf;
}

static ngx_command_t ngx_http_snowflake_commands[] = {

    {
      ngx_string("snowflake_group_id"),
	  NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_snowflake_group_id,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_snowflake_loc_conf_t, group_id),
      NULL },

    ngx_null_command
};

static ngx_http_module_t ngx_http_snowflake_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_snowflake_init,               /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

	ngx_http_snowflake_create_loc_conf,    /* create location configuration */
	ngx_http_snowflake_merge_loc_conf      /* merge location configuration */
};


ngx_module_t  ngx_http_snowflake_module = {
    NGX_MODULE_V1,
    &ngx_http_snowflake_module_ctx,        /* module context */
	ngx_http_snowflake_commands,           /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};

static snowflake_t ngx_snowflake = {};

static ngx_int_t
ngx_http_snowflake_handler(ngx_http_request_t *r)
{
    ngx_int_t                  rc;
    ngx_int_t                  content_length;
    ngx_buf_t                  *b;
    ngx_chain_t                out;
    u_char                     tmp_string[64] = {0};
    ngx_http_snowflake_loc_conf_t  *my_conf;


    ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, "ngx_http_snowflake_handler is called!");

    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    my_conf = ngx_http_get_module_loc_conf(r, ngx_http_snowflake_module);
    if (my_conf->group_id == NGX_CONF_UNSET ){
        ngx_log_error(NGX_LOG_EMERG, r->connection->log, 0, "group_id is empty!");
        return NGX_DECLINED;
    }

    ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, "group id is %l", my_conf->group_id);

    rc = ngx_http_discard_request_body(r);

    if (rc != NGX_OK) {
        return rc;
    }

    ngx_str_set(&r->headers_out.content_type, "application/json");

    ngx_snprintf(tmp_string, sizeof(tmp_string), "{\"id\":%l}", snowflake_id(my_conf->group_id, r->connection->log));
    content_length = ngx_strlen(tmp_string);

    /* send the header only, if the request type is http 'HEAD' */
    if (r->method == NGX_HTTP_HEAD) {
        r->headers_out.status = NGX_HTTP_OK;
        r->headers_out.content_length_n = content_length;

        return ngx_http_send_header(r);
    }

    /* allocate a buffer for your response body */
    b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    /* attach this buffer to the buffer chain */
    out.buf = b;
    out.next = NULL;

    /* adjust the pointers of the buffer */
    b->pos = tmp_string;
    b->last = tmp_string + content_length;
    b->memory = 1;    /* this buffer is in memory */
    b->last_buf = 1;  /* this is the last buffer in the buffer chain */

    /* set the status line */
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = content_length;

    /* send the headers of your response */
    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    return ngx_http_output_filter(r, &out);
}


static ngx_int_t
snowflake_id(ngx_int_t group_id, ngx_log_t *log) {
    ngx_int_t secs = ngx_time() - SNOWFLAKE_EPOCH;
    ngx_int_t id = 0L;
    ngx_int_t gid = 0L;

    // Catch NTP clock adjustment that rolls time backwards and sequence number overflow
    if ((ngx_snowflake.seq > ngx_snowflake.seq_max ) || ngx_snowflake.time > secs) {
        while (ngx_snowflake.time >= secs) {
            ngx_log_error(NGX_LOG_WARN, log, 0,
            "sleep time, time:%l and secs:%l", ngx_snowflake.time, secs);

            ngx_msleep(100);

            secs = ngx_time() - SNOWFLAKE_EPOCH;
        }
    }

    if (ngx_snowflake.time < secs) {
    	ngx_log_error(NGX_LOG_WARN, log, 0,
    	            "reset, time:%l and secs:%l", ngx_snowflake.time, secs);
        ngx_snowflake.time = secs;
        ngx_snowflake.seq = 0L;
    }

    gid = group_id%ngx_snowflake.group_id_mx;

    id = (secs << ngx_snowflake.time_shift_bits)
            | (gid << ngx_snowflake.group_shift_bits)
            | (ngx_snowflake.worker_id << ngx_snowflake.worker_shift_bits)
            | (ngx_snowflake.seq++);
    return id;
}

static ngx_int_t
ngx_http_snowflake_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;
    ngx_int_t                  max_worker_id;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_snowflake_handler;

    max_worker_id = (1 << SNOWFLAKE_WORKERID_BITS) - 1;
    ngx_snowflake.seq_max = (1L << SNOWFLAKE_SEQUENCE_BITS) - 1;
    ngx_snowflake.group_id_mx = (1L << SNOWFLAKE_GROUPID_BITS) - 1;
    ngx_snowflake.worker_id = ngx_getpid()%max_worker_id;
    ngx_snowflake.seq_max = (1L << SNOWFLAKE_SEQUENCE_BITS) - 1;
    ngx_snowflake.time_shift_bits   = SNOWFLAKE_GROUPID_BITS + SNOWFLAKE_WORKERID_BITS + SNOWFLAKE_SEQUENCE_BITS;
    ngx_snowflake.group_shift_bits = SNOWFLAKE_WORKERID_BITS + SNOWFLAKE_SEQUENCE_BITS;
    ngx_snowflake.worker_shift_bits = SNOWFLAKE_SEQUENCE_BITS;

    ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0,
                       "ngx_snowflake time_max: \"%l\"", (1L << ngx_snowflake.time_shift_bits) - 1);
    ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0,
                       "ngx_snowflake seq_max: \"%l\"", ngx_snowflake.seq_max);
    ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0,
                       "ngx_snowflake seq: \"%l\"", ngx_snowflake.seq);
    ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0,
                       "ngx_snowflake time: \"%l\"", ngx_snowflake.time);
    ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0,
                       "ngx_snowflake worker_id: \"%l\"", ngx_snowflake.worker_id);
    ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0,
                       "ngx_snowflake worker_id_max: \"%l\"", max_worker_id);
    ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0,
                       "ngx_snowflake group_id_mx: \"%l\"", ngx_snowflake.group_id_mx);

    return NGX_OK;
}


static char
*ngx_http_snowflake_group_id(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_snowflake_loc_conf_t *local_conf;
    local_conf = conf;
    char *rv = NULL;

    rv = ngx_conf_set_num_slot(cf, cmd, conf);

    ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0, "http_snowflake_group_id:%d", local_conf->group_id);

    return rv;
}


static char *
ngx_http_snowflake_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
	ngx_http_snowflake_loc_conf_t *prev = parent;
	ngx_http_snowflake_loc_conf_t *conf = child;

	ngx_conf_merge_value(conf->group_id,
                              prev->group_id, NGX_CONF_UNSET);

	ngx_conf_log_error(NGX_LOG_NOTICE, cf, 0, "new http_snowflake_group_id:%d", conf->group_id);
    return NGX_CONF_OK;
}
