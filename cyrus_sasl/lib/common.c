/* common.c - Functions that are common to server and clinet
 * Rob Siemborski
 * Tim Martin
 * $Id: common.c,v 1.3 2002/05/23 18:59:47 snsimon Exp $
 */
/* 
 * Copyright (c) 2001 Carnegie Mellon University.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any other legal
 *    details, please contact  
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#ifdef HAVE_SYSLOG
#include <syslog.h>
#endif
#include <stdarg.h>
#include <ctype.h>

#include <sasl.h>
#include <saslutil.h>
#include <saslplug.h>
#include "saslint.h"

#ifdef WIN32
/* need to handle the fact that errno has been defined as a function
   in a dll, not an extern int */
# ifdef errno
#  undef errno
# endif /* errno */
#endif /* WIN32 */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

static const char build_ident[] = "$Build: libsasl " PACKAGE "-" VERSION " $";

/* Should be a null-terminated array that lists the available mechanisms */
static const char **global_mech_list = NULL;

void *free_mutex = NULL;

void (*_sasl_client_cleanup_hook)(void) = NULL;
void (*_sasl_server_cleanup_hook)(void) = NULL;
int (*_sasl_client_idle_hook)(sasl_conn_t *conn) = NULL;
int (*_sasl_server_idle_hook)(sasl_conn_t *conn) = NULL;

sasl_allocation_utils_t _sasl_allocation_utils={
  (sasl_malloc_t *)  &malloc,
  (sasl_calloc_t *)  &calloc,
  (sasl_realloc_t *) &realloc,
  (sasl_free_t *) &free
};

/* Intenal mutex functions do as little as possible (no thread protection) */
static void *sasl_mutex_alloc(void)
{
  return (void *)0x1;
}

static int sasl_mutex_lock(void *mutex __attribute__((unused)))
{
    return SASL_OK;
}

static int sasl_mutex_unlock(void *mutex __attribute__((unused)))
{
    return SASL_OK;
}

static void sasl_mutex_free(void *mutex __attribute__((unused)))
{
    return;
}

sasl_mutex_utils_t _sasl_mutex_utils={
  &sasl_mutex_alloc,
  &sasl_mutex_lock,
  &sasl_mutex_unlock,
  &sasl_mutex_free
};

void sasl_set_mutex(sasl_mutex_alloc_t *n, sasl_mutex_lock_t *l,
		    sasl_mutex_unlock_t *u, sasl_mutex_free_t *d)
{
  _sasl_mutex_utils.alloc=n;
  _sasl_mutex_utils.lock=l;
  _sasl_mutex_utils.unlock=u;
  _sasl_mutex_utils.free=d;
}

/* copy a string to malloced memory */
int _sasl_strdup(const char *in, char **out, size_t *outlen)
{
  size_t len = strlen(in);
  if (outlen) *outlen = len;
  *out=sasl_ALLOC(len + 1);
  if (! *out) return SASL_NOMEM;
  strcpy((char *) *out, in);
  return SASL_OK;
}

/* adds a string to the buffer; reallocing if need be */
int _sasl_add_string(char **out, size_t *alloclen,
		     size_t *outlen, const char *add)
{
  size_t addlen;

  if (add==NULL) add = "(null)";

  addlen=strlen(add); /* only compute once */
  if (_buf_alloc(out, alloclen, (*outlen)+addlen)!=SASL_OK)
    return SASL_NOMEM;

  strncpy(*out + *outlen, add, addlen);
  *outlen += addlen;

  return SASL_OK;
}

/* security-encode a regular string.  Mostly a wrapper for sasl_encodev */
/* output is only valid until next call to sasl_encode or sasl_encodev */
int sasl_encode(sasl_conn_t *conn, const char *input,
		unsigned inputlen,
		const char **output, unsigned *outputlen)
{
    int result;
    struct iovec tmp;

    if(!conn) return SASL_BADPARAM;
    if(!input || !inputlen || !output || !outputlen)
	PARAMERROR(conn);
    
    /* maxoutbuf checking is done in sasl_encodev */

    /* Note: We are casting a const pointer here, but it's okay
     * because we believe people downstream of us are well-behaved, and the
     * alternative is an absolute mess, performance-wise. */
    tmp.iov_base = (void *)input;
    tmp.iov_len = inputlen;
    
    result = sasl_encodev(conn, &tmp, 1, output, outputlen);

    RETURN(conn, result);
}

/* security-encode an iovec */
/* output is only valid until next call to sasl_encode or sasl_encodev */
int sasl_encodev(sasl_conn_t *conn,
		 const struct iovec *invec, unsigned numiov,
		 const char **output, unsigned *outputlen)
{
    int result;
    unsigned i;
    size_t total_size = 0;

    if (!conn) return SASL_BADPARAM;
    if (! invec || ! output || ! outputlen || numiov < 1)
	PARAMERROR(conn);

    if(!conn->props.maxbufsize) {
	sasl_seterror(conn, 0,
		      "called sasl_encode[v] with application that does not support security layers");
	return SASL_TOOWEAK;
    }

    /* This might be better to check on a per-plugin basis, but I think
     * it's cleaner and more effective here.  It also encourages plugins
     * to be honest about what they accept */

    for(i=0; i<numiov;i++) {
	total_size += invec[i].iov_len;
    }    
    if(total_size > conn->oparams.maxoutbuf)
	PARAMERROR(conn);

    if(conn->oparams.encode == NULL)  {
	result = _iovec_to_buf(invec, numiov, &conn->encode_buf);
	if(result != SASL_OK) INTERROR(conn, result);
       
	*output = conn->encode_buf->data;
	*outputlen = conn->encode_buf->curlen;

    } else {
	result = conn->oparams.encode(conn->context, invec, numiov,
				      output, outputlen);
    }

    RETURN(conn, result);
}
 
