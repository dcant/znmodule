#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct
{
    ngx_str_t		sdat[3];
} ngx_http_zsubrequest_ctx_t;


static char *
ngx_http_zsubrequest(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static ngx_int_t ngx_http_zsubrequest_handler(ngx_http_request_t *r);
static ngx_int_t zsubrequest_subrequest_post_handler(ngx_http_request_t *r, void *data, ngx_int_t rc);
static void
zsubrequest_post_handler(ngx_http_request_t * r);




static ngx_command_t  ngx_http_zsubrequest_commands[] =
{

    {
        ngx_string("zsubrequest"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_HTTP_LMT_CONF | NGX_CONF_NOARGS,
        ngx_http_zsubrequest,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL
    },

    ngx_null_command
};

static ngx_http_module_t  ngx_http_zsubrequest_module_ctx =
{
    NULL,                       /* preconfiguration */
    NULL,                  		/* postconfiguration */

    NULL,                       /* create main configuration */
    NULL,                       /* init main configuration */

    NULL,                       /* create server configuration */
    NULL,                       /* merge server configuration */

    NULL,       			    /* create location configuration */
    NULL         			    /* merge location configuration */
};

ngx_module_t  ngx_http_zsubrequest_module =
{
    NGX_MODULE_V1,
    &ngx_http_zsubrequest_module_ctx,           /* module context */
    ngx_http_zsubrequest_commands,              /* module directives */
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

static ngx_int_t zsubrequest_subrequest_post_handler(ngx_http_request_t *r,
                                                void *data, ngx_int_t rc)
{
    ngx_http_request_t          *pr = r->parent;

    ngx_http_zsubrequest_ctx_t* myctx = ngx_http_get_module_ctx(pr, ngx_http_zsubrequest_module);

    pr->headers_out.status = r->headers_out.status;

    if (r->headers_out.status == NGX_HTTP_OK)
    {
        int flag = 0;
        int indx = 0;
        u_char *p = NULL;

        ngx_buf_t* pRecvBuf = &r->upstream->buffer;

        for (; pRecvBuf->pos != pRecvBuf->last; pRecvBuf->pos++)
        {
            if (*pRecvBuf->pos == ',' || *pRecvBuf->pos == '\"')
            {
                if (flag == 1 || flag == 2 || flag == 6)
                {
                    myctx->sdat[indx - 1].len = pRecvBuf->pos - p;
                }
                flag++;
                if (flag == 1 || flag == 2 || flag == 6)
                    myctx->sdat[indx++].data = pRecvBuf->pos + 1;
                p = pRecvBuf->pos + 1;
            }

            if (flag > 6)
                break;
        }
    }

    pr->write_event_handler = zsubrequest_post_handler;

    return NGX_OK;
}


static void
zsubrequest_post_handler(ngx_http_request_t * r)
{
    if (r->headers_out.status != NGX_HTTP_OK)
    {
        ngx_http_finalize_request(r, r->headers_out.status);
        return;
    }

    ngx_http_zsubrequest_ctx_t* myctx = ngx_http_get_module_ctx(r, ngx_http_zsubrequest_module);

    ngx_str_t output_format = ngx_string("[%V], Current price: %V, Turnover: %V");

    int bodylen = output_format.len + myctx->sdat[0].len
                  + myctx->sdat[1].len + myctx->sdat[2].len - 6;
    r->headers_out.content_length_n = bodylen;

    ngx_buf_t* b = ngx_create_temp_buf(r->pool, bodylen);
    ngx_snprintf(b->pos, bodylen, (char*)output_format.data,
                 &myctx->sdat[0], &myctx->sdat[1], &myctx->sdat[2]);
    b->last = b->pos + bodylen;
    b->last_buf = 1;

    ngx_chain_t out;
    out.buf = b;
    out.next = NULL;

    static ngx_str_t type = ngx_string("text/plain; charset=GBK");
    r->headers_out.content_type = type;
    r->headers_out.status = NGX_HTTP_OK;

    r->connection->buffered |= NGX_HTTP_WRITE_BUFFERED;
    ngx_int_t ret = ngx_http_send_header(r);
    ret = ngx_http_output_filter(r, &out);

    ngx_http_finalize_request(r, ret);
}



static char *
ngx_http_zsubrequest(ngx_conf_t * cf, ngx_command_t * cmd, void * conf)
{
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

    clcf->handler = ngx_http_zsubrequest_handler;

    return NGX_CONF_OK;
}

static ngx_int_t
ngx_http_zsubrequest_handler(ngx_http_request_t * r)
{
    ngx_http_zsubrequest_ctx_t* myctx = ngx_http_get_module_ctx(r, ngx_http_zsubrequest_module);
    if (myctx == NULL)
    {
        myctx = ngx_palloc(r->pool, sizeof(ngx_http_zsubrequest_ctx_t));
        if (myctx == NULL)
        {
            return NGX_ERROR;
        }

        ngx_http_set_ctx(r, myctx, ngx_http_zsubrequest_module);
    }

    ngx_http_post_subrequest_t *psr = ngx_palloc(r->pool, sizeof(ngx_http_post_subrequest_t));
    if (psr == NULL)
    {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    psr->handler = zsubrequest_subrequest_post_handler;

    psr->data = myctx;

    ngx_str_t sub_prefix = ngx_string("/list=");
    ngx_str_t sub_location;
    sub_location.len = sub_prefix.len + r->args.len;
    sub_location.data = ngx_palloc(r->pool, sub_location.len);
    ngx_snprintf(sub_location.data, sub_location.len,
                 "%V%V", &sub_prefix, &r->args);

    ngx_http_request_t *sr;

    ngx_int_t rc = ngx_http_subrequest(r, &sub_location, NULL, &sr, psr, NGX_HTTP_SUBREQUEST_IN_MEMORY);
    if (rc != NGX_OK)
    {
        return NGX_ERROR;
    }

    return NGX_DONE;
}