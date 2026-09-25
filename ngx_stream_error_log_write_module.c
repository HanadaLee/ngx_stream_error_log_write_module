
/*
 * Copyright (C) Hanada
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_stream.h>

#if (NGX_EXPR)
#include <ngx_stream_expr_module.h>
#endif


typedef struct {
    ngx_uint_t                    level;
    ngx_stream_complex_value_t   *message;
#if (NGX_EXPR)
    ngx_expr_when_id_t            expr_id;
#else
    ngx_stream_complex_value_t   *filter;
    ngx_int_t                     negative;
#endif
} ngx_stream_error_log_write_entry_t;


typedef struct {
    ngx_array_t                  *log_entries;
} ngx_stream_error_log_write_srv_conf_t;


static ngx_int_t ngx_stream_error_log_write_init(ngx_conf_t *cf);
static void *ngx_stream_error_log_write_create_srv_conf(ngx_conf_t *cf);
static char *ngx_stream_error_log_write_merge_srv_conf(ngx_conf_t *cf,
    void *parent, void *child);
static char *ngx_stream_error_log_write(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static ngx_int_t ngx_stream_error_log_write_handler(ngx_stream_session_t *s);


static ngx_command_t ngx_stream_error_log_write_commands[] = {

    { ngx_string("error_log_write"),
      NGX_STREAM_MAIN_CONF|NGX_STREAM_SRV_CONF
#if (NGX_EXPR)
                           |NGX_STREAM_MAIN_WHEN_CONF
                           |NGX_STREAM_SRV_WHEN_CONF|NGX_CONF_TAKE12,
#else
                           |NGX_CONF_TAKE123,
#endif
      ngx_stream_error_log_write,
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


static ngx_stream_module_t ngx_stream_error_log_write_module_ctx = {
    NULL,                                           /* preconfiguration */
    ngx_stream_error_log_write_init,                /* postconfiguration */

    NULL,                                           /* create main conf */
    NULL,                                           /* init main conf */

    ngx_stream_error_log_write_create_srv_conf,     /* create server conf */
    ngx_stream_error_log_write_merge_srv_conf       /* merge server conf */
};