/* output is only valid until next call to sasl_decode */
int sasl_decode(sasl_conn_t *conn,
		const char *input, unsigned inputlen,
		const char **output, unsigned *outputlen)
{
    int result;

    if(!conn) return SASL_BADPARAM;
    if(!input || !output || !outputlen)
	PARAMERROR(conn);

    if(!conn->props.maxbufsize) {
	sasl_seterror(conn, 0,
		      "called sasl_decode with application that does not support security layers");
	RETURN(conn, SASL_TOOWEAK);
    }

    if(conn->oparams.decode == NULL)
    {
	/* Since we know how long the output is maximally, we can
	 * just allocate it to begin with, and never need another
         * allocation! */

	/* However, if they pass us more than they actually can take,
	 * we cannot help them... */
	if(inputlen > conn->props.maxbufsize) {
	    sasl_seterror(conn, 0,
			  "input too large for default sasl_decode");
	    RETURN(conn,SASL_BUFOVER);
	}

	if(!conn->decode_buf)
	    conn->decode_buf = sasl_ALLOC(conn->props.maxbufsize + 1);
	if(!conn->decode_buf)	
	    MEMERROR(conn);
	
	memcpy(conn->decode_buf, input, inputlen);
	conn->decode_buf[inputlen] = '\0';
	*output = conn->decode_buf;
	*outputlen = inputlen;
	
        return SASL_OK;
    } else {
        result = conn->oparams.decode(conn->context, input, inputlen,
                                      output, outputlen);
        RETURN(conn, result);
    }

    INTERROR(conn, SASL_FAIL);
}


void
sasl_set_alloc(sasl_malloc_t *m,
	       sasl_calloc_t *c,
	       sasl_realloc_t *r,
	       sasl_free_t *f)
{
  _sasl_allocation_utils.malloc=m;
  _sasl_allocation_utils.calloc=c;
  _sasl_allocation_utils.realloc=r;
  _sasl_allocation_utils.free=f;
}

void sasl_done(void)
{
  if (_sasl_server_cleanup_hook)
    _sasl_server_cleanup_hook();

  if (_sasl_client_cleanup_hook)
    _sasl_client_cleanup_hook();
  
  _sasl_canonuser_free();
  _sasl_done_with_plugins();
  
  sasl_MUTEX_FREE(free_mutex);
  free_mutex = NULL;

  _sasl_free_utils(&sasl_global_utils);

  sasl_FREE(global_mech_list);
  global_mech_list = NULL;

  /* in case of another init/done */
  _sasl_server_cleanup_hook = NULL;
  _sasl_client_cleanup_hook = NULL;

  _sasl_client_idle_hook = NULL;
  _sasl_server_idle_hook = NULL;
}

/* fills in the base sasl_conn_t info */
int _sasl_conn_init(sasl_conn_t *conn,
		    const char *service,
		    unsigned int flags,
		    enum Sasl_conn_type type,
		    int (*idle_hook)(sasl_conn_t *conn),
		    const char *serverFQDN,
		    const char *iplocalport,
		    const char *ipremoteport,
		    const sasl_callback_t *callbacks,
		    const sasl_global_callbacks_t *global_callbacks) {
  int result = SASL_OK;

  conn->type = type;

  result = _sasl_strdup(service, &conn->service, NULL);
  if (result != SASL_OK) 
      MEMERROR(conn);

  memset(&conn->oparams, 0, sizeof(sasl_out_params_t));
  memset(&conn->external, 0, sizeof(_sasl_external_properties_t));

  conn->flags = flags;

  result = sasl_setprop(conn, SASL_IPLOCALPORT, iplocalport);
  if(result != SASL_OK)
      RETURN(conn, result);
  
  result = sasl_setprop(conn, SASL_IPREMOTEPORT, ipremoteport);
  if(result != SASL_OK)
      RETURN(conn, result);
  
  conn->encode_buf = NULL;
  conn->context = NULL;
  conn->secret = NULL;
  conn->idle_hook = idle_hook;
  conn->callbacks = callbacks;
  conn->global_callbacks = global_callbacks;

  memset(&conn->props, 0, sizeof(conn->props));

  /* Start this buffer out as an empty string */
  conn->error_code = SASL_OK;
  conn->errdetail_buf = conn->error_buf = NULL;
  conn->errdetail_buf_len = conn->error_buf_len = 150;

  result = _buf_alloc(&conn->error_buf, &conn->error_buf_len, 150);     
  if(result != SASL_OK) MEMERROR(conn);
  result = _buf_alloc(&conn->errdetail_buf, &conn->errdetail_buf_len, 150);
  if(result != SASL_OK) MEMERROR(conn);
  
  conn->error_buf[0] = '\0';
  conn->errdetail_buf[0] = '\0';
  
  conn->decode_buf = NULL;

  if(serverFQDN) {
      result = _sasl_strdup(serverFQDN, &conn->serverFQDN, NULL);
  } else if (conn->type == SASL_CONN_SERVER) {
      /* We can fake it because we *are* the server */
      char name[MAXHOSTNAMELEN];
      memset(name, 0, sizeof(name));
      gethostname(name, MAXHOSTNAMELEN);
      
      result = _sasl_strdup(name, &conn->serverFQDN, NULL);
  } else {
      conn->serverFQDN = NULL;
  }
  

  if(result != SASL_OK) MEMERROR( conn );

  RETURN(conn, SASL_OK);
}

/* It turns out to be conveinent to have a shared sasl_utils_t */
const sasl_utils_t *sasl_global_utils = NULL;

int _sasl_common_init(void)
{
    int result;
    
    /* Setup the global utilities */
    if(!sasl_global_utils) {
	sasl_global_utils = _sasl_alloc_utils(NULL, NULL);
	if(sasl_global_utils == NULL) return SASL_NOMEM;
    }

    /* Init the canon_user plugin */
    result = sasl_canonuser_add_plugin("INTERNAL", internal_canonuser_init);
    if(result != SASL_OK) return result;    

    if (!free_mutex)
	free_mutex = sasl_MUTEX_ALLOC();
    if (!free_mutex) return SASL_FAIL;

    return SASL_OK;
}

/* dispose connection state, sets it to NULL
 *  checks for pointer to NULL
 */
void sasl_dispose(sasl_conn_t **pconn)
{
  int result;

  if (! pconn) return;
  if (! *pconn) return;

  /* serialize disposes. this is necessary because we can't
     dispose of conn->mutex if someone else is locked on it */
  result = sasl_MUTEX_LOCK(free_mutex);
  if (result!=SASL_OK) return;
  
  /* *pconn might have become NULL by now */
  if (! (*pconn)) return;

  (*pconn)->destroy_conn(*pconn);
  sasl_FREE(*pconn);
  *pconn=NULL;

  sasl_MUTEX_UNLOCK(free_mutex);
}

