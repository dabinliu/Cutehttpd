
/**
  @author Ying Zeng (YinzCN_at_Gmail.com)
**/


#include "chtd.h"
#include "fcgi.h"


struct fcgi_pmgr_t *
fcgi_pmgr_new(struct htdx_t *htdx, char *extname, char *sz_addr, char *sz_port, char *sz_cmdl) {
    struct fcgi_pmgr_t *fcgi_pmgr = calloc(1, sizeof(struct fcgi_pmgr_t));
    fcgi_pmgr->n_conn_max = 64;
    strncpy(fcgi_pmgr->cgiextname, extname, 15);
    strncpy(fcgi_pmgr->fcgid_addr, sz_addr, 63);
    strncpy(fcgi_pmgr->fcgid_port, sz_port, 15);
    strncpy(fcgi_pmgr->fcgid_cmdl, sz_cmdl, 255);
    pthread_mutex_init(&fcgi_pmgr->mutex, NULL);
    fcgi_pmgr->htdx = htdx;
    return fcgi_pmgr;
}


void
fcgi_pmgr_del(struct fcgi_pmgr_t *fcgi_pmgr)
{
    if (!fcgi_pmgr) {
        return;
    }
    pthread_mutex_destroy(&fcgi_pmgr->mutex);
    fcgi_pmgr->htdx->fcgi_pmgr = NULL;
    free(fcgi_pmgr);
}


int
fcgi_pmgr_proc_spawn(struct fcgi_pmgr_t *fcgi_pmgr)
{
    int nm_port = atoi(fcgi_pmgr->sz_port);
    struct fcgi_proc_t *curr, *last;
    curr = fcgi_pmgr->fcgi_procs;
    last = curr->prev;
    while (1) {
        if (curr->port == nm_port) {
            nm_port++;
            curr = last->next;
        }
        if (curr == last) {
            break;
        }
        curr = curr->next;
    }

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    si.cb  = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    char sz_port[8];
    char *temp = str_replace("@addr@", fcgi_pmgr->fcgi_addr, fcgi_pmgr->fcgi_cmdl);
    char *cmdl = str_replace("@port@", itoa(nm_port, sz_port, 10), temp);
    free(temp);

    int retn;
    retn = CreateProcessA(NULL, cmdl, NULL, NULL, TRUE, CREATE_NEW_PROCESS_GROUP, envblk, dir, &si, &pi);
    if (retn) {
        struct fcgi_proc_t *fcgi_proc = calloc(1, sizeof(struct fcgi_proc_t));        
        fcgi_proc->rsa.u.sin.sin_family = AF_INET;
        fcgi_proc->rsa.u.sin.sin_addr.s_addr = inet_addr(fcgi_pmgr->fcgi_addr);
        fcgi_proc->rsa.u.sin.sin_port = htons(nm_port);
        if (fcgi_pmgr->fcgi_procs) {
            fcgi_proc->prev = fcgi_pmgr->fcgi_procs->prev;
            fcgi_proc->next = fcgi_pmgr->fcgi_procs;
            fcgi_proc->prev->next = fcgi_proc;
            fcgi_proc->next->prev = fcgi_proc;
        } else {
            fcgi_proc->prev = fcgi_proc;
            fcgi_proc->next = fcgi_proc;
            fcgi_pmgr->fcgi_procs = fcgi_proc;
        }
    } else {
        htd_cry(fcgi_pmgr->htdx, "CreateProcessA() failed! [%s]", cmdl);
    }
    free(cmdl);
    return retn ? 0 : -1;
}


struct fcgi_pmgr_t *
fcgi_pmgr_match(struct htdx_t *htdx, char *extname)
{
    struct fcgi_pmgr_t *curr, *last;
    if (htdx->fcgi_pmgrs == NULL) {
        return NULL;
    }
    curr = htdx->fcgi_pmgrs;
    last = curr->prev;
    while (1) {
        if (strcasecmp(curr->cgiextname, extname) == 0) {
            return curr;
        }
        if (curr == last) {
            break;
        }
        curr = curr->next;
    }
    return NULL;
}


int
fcgi_pmgr_conn(struct fcgi_pmgr_t *fcgi_pmgr, struct fcgi_conn_t *fcgi_conn)
{
    struct sock_t *sock = &fcgi_conn->sock;

    /* [ get rsa */
    struct fcgi_proc_t *curr, *last;
    curr = fcgi_pmgr->fcgi_procs;
    last = curr->prev;
    while (1) {
        if (1) {
            memcpy(sock->rsa, curr->rsa, sizeof(struct usa_t));
        }
        if (curr == last) {
            break;
        }
        curr = curr->next;
    }
    /* ] */

    sock->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock->socket == 0) {
        chtd_cry(htdx, "ERROR: fcgi_conn_new() -> socket() failed!%d");
        return -1;
    }

    if (connect(sock->socket, &sock->rsa.u.sa, sizeof(sock->rsa.u)) == -1) {
        chtd_cry(htdx, "ERROR: connect() to fastcgi server (%s:%s) failed!",
                 fcgi_pmgr->fcgid_addr, fcgi_pmgr->fcgid_port);
#ifdef WIN32
        closesocket(sock->socket);
#else
        close(sock->socket);
#endif
        return -1;
    }
    return 0;
}


int
chtd_set_fcgi(struct htdx_t *htdx, char *extname, char *sz_addr, char *sz_port, char *sz_cmdl)
{
    int port = atoi(sz_port);

    if (port > 65535 || port < 1) {
        sz_port = "9000";
    }

    if (strlen(sz_addr) == 0) {
        sz_addr = "0.0.0.0";
    }

    fcgi_pmgr_del(htdx->fcgi_pmgr);
    htdx->fcgi_pmgr = fcgi_pmgr_new(htdx, extname, sz_addr, sz_port, sz_cmdl);
    free(temp);
    free(cmdl);
    return 1;
}