ngx_module_t ngx_stream_error_log_write_module = {
    NGX_MODULE_V1,
    &ngx_stream_error_log_write_module_ctx,         /* module context */
    ngx_stream_error_log_write_commands,            /* module directives */
    NGX_STREAM_MODULE,                              /* module type */
    NULL,                                           /* init master */
    NULL,                                           /* init module */
    NULL,                                           /* init process */
    NULL,                                           /* init thread */
    NULL,                                           /* exit thread */
    NULL,                                           /* exit process */
    NULL,                                           /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_int_t
ngx_stream_error_log_write_handler(ngx_stream_session_t *s)
{
    ngx_stream_error_log_write_srv_conf_t  *escf;
    ngx_stream_error_log_write_entry_t     *entries;
    ngx_str_t                               message;
    ngx_uint_t                              i;
#if !(NGX_EXPR)
    ngx_str_t                               val;
#endif

    ngx_log_debug0(NGX_LOG_DEBUG_STREAM, s->connection->log, 0,
                   "error_log_write handler");

    escf = ngx_stream_get_module_srv_conf(s,
                                          ngx_stream_error_log_write_module);

    if (escf->log_entries == NULL || escf->log_entries->nelts == 0) {
        return NGX_DECLINED;
    }

    entries = escf->log_entries->elts;

    for (i = 0; i < escf->log_entries->nelts; i++) {

#if (NGX_EXPR)
        if (ngx_stream_expr_get_result(s, entries[i].expr_id)
            != NGX_EXPR_WHEN_HIT)
        {
            continue;
        }
#else
        if (entries[i].filter) {
            if (ngx_stream_complex_value(s, entries[i].filter, &val) != NGX_OK)
            {
                return NGX_ERROR;
            }

            if (val.len == 0 || (val.len == 1 && val.data[0] == '0')) {
                if (!entries[i].negative) {
                    continue;
                }

            } else {
                if (entries[i].negative) {
                    continue;
                }
            }
        }
#endif

        if (ngx_stream_complex_value(s, entries[i].message, &message) != NGX_OK)
        {
            ngx_log_error(NGX_LOG_ERR, s->connection->log, 0,
                          "error_log_write: failed to evaluate message");
            continue;
        }

        ngx_log_error(entries[i].level, s->connection->log, 0,
                      "%V", &message);
    }

    return NGX_DECLINED;
}


static char *
ngx_stream_error_log_write(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_stream_error_log_write_srv_conf_t  *escf = conf;

    ngx_str_t                            *value;
    ngx_stream_error_log_write_entry_t   *entry;
    ngx_uint_t                            n;
    ngx_str_t                             s;
    ngx_stream_compile_complex_value_t    ccv;

    if (escf->log_entries == NULL) {
        escf->log_entries = ngx_array_create(cf->pool, 1,
                                sizeof(ngx_stream_error_log_write_entry_t));
        if (escf->log_entries == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    entry = ngx_array_push(escf->log_entries);
    if (entry == NULL) {
        return NGX_CONF_ERROR;
    }

    entry->level = NGX_LOG_ERR;
#if (NGX_EXPR)
    entry->expr_id = ngx_expr_get_associated_when_id(cf);
#else
    entry->filter = NULL;
    entry->negative = 0;
#endif

    value = cf->args->elts;

    for (n = 1; n < cf->args->nelts; n++) {

        if (ngx_strncmp(value[n].data, "level=", 6) == 0) {
            s.len = value[n].len - 6;
            s.data = value[n].data + 6;

            if (s.len == 6 && ngx_strncmp(s.data, "stderr", 6) == 0) {
                entry->level = NGX_LOG_STDERR;
                continue;
            }

            if (s.len == 5 && ngx_strncmp(s.data, "emerg", 5) == 0) {
                entry->level = NGX_LOG_EMERG;
                continue;
            }

            if (s.len == 5 && ngx_strncmp(s.data, "alert", 5) == 0) {
                entry->level = NGX_LOG_ALERT;
                continue;
            }

            if (s.len == 4 && ngx_strncmp(s.data, "crit", 4) == 0) {
                entry->level = NGX_LOG_CRIT;
                continue;
            }

            if ((s.len == 3 && ngx_strncmp(s.data, "err", 3) == 0)
                || (s.len == 5 && ngx_strncmp(s.data, "error", 5) == 0))
            {
                entry->level = NGX_LOG_ERR;
                continue;
            }

            if (s.len == 4 && ngx_strncmp(s.data, "warn", 4) == 0) {
                entry->level = NGX_LOG_WARN;
                continue;
            }

            if (s.len == 6 && ngx_strncmp(s.data, "notice", 6) == 0) {
                entry->level = NGX_LOG_NOTICE;
                continue;
            }

            if (s.len == 4 && ngx_strncmp(s.data, "info", 4) == 0) {
                entry->level = NGX_LOG_INFO;
                continue;
            }

            if (s.len == 5 && ngx_strncmp(s.data, "debug", 5) == 0) {
                entry->level = NGX_LOG_DEBUG;
                continue;
            }

            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid log level \"%V\"", &s);
            return NGX_CONF_ERROR;
        }

        if (ngx_strncmp(value[n].data, "message=", 8) == 0) {
            s.data = value[n].data + 8;
            s.len = value[n].len - 8;

            ngx_memzero(&ccv, sizeof(ngx_stream_compile_complex_value_t));
            ccv.cf = cf;
            ccv.value = &s;
            ccv.complex_value = ngx_palloc(cf->pool,
                                           sizeof(ngx_stream_complex_value_t));

            if (ccv.complex_value == NULL) {
                return NGX_CONF_ERROR;
            }

            if (ngx_stream_compile_complex_value(&ccv) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

            entry->message = ccv.complex_value;

            continue;
        }

#if !(NGX_EXPR)
        if (ngx_strncmp(value[n].data, "if=", 3) == 0) {
            s.len = value[n].len - 3;
            s.data = value[n].data + 3;

            ngx_memzero(&ccv, sizeof(ngx_stream_compile_complex_value_t));
            ccv.cf = cf;
            ccv.value = &s;
            ccv.complex_value = ngx_palloc(cf->pool,
                                           sizeof(ngx_stream_complex_value_t));

            if (ccv.complex_value == NULL) {
                return NGX_CONF_ERROR;
            }

            if (ngx_stream_compile_complex_value(&ccv) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

            entry->filter = ccv.complex_value;
            entry->negative = 0;

            continue;
        }

        if (ngx_strncmp(value[n].data, "if!=", 4) == 0) {
            s.len = value[n].len - 4;
            s.data = value[n].data + 4;

            ngx_memzero(&ccv, sizeof(ngx_stream_compile_complex_value_t));
            ccv.cf = cf;
            ccv.value = &s;
            ccv.complex_value = ngx_palloc(cf->pool,
                                           sizeof(ngx_stream_complex_value_t));

            if (ccv.complex_value == NULL) {
                return NGX_CONF_ERROR;
            }

            if (ngx_stream_compile_complex_value(&ccv) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

            entry->filter = ccv.complex_value;
            entry->negative = 1;

            continue;
        }
#endif

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[n]);
        return NGX_CONF_ERROR;
    }

    if (entry->message == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"error_log_write\" requires \"message\" "
                           "parameter");
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static void *
ngx_stream_error_log_write_create_srv_conf(ngx_conf_t *cf)
{
    ngx_stream_error_log_write_srv_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool,
                       sizeof(ngx_stream_error_log_write_srv_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->log_entries = NULL;

    return conf;
}


static char *
ngx_stream_error_log_write_merge_srv_conf(ngx_conf_t *cf, void *parent,
    void *child)
{
    ngx_stream_error_log_write_srv_conf_t  *prev = parent;
    ngx_stream_error_log_write_srv_conf_t  *conf = child;

    ngx_uint_t                              i;
    ngx_stream_error_log_write_entry_t     *entry;
    ngx_stream_error_log_write_entry_t     *prev_entries;

    if (conf->log_entries == NULL || conf->log_entries->nelts == 0) {
        conf->log_entries = prev->log_entries;
        return NGX_CONF_OK;
    }

    if (prev->log_entries == NULL || prev->log_entries->nelts == 0) {
        return NGX_CONF_OK;
    }

    prev_entries = prev->log_entries->elts;

    for (i = 0; i < prev->log_entries->nelts; i++) {
        entry = ngx_array_push(conf->log_entries);
        if (entry == NULL) {
            return NGX_CONF_ERROR;
        }

        *entry = prev_entries[i];
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_stream_error_log_write_init(ngx_conf_t *cf)
{
    ngx_stream_core_main_conf_t *cmcf;
    ngx_stream_handler_pt       *h;

    cmcf = ngx_stream_conf_get_module_main_conf(cf, ngx_stream_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_STREAM_LOG_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_stream_error_log_write_handler;

    return NGX_OK;
}