void _sasl_conn_dispose(sasl_conn_t *conn) {
  if (conn->serverFQDN)
      sasl_FREE(conn->serverFQDN);

  if (conn->external.auth_id)
      sasl_FREE(conn->external.auth_id);

  if(conn->encode_buf) {
      if(conn->encode_buf->data) sasl_FREE(conn->encode_buf->data);
      sasl_FREE(conn->encode_buf);
  }

  if(conn->error_buf)
      sasl_FREE(conn->error_buf);
  
  if(conn->errdetail_buf)
      sasl_FREE(conn->errdetail_buf);

  if(conn->decode_buf)
      sasl_FREE(conn->decode_buf);

  if(conn->mechlist_buf)
      sasl_FREE(conn->mechlist_buf);

  if(conn->service)
      sasl_FREE(conn->service);

  /* oparams sub-members should be freed by the plugin, in so much
   * as they were allocated by the plugin */
}


/* get property from SASL connection state
 *  propnum       -- property number
 *  pvalue        -- pointer to value
 * returns:
 *  SASL_OK       -- no error
 *  SASL_NOTDONE  -- property not available yet
 *  SASL_BADPARAM -- bad property number
 */
int sasl_getprop(sasl_conn_t *conn, int propnum, const void **pvalue)
{
  int result = SASL_OK;
  sasl_getopt_t *getopt;
  void *context;
  
  if (! conn) return SASL_BADPARAM;
  if (! pvalue) PARAMERROR(conn);

  switch(propnum)
  {
  case SASL_SSF:
      *(sasl_ssf_t **)pvalue= &conn->oparams.mech_ssf;
      break;      
  case SASL_MAXOUTBUF:
      *(unsigned **)pvalue = &conn->oparams.maxoutbuf;
      break;
  case SASL_GETOPTCTX:
      result = _sasl_getcallback(conn, SASL_CB_GETOPT, &getopt, &context);
      if(result != SASL_OK) break;
      
      *(void **)pvalue = context;
      break;
  case SASL_CALLBACK:
      *(const sasl_callback_t **)pvalue = conn->callbacks;
      break;
  case SASL_IPLOCALPORT:
      if(conn->got_ip_local)
	  *(const char **)pvalue = conn->iplocalport;
      else {
	  *(const char **)pvalue = NULL;
	  result = SASL_NOTDONE;
      }
      break;
  case SASL_IPREMOTEPORT:
      if(conn->got_ip_remote)
	  *(const char **)pvalue = conn->ipremoteport;
      else {
	  *(const char **)pvalue = NULL;
	  result = SASL_NOTDONE;
      }	  
      break;
  case SASL_USERNAME:
      if(! conn->oparams.user)
	  result = SASL_NOTDONE;
      else
	  *((const char **)pvalue) = conn->oparams.user;
      break;
  case SASL_SERVERFQDN:
      *((const char **)pvalue) = conn->serverFQDN;
      break;
  case SASL_DEFUSERREALM:
      if(conn->type != SASL_CONN_SERVER) result = SASL_BADPROT;
      else
	  *((const char **)pvalue) = ((sasl_server_conn_t *)conn)->user_realm;
      break;
  case SASL_SERVICE:
      *((const char **)pvalue) = conn->service;
      break;
  case SASL_AUTHSOURCE: /* name of plugin (not name of mech) */
      if(conn->type == SASL_CONN_CLIENT) {
	  if(!((sasl_client_conn_t *)conn)->mech) {
	      result = SASL_NOTDONE;
	      break;
	  }
	  *((const char **)pvalue) =
	      ((sasl_client_conn_t *)conn)->mech->plugname;
      } else if (conn->type == SASL_CONN_SERVER) {
	  if(!((sasl_server_conn_t *)conn)->mech) {
	      result = SASL_NOTDONE;
	      break;
	  }
	  *((const char **)pvalue) =
	      ((sasl_server_conn_t *)conn)->mech->plugname;
      } else {
	  result = SASL_BADPARAM;
      }
      break;
  case SASL_MECHNAME: /* name of mech */
      if(conn->type == SASL_CONN_CLIENT) {
	  if(!((sasl_client_conn_t *)conn)->mech) {
	      result = SASL_NOTDONE;
	      break;
	  }
	  *((const char **)pvalue) =
	      ((sasl_client_conn_t *)conn)->mech->plug->mech_name;
      } else if (conn->type == SASL_CONN_SERVER) {
	  if(!((sasl_server_conn_t *)conn)->mech) {
	      result = SASL_NOTDONE;
	      break;
	  }
	  *((const char **)pvalue) =
	      ((sasl_server_conn_t *)conn)->mech->plug->mech_name;
      } else {
	  result = SASL_BADPARAM;
      }
      
      if(!(*pvalue) && result == SASL_OK) result = SASL_NOTDONE;
      break;
  case SASL_PLUGERR:
      *((const char **)pvalue) = conn->error_buf;
      break;
  case SASL_SSF_EXTERNAL:
      *((const sasl_ssf_t **)pvalue) = &conn->external.ssf;
      break;
  case SASL_AUTH_EXTERNAL:
      *((const char **)pvalue) = conn->external.auth_id;
      break;
  case SASL_SEC_PROPS:
      *((const sasl_security_properties_t **)pvalue) = &conn->props;
      break;
  default: 
      result = SASL_BADPARAM;
  }

  if(result == SASL_BADPARAM) {
      PARAMERROR(conn);
  } else if(result == SASL_NOTDONE) {
      sasl_seterror(conn, SASL_NOLOG,
		    "Information that was requested is not yet available.");
      RETURN(conn, result);
  } else if(result != SASL_OK) {
      INTERROR(conn, result);
  } else
      RETURN(conn, result); 
}

/* set property in SASL connection state
 * returns:
 *  SASL_OK       -- value set
 *  SASL_BADPARAM -- invalid property or value
 */
int sasl_setprop(sasl_conn_t *conn, int propnum, const void *value)
{
  int result = SASL_OK;
  char *str;

  /* make sure the sasl context is valid */
  if (!conn)
    return SASL_BADPARAM;

  switch(propnum)
  {
  case SASL_SSF_EXTERNAL:
      conn->external.ssf = *((sasl_ssf_t *)value);
      break;

  case SASL_AUTH_EXTERNAL:
      if(value && strlen(value)) {
	  result = _sasl_strdup(value, &str, NULL);
	  if(result != SASL_OK) MEMERROR(conn);
      } else {
	  str = NULL;
      }

      if(conn->external.auth_id)
	  sasl_FREE(conn->external.auth_id);

      conn->external.auth_id = str;

      break;

  case SASL_DEFUSERREALM:
      if(conn->type != SASL_CONN_SERVER) {
	sasl_seterror(conn, 0, "Tried to set realm on non-server connection");
	result = SASL_BADPROT;
	break;
      }

      if(value && strlen(value)) {
	  result = _sasl_strdup(value, &str, NULL);
	  if(result != SASL_OK) MEMERROR(conn);
      } else {
	  PARAMERROR(conn);
      }

      if(((sasl_server_conn_t *)conn)->user_realm)
      	  sasl_FREE(((sasl_server_conn_t *)conn)->user_realm);

      ((sasl_server_conn_t *)conn)->user_realm = str;
      ((sasl_server_conn_t *)conn)->sparams->user_realm = str;

      break;

  case SASL_SEC_PROPS:
      memcpy(&(conn->props),(sasl_security_properties_t *)value,
	     sizeof(sasl_security_properties_t));
      break;

  case SASL_IPREMOTEPORT:
  {
      const char *ipremoteport = (const char *)value;
      if(!value) {
	  conn->got_ip_remote = 0; 
      } else if (_sasl_ipfromstring(ipremoteport, NULL, 0)
		 != SASL_OK) {
	  sasl_seterror(conn, 0, "Bad IPREMOTEPORT value");
	  RETURN(conn, SASL_BADPARAM);
      } else {
	  strcpy(conn->ipremoteport, ipremoteport);
	  conn->got_ip_remote = 1;
      }
      
      if(conn->got_ip_remote) {
	  if(conn->type == SASL_CONN_CLIENT) {
	      ((sasl_client_conn_t *)conn)->cparams->ipremoteport
		  = conn->ipremoteport;
	  } else if (conn->type == SASL_CONN_SERVER) {
	      ((sasl_server_conn_t *)conn)->sparams->ipremoteport
		  = conn->ipremoteport;
	  }
      } else {
	  if(conn->type == SASL_CONN_CLIENT) {
	      ((sasl_client_conn_t *)conn)->cparams->ipremoteport
		  = NULL;
	  } else if (conn->type == SASL_CONN_SERVER) {
	      ((sasl_server_conn_t *)conn)->sparams->ipremoteport
		  = NULL;
	  }
      }

      break;
  }

  case SASL_IPLOCALPORT:
  {
      const char *iplocalport = (const char *)value;
      if(!value) {
	  conn->got_ip_local = 0;	  
      } else if (_sasl_ipfromstring(iplocalport, NULL, 0)
		 != SASL_OK) {
	  sasl_seterror(conn, 0, "Bad IPLOCALPORT value");
	  RETURN(conn, SASL_BADPARAM);
      } else {
	  strcpy(conn->iplocalport, iplocalport);
	  conn->got_ip_local = 1;
      }

      if(conn->got_ip_local) {
	  if(conn->type == SASL_CONN_CLIENT) {
	      ((sasl_client_conn_t *)conn)->cparams->iplocalport
		  = conn->iplocalport;
	  } else if (conn->type == SASL_CONN_SERVER) {
	      ((sasl_server_conn_t *)conn)->sparams->iplocalport
		  = conn->iplocalport;
	  }
      } else {
	  if(conn->type == SASL_CONN_CLIENT) {
	      ((sasl_client_conn_t *)conn)->cparams->iplocalport
		  = NULL;
	  } else if (conn->type == SASL_CONN_SERVER) {
	      ((sasl_server_conn_t *)conn)->sparams->iplocalport
		  = NULL;
	  }
      }
      break;
  }

  default:
      sasl_seterror(conn, 0, "Unknown parameter type");
      result = SASL_BADPARAM;
  }
  
  RETURN(conn, result);
}

/* this is apparently no longer a user function */
static int sasl_usererr(int saslerr)
{
    /* Hide the difference in a username failure and a password failure */
    if (saslerr == SASL_NOUSER)
	return SASL_BADAUTH;

    /* otherwise return the error given; no transform necessary */
    return saslerr;
}

const char *sasl_errstring(int saslerr,
			   const char *langlist __attribute__((unused)),
			   const char **outlang)
{
  if (outlang) *outlang="en-us";

  switch(saslerr)
    {
    case SASL_CONTINUE: return "another step is needed in authentication";
    case SASL_OK:       return "successful result";
    case SASL_FAIL:     return "generic failure";
    case SASL_NOMEM:    return "no memory available";
    case SASL_BUFOVER:  return "overflowed buffer";
    case SASL_NOMECH:   return "no mechanism available";
    case SASL_BADPROT:  return "bad protocol / cancel";
    case SASL_NOTDONE:  return "can't request info until later in exchange";
    case SASL_BADPARAM: return "invalid parameter supplied";
    case SASL_TRYAGAIN: return "transient failure (e.g., weak key)";
    case SASL_BADMAC:   return "integrity check failed";
    case SASL_NOTINIT:  return "SASL library not initialized";
                             /* -- client only codes -- */
    case SASL_INTERACT:   return "needs user interaction";
    case SASL_BADSERV:    return "server failed mutual authentication step";
    case SASL_WRONGMECH:  return "mechanism doesn't support requested feature";
                             /* -- server only codes -- */
    case SASL_BADAUTH:    return "authentication failure";
    case SASL_NOAUTHZ:    return "authorization failure";
    case SASL_TOOWEAK:    return "mechanism too weak for this user";
    case SASL_ENCRYPT:    return "encryption needed to use mechanism";
    case SASL_TRANS:      return "One time use of a plaintext password will enable requested mechanism for user";
    case SASL_EXPIRED:    return "passphrase expired, has to be reset";
    case SASL_DISABLED:   return "account disabled";
    case SASL_NOUSER:     return "user not found";
    case SASL_BADVERS:    return "version mismatch with plug-in";
    case SASL_UNAVAIL:    return "remote authentication server unavailable";
    case SASL_NOVERIFY:   return "user exists, but no verifier for user";
    case SASL_PWLOCK:     return "passphrase locked";
    case SASL_NOCHANGE:   return "requested change was not needed";
    case SASL_WEAKPASS:   return "passphrase is too weak for security policy";
    case SASL_NOUSERPASS: return "user supplied passwords are not permitted";

    default:   return "undefined error!";
    }

}

/* Return the sanitized error detail about the last error that occured for 
 * a connection */
const char *sasl_errdetail(sasl_conn_t *conn) 
{
    unsigned need_len;
    const char *errstr;
    char leader[128];

    if(!conn) return NULL;
    
    errstr = sasl_errstring(conn->error_code, NULL, NULL);
    snprintf(leader,128,"SASL(%d): %s: ",
	     sasl_usererr(conn->error_code), errstr);
    
    need_len = strlen(leader) + strlen(conn->error_buf) + 12;
    _buf_alloc(&conn->errdetail_buf, &conn->errdetail_buf_len, need_len);

    snprintf(conn->errdetail_buf, need_len, "%s%s", leader, conn->error_buf);
   
    return conn->errdetail_buf;
}


/* Note that this needs the global callbacks, so if you don't give getcallbacks
 * a sasl_conn_t, you're going to need to pass it yourself (or else we couldn't
 * have client and server at the same time */
static int _sasl_global_getopt(void *context,
			       const char *plugin_name,
			       const char *option,
			       const char ** result,
			       size_t *len)
{
  const sasl_global_callbacks_t * global_callbacks;
  const sasl_callback_t *callback;

  global_callbacks = (const sasl_global_callbacks_t *) context;

  if (global_callbacks && global_callbacks->callbacks) {
      for (callback = global_callbacks->callbacks;
	   callback->id != SASL_CB_LIST_END;
	   callback++) {
	  if (callback->id == SASL_CB_GETOPT
	      && (((sasl_getopt_t *)(callback->proc))(callback->context,
						      plugin_name,
						      option,
						      result,
						      (unsigned *)len)
		  == SASL_OK))
	      return SASL_OK;
      }
  }

  /* look it up in our configuration file */
  *result = sasl_config_getstring(option, NULL);
  if (*result != NULL) {
      if (len) { *len = strlen(*result); }
      return SASL_OK;
  }

  return SASL_FAIL;
}

static int
_sasl_conn_getopt(void *context,
		  const char *plugin_name,
		  const char *option,
		  const char ** result,
		  unsigned *len)
{
  sasl_conn_t * conn;
  const sasl_callback_t *callback;

  if (! context)
    return SASL_BADPARAM;

  conn = (sasl_conn_t *) context;

  if (conn->callbacks)
    for (callback = conn->callbacks;
	 callback->id != SASL_CB_LIST_END;
	 callback++)
      if (callback->id == SASL_CB_GETOPT
	  && (((sasl_getopt_t *)(callback->proc))(callback->context,
						  plugin_name,
						  option,
						  result,
						  len)
	      == SASL_OK))
	return SASL_OK;

  /* If we made it here, we didn't find an appropriate callback
   * in the connection's callback list, or the callback we did
   * find didn't return SASL_OK.  So we attempt to use the
   * global callback for this connection... */
  return _sasl_global_getopt((void *)conn->global_callbacks,
			     plugin_name,
			     option,
			     result,
			     len);
}

#ifdef HAVE_SYSLOG
/* this is the default logging */
static int _sasl_syslog(void *context __attribute__((unused)),
			int priority,
			const char *message)
{
    int syslog_priority;

    /* set syslog priority */
    switch(priority) {
    case SASL_LOG_NONE:
	return SASL_OK;
	break;
    case SASL_LOG_ERR:
	syslog_priority = LOG_ERR;
	break;
    case SASL_LOG_WARN:
	syslog_priority = LOG_WARNING;
	break;
    case SASL_LOG_NOTE:
    case SASL_LOG_FAIL:
	syslog_priority = LOG_NOTICE;
	break;
    case SASL_LOG_PASS:
    case SASL_LOG_TRACE:
    case SASL_LOG_DEBUG:
    default:
	syslog_priority = LOG_DEBUG;
	break;
    }
    
    /* do the syslog call. do not need to call openlog */
    syslog(syslog_priority | LOG_AUTH, "%s", message);
    
    return SASL_OK;
}
#endif				/* HAVE_SYSLOG */

static int
_sasl_getsimple(void *context,
		int id,
		const char ** result,
		size_t *len)
{
  const char *userid;
  sasl_conn_t *conn;

  if (! context || ! result) return SASL_BADPARAM;

  conn = (sasl_conn_t *)context;

  switch(id) {
  case SASL_CB_AUTHNAME:
    userid = getenv("USER");
    if (userid != NULL) {
	*result = userid;
	if (len) *len = strlen(userid);
	return SASL_OK;
    }
    userid = getenv("USERNAME");
    if (userid != NULL) {
	*result = userid;
	if (len) *len = strlen(userid);
	return SASL_OK;
    }
#ifdef WIN32
    /* for win32, try using the GetUserName standard call */
    {
	DWORD i;
	BOOL rval;
	static char sender[128];
	
	i = sizeof(sender);
	rval = GetUserName(sender, &i);
	if ( rval) { /* got a userid */
		*result = sender;
		if (len) *len = strlen(sender);
		return SASL_OK;
	}
    }
#endif /* WIN32 */
    return SASL_FAIL;
  default:
    return SASL_BADPARAM;
  }
}

static int
_sasl_getpath(void *context __attribute__((unused)),
	      const char **path)
{
  if (! path)
    return SASL_BADPARAM;

  *path = getenv(SASL_PATH_ENV_VAR);
  if (! *path)
    *path = PLUGINDIR;

  return SASL_OK;
}

static int
_sasl_verifyfile(void *context __attribute__((unused)),
		 char *file  __attribute__((unused)),
		 int type  __attribute__((unused)))
{
  /* always say ok */
  return SASL_OK;
}


static int
_sasl_proxy_policy(sasl_conn_t *conn,
		   void *context __attribute__((unused)),
		   const char *requested_user, unsigned rlen,
		   const char *auth_identity, unsigned alen,
		   const char *def_realm __attribute__((unused)),
		   unsigned urlen __attribute__((unused)),
		   struct propctx *propctx __attribute__((unused)))
{
    if (!conn)
	return SASL_BADPARAM;

    if (!requested_user || *requested_user == '\0')
	return SASL_OK;

    if (!auth_identity || !requested_user || rlen != alen ||
	(memcmp(auth_identity, requested_user, rlen) != 0)) {
	sasl_seterror(conn, 0,
		      "Requested identity not authenticated identity");
	RETURN(conn, SASL_BADAUTH);
    }

    return SASL_OK;
}

int _sasl_getcallback(sasl_conn_t * conn,
		      unsigned long callbackid,
		      int (**pproc)(),
		      void **pcontext)
{
  const sasl_callback_t *callback;

  if (!pproc || !pcontext)
      PARAMERROR(conn);

  /* Some callbacks are always provided by the library */
  switch (callbackid) {
  case SASL_CB_LIST_END:
    /* Nothing ever gets to provide this */
      INTERROR(conn, SASL_FAIL);
  case SASL_CB_GETOPT:
      if (conn) {
	  *pproc = &_sasl_conn_getopt;
	  *pcontext = conn;
      } else {
	  *pproc = &_sasl_global_getopt;
	  *pcontext = NULL;
      }
      return SASL_OK;
  }

  /* If it's not always provided by the library, see if there's
   * a version provided by the application for this connection... */
  if (conn && conn->callbacks) {
    for (callback = conn->callbacks; callback->id != SASL_CB_LIST_END;
	 callback++) {
	if (callback->id == callbackid) {
	    *pproc = callback->proc;
	    *pcontext = callback->context;
	    if (callback->proc) {
		return SASL_OK;
	    } else {
		return SASL_INTERACT;
	    }
	}
    }
  }

  /* And, if not for this connection, see if there's one
   * for all {server,client} connections... */
  if (conn && conn->global_callbacks && conn->global_callbacks->callbacks) {
      for (callback = conn->global_callbacks->callbacks;
	   callback->id != SASL_CB_LIST_END;
	   callback++) {
	  if (callback->id == callbackid) {
	      *pproc = callback->proc;
	      *pcontext = callback->context;
	      if (callback->proc) {
		  return SASL_OK;
	      } else {
		  return SASL_INTERACT;
	      }
	  }
      }
  }

  /* Otherwise, see if the library provides a default callback. */
  switch (callbackid) {
#ifdef HAVE_SYSLOG
  case SASL_CB_LOG:
    *pproc = (int (*)()) &_sasl_syslog;
    *pcontext = NULL;
    return SASL_OK;
#endif /* HAVE_SYSLOG */
  case SASL_CB_GETPATH:
    *pproc = (int (*)()) &_sasl_getpath;
    *pcontext = NULL;
    return SASL_OK;
  case SASL_CB_AUTHNAME:
    *pproc = (int (*)()) &_sasl_getsimple;
    *pcontext = conn;
    return SASL_OK;
  case SASL_CB_VERIFYFILE:
    *pproc = & _sasl_verifyfile;
    *pcontext = NULL;
    return SASL_OK;
  case SASL_CB_PROXY_POLICY:
    *pproc = (int (*)()) &_sasl_proxy_policy;
    *pcontext = NULL;
    return SASL_OK;
  }

  /* Unable to find a callback... */
  *pproc = NULL;
  *pcontext = NULL;
  sasl_seterror(conn, SASL_NOLOG, "Unable to find a callback: %d", callbackid);
  RETURN(conn,SASL_FAIL);
}


/*
 * This function is typically called from a plugin.
 * It creates a string from the formatting and varargs given
 * and calls the logging callback (syslog by default)
 *
 * %m will parse the value in the next argument as an errno string
 * %z will parse the next argument as a SASL error code.
 */

void
_sasl_log (sasl_conn_t *conn,
	   int level,
	   const char *fmt,
	   ...)
{
  char *out=(char *) sasl_ALLOC(250);
  size_t alloclen=100; /* current allocated length */
  size_t outlen=0; /* current length of output buffer */
  size_t formatlen;
  size_t pos=0; /* current position in format string */
  int result;
  sasl_log_t *log_cb;
  void *log_ctx;
  
  int ival;
  char *cval;
  va_list ap; /* varargs thing */

  if(!fmt) goto done;
  if(!out) return;
  
  formatlen = strlen(fmt);

  /* See if we have a logging callback... */
  result = _sasl_getcallback(conn, SASL_CB_LOG, &log_cb, &log_ctx);
  if (result == SASL_OK && ! log_cb)
    result = SASL_FAIL;
  if (result != SASL_OK) goto done;
  
  va_start(ap, fmt); /* start varargs */

  while(pos<formatlen)
  {
    if (fmt[pos]!='%') /* regular character */
    {
      result = _buf_alloc(&out, &alloclen, outlen+1);
      if (result != SASL_OK) goto done;
      out[outlen]=fmt[pos];
      outlen++;
      pos++;

    } else { /* formating thing */
      int done=0;
      char frmt[10];
      int frmtpos=1;
      char tempbuf[21];
      frmt[0]='%';
      pos++;

      while (done==0)
      {
	switch(fmt[pos])
	  {
	  case 's': /* need to handle this */
	    cval = va_arg(ap, char *); /* get the next arg */
	    result = _sasl_add_string(&out, &alloclen,
				&outlen, cval);
	      
	    if (result != SASL_OK) /* add the string */
		goto done;

	    done=1;
	    break;

	  case '%': /* double % output the '%' character */
	    result = _buf_alloc(&out,&alloclen,outlen+1);
	    if (result != SASL_OK)
		goto done;
	    
	    out[outlen]='%';
	    outlen++;
	    done=1;
	    break;

	  case 'm': /* insert the errno string */
	    result = _sasl_add_string(&out, &alloclen, &outlen,
				strerror(va_arg(ap, int)));
	    if (result != SASL_OK)
		goto done;
	    
	    done=1;
	    break;

	  case 'z': /* insert the sasl error string */
	    result = _sasl_add_string(&out, &alloclen, &outlen,
				(char *) sasl_errstring(va_arg(ap, int),NULL,NULL));
	    if (result != SASL_OK)
		goto done;
	    
	    done=1;
	    break;

	  case 'c':
	    frmt[frmtpos++]=fmt[pos];
	    frmt[frmtpos]=0;
	    tempbuf[0] = (char) va_arg(ap, int); /* get the next arg */
	    tempbuf[1]='\0';
	    
	    /* now add the character */
	    result = _sasl_add_string(&out, &alloclen, &outlen, tempbuf);
	    if (result != SASL_OK)
		goto done;
		
	    done=1;
	    break;

	  case 'd':
	  case 'i':
	    frmt[frmtpos++]=fmt[pos];
	    frmt[frmtpos]=0;
	    ival = va_arg(ap, int); /* get the next arg */

	    snprintf(tempbuf,20,frmt,ival); /* have snprintf do the work */
	    /* now add the string */
	    result = _sasl_add_string(&out, &alloclen, &outlen, tempbuf);
	    if (result != SASL_OK)
		goto done;

	    done=1;

	    break;
	  default: 
	    frmt[frmtpos++]=fmt[pos]; /* add to the formating */
	    frmt[frmtpos]=0;	    
	    if (frmtpos>9) 
	      done=1;
	  }
	pos++;
	if (pos>formatlen)
	  done=1;
      }

    }
  }

  out[outlen]=0; /* put 0 at end */

  va_end(ap);    

  /* send log message */
  result = log_cb(log_ctx, level, out);

 done:
  if(out) sasl_FREE(out);
}



/* Allocate and Init a sasl_utils_t structure */
sasl_utils_t *
_sasl_alloc_utils(sasl_conn_t *conn,
		  sasl_global_callbacks_t *global_callbacks)
{
  sasl_utils_t *utils;
  /* set util functions - need to do rest*/
  utils=sasl_ALLOC(sizeof(sasl_utils_t));
  if (utils==NULL)
    return NULL;

  utils->conn = conn;

  sasl_randcreate(&utils->rpool);

  if (conn) {
    utils->getopt = &_sasl_conn_getopt;
    utils->getopt_context = conn;
  } else {
    utils->getopt = &_sasl_global_getopt;
    utils->getopt_context = global_callbacks;
  }

  utils->malloc=_sasl_allocation_utils.malloc;
  utils->calloc=_sasl_allocation_utils.calloc;
  utils->realloc=_sasl_allocation_utils.realloc;
  utils->free=_sasl_allocation_utils.free;

  utils->mutex_alloc = _sasl_mutex_utils.alloc;
  utils->mutex_lock = _sasl_mutex_utils.lock;
  utils->mutex_unlock = _sasl_mutex_utils.unlock;
  utils->mutex_free = _sasl_mutex_utils.free;
  
  utils->MD5Init  = &_sasl_MD5Init;
  utils->MD5Update= &_sasl_MD5Update;
  utils->MD5Final = &_sasl_MD5Final;
  utils->hmac_md5 = &_sasl_hmac_md5;
  utils->hmac_md5_init = &_sasl_hmac_md5_init;
  utils->hmac_md5_final = &_sasl_hmac_md5_final;
  utils->hmac_md5_precalc = &_sasl_hmac_md5_precalc;
  utils->hmac_md5_import = &_sasl_hmac_md5_import;
  utils->mkchal = &sasl_mkchal;
  utils->utf8verify = &sasl_utf8verify;
  utils->rand=&sasl_rand;
  utils->churn=&sasl_churn;  
  utils->checkpass=NULL;
  
  utils->encode64=&sasl_encode64;
  utils->decode64=&sasl_decode64;
  
  utils->erasebuffer=&sasl_erasebuffer;

  utils->getprop=&sasl_getprop;
  utils->setprop=&sasl_setprop;

  utils->getcallback=&_sasl_getcallback;

  utils->log=&_sasl_log;

  utils->seterror=&sasl_seterror;

#ifndef macintosh
  /* Aux Property Utilities */
  utils->prop_new=&prop_new;
  utils->prop_dup=&prop_dup;
  utils->prop_request=&prop_request;
  utils->prop_get=&prop_get;
  utils->prop_getnames=&prop_getnames;
  utils->prop_clear=&prop_clear;
  utils->prop_dispose=&prop_dispose;
  utils->prop_format=&prop_format;
  utils->prop_set=&prop_set;
  utils->prop_setvals=&prop_setvals;
  utils->prop_erase=&prop_erase;
#endif

  /* Spares */
  utils->spare_fptr = NULL;
  utils->spare_fptr1 = utils->spare_fptr2 = 
      utils->spare_fptr3 = NULL;
  
  return utils;
}

int
_sasl_free_utils(const sasl_utils_t ** utils)
{
    sasl_utils_t *nonconst;

    if(!utils) return SASL_BADPARAM;
    if(!*utils) return SASL_OK;

    /* I wish we could avoid this cast, it's pretty gratuitous but it
     * does make life easier to have it const everywhere else. */
    nonconst = (sasl_utils_t *)(*utils);

    sasl_randfree(&(nonconst->rpool));
    sasl_FREE(nonconst);

    *utils = NULL;
    return SASL_OK;
}

int sasl_idle(sasl_conn_t *conn)
{
  if (! conn) {
    if (_sasl_server_idle_hook
	&& _sasl_server_idle_hook(NULL))
      return 1;
    if (_sasl_client_idle_hook
	&& _sasl_client_idle_hook(NULL))
      return 1;
    return 0;
  }

  if (conn->idle_hook)
    return conn->idle_hook(conn);

  return 0;
}

const sasl_callback_t *
_sasl_find_getpath_callback(const sasl_callback_t *callbacks)
{
  static const sasl_callback_t default_getpath_cb = {
    SASL_CB_GETPATH,
    &_sasl_getpath,
    NULL
  };

  if (callbacks)
    while (callbacks->id != SASL_CB_LIST_END)
    {
      if (callbacks->id == SASL_CB_GETPATH)
      {
	return callbacks;
      } else {
	++callbacks;
      }
    }
  
  return &default_getpath_cb;
}

const sasl_callback_t *
_sasl_find_verifyfile_callback(const sasl_callback_t *callbacks)
{
  static const sasl_callback_t default_verifyfile_cb = {
    SASL_CB_VERIFYFILE,
    &_sasl_verifyfile,
    NULL
  };

  if (callbacks)
    while (callbacks->id != SASL_CB_LIST_END)
    {
      if (callbacks->id == SASL_CB_VERIFYFILE)
      {
	return callbacks;
      } else {
	++callbacks;
      }
    }
  
  return &default_verifyfile_cb;
}

/* Basically a conditional call to realloc(), if we need more */
int _buf_alloc(char **rwbuf, size_t *curlen, size_t newlen) 
{
    if(!(*rwbuf)) {
	*rwbuf = sasl_ALLOC(newlen);
	if (*rwbuf == NULL) {
	    *curlen = 0;
	    return SASL_NOMEM;
	}
	*curlen = newlen;
    } else if(*rwbuf && *curlen < newlen) {
	size_t needed = 2*(*curlen);

	while(needed < newlen)
	    needed *= 2;

	*rwbuf = sasl_REALLOC(*rwbuf, needed);
	
	if (*rwbuf == NULL) {
	    *curlen = 0;
	    return SASL_NOMEM;
	}
	*curlen = needed;
    } 

    return SASL_OK;
}

/* for the mac os x cfm glue: this lets the calling function
   get pointers to the error buffer without having to touch the sasl_conn_t struct */
void _sasl_get_errorbuf(sasl_conn_t *conn, char ***bufhdl, unsigned **lenhdl)
{
	*bufhdl = &conn->error_buf;
	*lenhdl = &conn->error_buf_len;
}

/* convert an iovec to a single buffer */
int _iovec_to_buf(const struct iovec *vec,
		  unsigned numiov, buffer_info_t **output) 
{
    unsigned i;
    int ret;
    buffer_info_t *out;
    char *pos;

    if(!vec || !output) return SASL_BADPARAM;

    if(!(*output)) {
	*output = sasl_ALLOC(sizeof(buffer_info_t));
	if(!*output) return SASL_NOMEM;
	memset(*output,0,sizeof(buffer_info_t));
    }

    out = *output;
    
    out->curlen = 0;
    for(i=0; i<numiov; i++)
	out->curlen += vec[i].iov_len;

    ret = _buf_alloc(&out->data, (size_t *)&out->reallen, out->curlen);

    if(ret != SASL_OK) return SASL_NOMEM;
    
    memset(out->data, 0, out->reallen);
    pos = out->data;
    
    for(i=0; i<numiov; i++) {
	memcpy(pos, vec[i].iov_base, vec[i].iov_len);
	pos += vec[i].iov_len;
    }

    return SASL_OK;
}

/* This code might be useful in the future, but it isn't now, so.... */
#if 0
int _sasl_iptostring(const struct sockaddr *addr, socklen_t addrlen,
		     char *out, unsigned outlen) {
    char hbuf[NI_MAXHOST], pbuf[NI_MAXSERV];
    
    if(!addr || !out) return SASL_BADPARAM;

    getnameinfo(addr, addrlen, hbuf, sizeof(hbuf), pbuf, sizeof(pbuf),
		NI_NUMERICHOST | NI_WITHSCOPEID | NI_NUMERICSERV);

    if(outlen < strlen(hbuf) + strlen(pbuf) + 2)
	return SASL_BUFOVER;

    snprintf(out, outlen, "%s;%s", hbuf, pbuf);

    return SASL_OK;
}
#endif

int _sasl_ipfromstring(const char *addr,
		       struct sockaddr *out, socklen_t outlen) 
{
    int i, j;
    struct addrinfo hints, *ai = NULL;
    char hbuf[NI_MAXHOST];
    
    /* A NULL out pointer just implies we don't do a copy, just verify it */

    if(!addr) return SASL_BADPARAM;

    /* Parse the address */
    for (i = 0; addr[i] != '\0' && addr[i] != ';'; i++) {
	if (i >= NI_MAXHOST)
	    return SASL_BADPARAM;
	hbuf[i] = addr[i];
    }
    hbuf[i] = '\0';

    if (addr[i] == ';')
	i++;
    /* XXX: Do we need this check? */
    for (j = i; addr[j] != '\0'; j++)
	if (!isdigit((int)(addr[j])))
	    return SASL_BADPARAM;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;
    if (getaddrinfo(hbuf, &addr[i], &hints, &ai) != 0)
	return SASL_BADPARAM;

    if (out) {
	if (outlen < (unsigned)ai->ai_addrlen) {
	    freeaddrinfo(ai);
	    return SASL_BUFOVER;
	}
	memcpy(out, ai->ai_addr, ai->ai_addrlen);
    }

    freeaddrinfo(ai);

    return SASL_OK;
}

int _sasl_build_mechlist(void) 
{
    int count = 0;
    sasl_string_list_t *clist = NULL, *slist = NULL, *olist = NULL;
    sasl_string_list_t *p, *q, **last, *p_next;

    clist = _sasl_client_mechs();
    slist = _sasl_server_mechs();

    if(!clist) {
	olist = slist;
    } else {
	int flag;
	
	/* append slist to clist, and set olist to clist */
	for(p = slist; p; p = p_next) {
	    flag = 0;
	    p_next = p->next;

	    last = &clist;
	    for(q = clist; q; q = q->next) {
		if(!strcmp(q->d, p->d)) {
		    /* They match, set the flag */
		    flag = 1;
		    break;
		}
		last = &(q->next);
	    }

	    if(!flag) {
		(*last)->next = p;
		p->next = NULL;
	    } else {
		sasl_FREE(p);
	    }
	}

	olist = clist;
    }

    if(!olist) {
	printf ("no olist");
	return SASL_FAIL;
    }

    for (p = olist; p; p = p->next) count++;
    
    if(global_mech_list) {
	sasl_FREE(global_mech_list);
	global_mech_list = NULL;
    }
    
    global_mech_list = sasl_ALLOC((count + 1) * sizeof(char *));
    if(!global_mech_list) return SASL_NOMEM;
    
    memset(global_mech_list, 0, (count + 1) * sizeof(char *));
    
    count = 0;
    for (p = olist; p; p = p_next) {
	p_next = p->next;

	global_mech_list[count++] = p->d;

    	sasl_FREE(p);
    }

    return SASL_OK;
}

const char ** sasl_global_listmech(void) 
{
    return global_mech_list;
}

int sasl_listmech(sasl_conn_t *conn,
		  const char *user,
		  const char *prefix,
		  const char *sep,
		  const char *suffix,
		  const char **result,
		  unsigned *plen,
		  int *pcount)
{
    if(!conn) {
	return SASL_BADPARAM;
    } else if(conn->type == SASL_CONN_SERVER) {
	RETURN(conn, _sasl_server_listmech(conn, user, prefix, sep, suffix,
					   result, plen, pcount));
    } else if (conn->type == SASL_CONN_CLIENT) {
	RETURN(conn, _sasl_client_listmech(conn, prefix, sep, suffix,
					   result, plen, pcount));
    }
    
    PARAMERROR(conn);
}
