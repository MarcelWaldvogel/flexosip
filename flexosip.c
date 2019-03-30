#define _GNU_SOURCE // For strcasestr()
#include <string.h>
#include "flexosip.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <time.h>
#include "unused.h"
#include <stdbool.h>
#include "flexosnd.h"
#include "flexortp.h"
#include <assert.h>

#define REGISTRATION_WAIT 15 // By when it should be successful
#define REGISTRATION_TIMEOUT 1800 // How often to refresh
#define RTP_PORT 5070 // Default port number

//#define TRY_PCMA16000

#define SIP_RINGING 180
#define SIP_BUSY 486

static struct eXosip_t *ctx;
static int cid = -1, did = -1, tid = -1;
static int quit_registered;
static int rtp_port=RTP_PORT;
static _Bool call_in_progress = false;
static _Bool clean_up_please = false;

struct eXosip_t *fesip_ctx(void)
{
  if (ctx == NULL) {
    // alloc
    ctx = eXosip_malloc();
    if (ctx != NULL) {
      // init
      ortp_init();
      if (eXosip_init(ctx) != 0) {
	fprintf(stderr, "flexosip: Could not initialize ctx\n");
	eXosip_quit(ctx);
	ctx = NULL;
      } else {
	// Renew it on every initialization
	cid = did = -1;
	// register dealloc only once
	if (!quit_registered) {
	  atexit(fesip_quit);
	  quit_registered++;
	}
      }
    }
  }
  return ctx;
}

void fesip_quit(void)
{
  if (ctx != NULL) {
    fesip_terminate_nolock();
    eXosip_quit(ctx);
  }
  ctx = NULL;
  cid = did = -1;
  ortp_exit();
}

int fesip_listen(int proto, int secure, int port)
{
  fesip_ctx();
  if (port == 0) {
    port = 5060 + !!secure;
  }
  int i = eXosip_listen_addr (ctx, proto, NULL, port, AF_INET, secure);
  if (i != 0)
  {
    eXosip_quit(ctx);
    fprintf(stderr, "flexosip: Could not initialize transport layer\n");
    return -1;
  }
  return 0;
}

int fesip_register(const char *url,
		   const char *registrar,
		   const char *login,
		   const char *password)
{
  fesip_ctx();
  osip_message_t *reg = NULL;
  int rid;
  int i;
  eXosip_lock(ctx);
  rid = eXosip_register_build_initial_register(ctx, url, registrar, NULL, REGISTRATION_TIMEOUT, &reg);
  if (rid < 0) {
    eXosip_unlock(ctx);
    fprintf(stderr, "eXosip_register_build_initial_register() failed with %d\n", rid);
    return rid;
  }
  // specifying the registrar here does not seem to work, as the
  // registrar in eXosip_find_authentication_info() includes the double quotes
  eXosip_add_authentication_info(ctx, login, login, password, NULL, NULL);

  osip_message_set_supported(reg, "100rel");
  osip_message_set_supported(reg, "path");
  i = eXosip_register_send_register(ctx, rid, reg);
  eXosip_unlock(ctx);
  if (i < 0) {
    fprintf(stderr, "eXosip_register_send_register() failed with %d\n", i);
    return i;
  } else {
    return rid;
  }
}

int fesip_wait_registered(void)
{
  int tries=1;
  int timeout=REGISTRATION_WAIT;
  eXosip_event_t *evt;
  for (; timeout > 0; timeout--) {
    evt = fesip_wait_event(1, 0);
    if (evt != NULL) {
      if (evt->type == EXOSIP_REGISTRATION_FAILURE) {
	tries--;
	if (tries < 0)
	  return OSIP_NO_RIGHTS;
      } else if (evt->type == EXOSIP_REGISTRATION_SUCCESS) {
	return OSIP_SUCCESS;
      }
    }
  }
  return OSIP_TIMEOUT;
}

int fesip_unregister(int rid)
{
  fesip_ctx();
  osip_message_t *reg = NULL;
  int i;
  eXosip_lock(ctx);
  i = eXosip_register_build_register(ctx, rid, 0, &reg);
  if (i < 0) {
    eXosip_unlock(ctx);
    return -1;
  }
  eXosip_register_send_register(ctx, rid, reg);
  eXosip_unlock(ctx);
  return 0;
}

static const char *fesip_strevent(int event)
{
  static char buf[100]; // Not thread safe
  
  switch (event) {
  /* REGISTER related events */
  case EXOSIP_REGISTRATION_SUCCESS:		return "REGISTER: user is successfully registered";
  case EXOSIP_REGISTRATION_FAILURE:		return "REGISTER: user is not registered";
  
  /* INVITE related events within calls */
  case EXOSIP_CALL_INVITE: 			return "INVITE: new call";
  case EXOSIP_CALL_REINVITE: 			return "INVITE: new INVITE within call";
  
  case EXOSIP_CALL_NOANSWER: 			return "INVITE: no answer within the timeout";
  case EXOSIP_CALL_PROCEEDING:			return "INVITE: processing by a remote app";
  case EXOSIP_CALL_RINGING:			return "INVITE: ringback";
  case EXOSIP_CALL_ANSWERED:			return "INVITE: start of call";
  case EXOSIP_CALL_REDIRECTED:			return "INVITE: redirection";
  case EXOSIP_CALL_REQUESTFAILURE:		return "INVITE: request failure";
  case EXOSIP_CALL_SERVERFAILURE:		return "INVITE: server failure";
  case EXOSIP_CALL_GLOBALFAILURE:		return "INVITE: global failure";
  case EXOSIP_CALL_ACK:				return "INVITE: ACK received for 200ok to INVITE";
  
  case EXOSIP_CALL_CANCELLED:			return "CALL: that call has been cancelled";
  
  /* request related events within calls (except INVITE) */
  case EXOSIP_CALL_MESSAGE_NEW:			return "MESSAGE: new incoming request";
  case EXOSIP_CALL_MESSAGE_PROCEEDING:		return "MESSAGE: 1xx for request";
  case EXOSIP_CALL_MESSAGE_ANSWERED:		return "MESSAGE: 200ok";
  case EXOSIP_CALL_MESSAGE_REDIRECTED:		return "MESSAGE: failure";
  case EXOSIP_CALL_MESSAGE_REQUESTFAILURE:	return "MESSAGE: request failure";
  case EXOSIP_CALL_MESSAGE_SERVERFAILURE:	return "MESSAGE: server failure";
  case EXOSIP_CALL_MESSAGE_GLOBALFAILURE:	return "MESSAGE: global failure";
  
  case EXOSIP_CALL_CLOSED:			return "BYE: received for this call";
  
  /* for both UAS & UAC events */
  case EXOSIP_CALL_RELEASED:			return "CALL: call context is cleared";
  
  /* events received for request outside calls */
  case EXOSIP_MESSAGE_NEW:			return "OUTSIDE: new incoming request";
  case EXOSIP_MESSAGE_PROCEEDING:		return "OUTSIDE: 1xx for request";
  case EXOSIP_MESSAGE_ANSWERED:			return "OUTSIDE: 200ok";
  case EXOSIP_MESSAGE_REDIRECTED:		return "OUTSIDE: failure";
  case EXOSIP_MESSAGE_REQUESTFAILURE:		return "OUTSIDE: request failure";
  case EXOSIP_MESSAGE_SERVERFAILURE:		return "OUTSIDE: server failure";
  case EXOSIP_MESSAGE_GLOBALFAILURE:		return "OUTSIDE: global failure";
  
  /* Presence and Instant Messaging */
  case EXOSIP_SUBSCRIPTION_NOANSWER:		return "IM: no answer";
  case EXOSIP_SUBSCRIPTION_PROCEEDING:		return "IM: 1xx";
  case EXOSIP_SUBSCRIPTION_ANSWERED:		return "IM: 200ok";
  case EXOSIP_SUBSCRIPTION_REDIRECTED:		return "IM: redirection";
  case EXOSIP_SUBSCRIPTION_REQUESTFAILURE:	return "IM: request failure";
  case EXOSIP_SUBSCRIPTION_SERVERFAILURE:	return "IM: server failure";
  case EXOSIP_SUBSCRIPTION_GLOBALFAILURE:	return "IM: global failure";
  case EXOSIP_SUBSCRIPTION_NOTIFY:		return "IM: new NOTIFY request";
  
  case EXOSIP_IN_SUBSCRIPTION_NEW:		return "IM: new incoming SUBSCRIBE/REFER";
  
  case EXOSIP_NOTIFICATION_NOANSWER:		return "NOTIFY: no answer";
  case EXOSIP_NOTIFICATION_PROCEEDING:		return "NOTIFY: 1xx";
  case EXOSIP_NOTIFICATION_ANSWERED:		return "NOTIFY: 200ok";
  case EXOSIP_NOTIFICATION_REDIRECTED:		return "NOTIFY: redirection";
  case EXOSIP_NOTIFICATION_REQUESTFAILURE:	return "NOTIFY: request failure";
  case EXOSIP_NOTIFICATION_SERVERFAILURE:	return "NOTIFY: server failure";
  case EXOSIP_NOTIFICATION_GLOBALFAILURE:	return "NOTIFY: global failure";

  default: // Not thread safe
    snprintf(buf, sizeof(buf), "Unrecognized event %d", event);
    return buf;
  }
}

#define HOSTLEN 128
static char remote_host[HOSTLEN];
static int remote_port;
static int payload_format;
static int codec_samples;
static char *codec_name;
#ifdef TRY_PCMA16000
// Use 96, but adapt to remote side (for beauty only)
static int pcma16000_payload_format = 96;
#endif

/**
 * Get address of remote connection
 *
 * (proto, addr) options:
 * - (---, NULL):        No entry
 * - ("other", "other"): Non-IP
 * - ("IP4", ip4addr):   IPv4 address
 * - ("IP6", ip6addr):   IPv6 address
 *
 * @param sdp		The SDP message to analyze
 * @param pos_media	Which media entry (-1=global)
 * @param pos		Which connection entry in the media
 * @param proto		Set to the proto to be used
 */
static char *fesip_connection(sdp_message_t *sdp, int pos_media, int pos,
	char **proto)
{
  char *net;
  if ((net = sdp_message_c_nettype_get(sdp, pos_media, pos)) == NULL
   && pos_media >= 0 && pos == 0) {
    // No per-media connection at all, revert to global connection
    // for the first media connection
    return fesip_connection(sdp, -1, 0, proto);
  }
  // Non-IP: proto, addr = "other"
  if (strcmp(net, "IN") != 0) {
    *proto = "other";
    return *proto;
  }
  // "IP4" or "IP6"
  *proto = sdp_message_c_addrtype_get(sdp, pos_media, pos);
  return sdp_message_c_addr_get(sdp, pos_media, pos);
}

/**
 * Find a rtpmap entry with the requested codec and return
 * its RTP payload type.
 * 
 * Returns -1 if none found.
 * 
 * @param sdp		The SDP message to analyze
 * @param pos_media	Which media entry to scan for this codec
 * @param codec		The codec to look for
 */
static int fesip_has_format(sdp_message_t *sdp, int pos_media,
      const char *codec)
{
  int pos = 0;
  char *field, *value;
  int payload;
  
  while ((field = sdp_message_a_att_field_get(sdp, pos_media, pos)) != NULL) {
    if (strcmp(field, "rtpmap") == 0) {
      value = sdp_message_a_att_value_get(sdp, pos_media, pos);
      // Look for "<payload-type-num> <codec-str>"
      char *end;
      payload = strtoul(value, &end, 10);
      if (end != value // Number found
       && *end == ' ') {
	if (strcmp(end + 1, codec) == 0) {
	  return payload;
	}
      }
    }
    pos++;
  }
  return -1;
}

int fesip_remote_params(osip_message_t *msg)
{
  sdp_message_t *sdp = eXosip_get_sdp_info(msg);
  int pos_media = 0;
  const char *media;
  
  if (sdp == NULL)
    return 0;

  while ((media = sdp_message_m_media_get(sdp, pos_media)) != NULL) {
    if (strcmp(media, "audio") == 0
     && strcmp(sdp_message_m_proto_get(sdp, pos_media), "RTP/AVP") == 0) {
      remote_port = atoi(sdp_message_m_port_get(sdp, pos_media));

      // Look for an IPv4 address
      int pos = 0;
      char *addr, *proto;
      remote_host[0] = '\0';
      while ((addr = fesip_connection(sdp, pos_media, pos, &proto)) != NULL) {
	if (strcmp(proto, "IP4") == 0 && strlen(addr) < HOSTLEN) {
	  strncpy(remote_host, addr, HOSTLEN);
	  break;
	}
	pos++;
      }
      if (remote_host[0] == '\0') {
	return 0;
      }

      // Any supported codec?
#ifdef TRY_PCMA16000
      payload_format = fesip_has_format(sdp, pos_media, "PCMA/16000");
      if (payload_format >= 0) {
	codec_name = "PCMA/16000";
	codec_samples = 320;
	pcma16000_payload_format = payload_format;
	return payload_format;
      }
#endif
      payload_format = fesip_has_format(sdp, pos_media, "PCMA/8000");
      if (payload_format >= 0) {
	codec_name = "PCMA/8000";
	codec_samples = 160;
	return payload_format;
      }
      // XXX Hack, fall back to PCMA/8000, even if no rtpmap entry exists for it
      // (Fritz!Boxes announce it in the m= header, but not in a=rtpmap:)
      fprintf(stderr, "Falling back to unannounced PCMA/8000\n");
      codec_name = "PCMA/8000";
      codec_samples = 160;
      payload_format = 8;
      return payload_format;
    }
    pos_media++;
  }
  return 0;
}

static _Bool is_playing = false;

void fesip_play_after_delay(int delay, const char *filename)
{
  fesnd_add_after_delay(delay, filename);
  if (!is_playing) {
    is_playing = true;
    fertp_resume();
  }
}

void fesip_play(const char *filename)
{
  fesip_play_after_delay(0, filename);
}

static void fesip_build_sdp(osip_message_t *invite)
{
  char tmp[4096];
  char lenstr[100];
  char localip4[128], localip6[128];
  eXosip_guess_localip(ctx, AF_INET, localip4, 128);
  eXosip_guess_localip(ctx, AF_INET6, localip6, 128);
  snprintf(tmp, sizeof(tmp),
	    "v=0\r\n"
	    "o=cowbell 0 0 IN IP4 %s\r\n"
	    "s=call\r\n"
	    "c=IN IP4 %s\r\n"
	    "t=0 0\r\n"
#ifdef TRY_PCMA16000
	    "m=audio %d RTP/AVP 8 %d\r\n"
#else
	    "m=audio %d RTP/AVP 8\r\n"
#endif
	    "a=rtpmap:8 PCMA/8000\r\n"
#ifdef TRY_PCMA16000
	    "a=rtpmap:%d PCMA/16000\r\n"
#endif
	    ,
	    localip4, //localip6,
	    localip4, //localip6,
	    rtp_port
#ifdef TRY_PCMA16000
	    ,
	    pcma16000_payload_format,
	    pcma16000_payload_format
#endif
	    );
  osip_message_set_body(invite, tmp, strlen(tmp));
  snprintf(lenstr, sizeof(lenstr), "%zd", strlen(tmp));
  osip_message_set_content_length(invite, lenstr);
  osip_message_set_content_type(invite, "application/sdp");
}

void fesip_answer(void)
{
  extern PayloadType payload_type_pcma16000;
  int i;
  osip_message_t *answer = NULL;
  fesip_ctx();
  eXosip_lock(ctx);

  i = eXosip_call_build_answer(ctx, tid, 200, &answer);
  if (i != 0) {
    eXosip_call_send_answer(ctx, tid, 400, NULL);
  } else {
    fesip_build_sdp(answer);
    eXosip_call_send_answer(ctx, tid, 200, answer);
  }
  // if the format is dynamic, the payload type will always be PCMA/16000
  // (as long as we just support PCMA/8000 and PCMA/16000)
  fertp_start(remote_host, remote_port, payload_format, &payload_type_pcma16000);
  eXosip_unlock(ctx);
}

eXosip_event_t *fesip_wait_event(int seconds, int milliseconds)
{
  fesip_ctx();
  eXosip_event_t *evt = eXosip_event_wait(ctx, seconds, milliseconds);
  if (clean_up_please) {
    fprintf(stderr, "Cleaning up...\n");
    fesip_terminate();
    fesip_quit();
    exit(0);
  }
  eXosip_lock(ctx);
  eXosip_automatic_action(ctx);
  if (evt != NULL) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch" // We do not handle all enumerations
    switch (evt->type)
    {
    case EXOSIP_CALL_INVITE:
      if (call_in_progress) {
	// Already an existing call: Terminate incoming call and drop the event
	eXosip_call_send_answer(ctx, evt->tid, SIP_BUSY, NULL);
	evt = NULL;
      } else {
	if (fesip_remote_params(evt->request)) {
	  tid = evt->tid; // For fesip_answer()
	  int code = fesip_event_invite(evt, remote_host, remote_port, payload_format);
	  // Should be SIP_RINGING or SIP_BUSY_HERE
	  // Returning SIP_OK directly will cause problems
	  eXosip_call_send_answer(ctx, evt->tid, code, NULL);
	} else {
	  eXosip_call_send_answer(ctx, evt->tid, SIP_NOT_ACCEPTABLE_HERE, NULL);
	}
      }
      break;
    case EXOSIP_CALL_ANSWERED:
      if (fesip_remote_params(evt->response)) {
	fprintf(stderr, "Call answered with req=%p, resp=%p, ack=%p!\n",
		evt->request, evt->response, evt->ack);
	eXosip_call_build_ack(ctx, evt->did, &evt->ack);
	eXosip_call_send_ack(ctx, evt->did, evt->ack);
	did = evt->did;
	cid = evt->cid;
	call_in_progress = true;
	fesip_event_answered(evt, remote_host, remote_port, payload_format);
      }
      break;
    case EXOSIP_CALL_NOANSWER:
    case EXOSIP_CALL_REQUESTFAILURE: // Error in the request
    case EXOSIP_CALL_GLOBALFAILURE: // Also when BUSYing the call
    case EXOSIP_CALL_CANCELLED:
    case EXOSIP_CALL_CLOSED:
    case EXOSIP_CALL_RELEASED:
      fprintf(stderr, "Closing request %s\n", fesip_strevent(evt->type));
      if (call_in_progress &&
          !(evt->type == EXOSIP_CALL_REQUESTFAILURE
	    && evt->response->status_code == SIP_UNAUTHORIZED)) {
	// Probably too eager
	if (evt->response != NULL) {
	  fprintf(stderr, "Terminating call because of response %s (%d)\n",
		  fesip_strevent(evt->type), evt->response->status_code);
	} else if (evt->request != NULL) {
	  fprintf(stderr, "Terminating call because of request %s (%d)\n",
		  fesip_strevent(evt->type), evt->request->status_code);
	} else {
	  // Should never happen…
	  fprintf(stderr, "Terminating call because of unexpected %s\n",
		  fesip_strevent(evt->type));
	}
	fesip_event_terminate(evt);
	fesip_terminate_nolock();
      }
      break;
    case EXOSIP_CALL_MESSAGE_NEW:
      if (strcasecmp(evt->request->sip_method, "INFO") == 0 &&
	  strcmp(evt->request->content_type->type, "application") == 0 &&
	  strcmp(evt->request->content_type->subtype, "dtmf-relay") == 0) {
	osip_list_iterator_t it;
        osip_body_t *body = (osip_body_t *)osip_list_get_first(&evt->request->bodies, &it);
	char *match = strcasestr(body->body, "Signal=");
	if (match != NULL && match[7] != '\0') {
	  fesip_event_dtmf(match[7]);
	}
	eXosip_call_build_ack(ctx, evt->did, &evt->ack);
	eXosip_call_send_ack(ctx, evt->did, evt->ack);
      }
      break;
    default:
      fprintf(stderr, "Received event %s\n", fesip_strevent(evt->type));
      break;
    }
#pragma GCC diagnostic pop
  }
  eXosip_unlock(ctx);
  return evt;
}

eXosip_event_t *fesip_handle_event(void)
{
  eXosip_event_t *evt = fesip_wait_event(0, 10); // Shorter than 20ms inter-packet time
  if (is_playing) {
    short buf[ALAW16K_BUF20MS];
    assert(codec_samples <= ALAW16K_BUF20MS);
    ssize_t nsamples = fesnd_read(buf, ALAW16K_BUF20MS);
    if (nsamples > 0) {
      unsigned char alawbuf[ALAW16K_BUF20MS];
      if (codec_samples != ALAW16K_BUF20MS) {
	// Need to downsample
	fesnd_encode_alaw(alawbuf, buf, nsamples, true);
	fertp_send_alaw(alawbuf, nsamples/2, nsamples/2);
      } else {
	fesnd_encode_alaw(alawbuf, buf, nsamples, false);
	fertp_send_alaw(alawbuf, nsamples, nsamples);
      }
    } else {
      is_playing = false;
    }
  }
  return evt;
}

int fesip_call(const char *from, const char *to, const char *subject,
	       void *reference)
{
  osip_message_t *invite;
  int i;

  fesip_ctx();
  // Already a call in progress?
  eXosip_lock(ctx);
  if (did >= 0 || cid >= 0 || call_in_progress) {
    OSIP_TRACE(osip_trace(__FILE__, __LINE__, OSIP_ERROR, NULL,
			  "fesip_call() while call is already active\r\n"));
    eXosip_unlock(ctx);
    return OSIP_TOOMUCHCALL;
  }
  i = eXosip_call_build_initial_invite(ctx, &invite, to, from,
				       NULL, // optional route header
				       subject);
  if (i != 0) {
    return -1;
  }
  osip_message_set_supported(invite, "100rel");
  osip_message_set_header(invite, "Alert-Info", "<urn:alert:source:external>");

  cid = eXosip_call_send_initial_invite(ctx, invite);
  if (cid > 0) {
    eXosip_call_set_reference(ctx, cid, reference);
    call_in_progress = true;
  }
  eXosip_unlock(ctx);
  return cid;
}

int fesip_ringback(eXosip_event_t *evt)
{
  fesip_ctx();
  eXosip_lock(ctx);
  eXosip_call_send_answer(ctx, evt->tid, SIP_RINGING, NULL);
  eXosip_unlock(ctx);
  return 0;
}

int fesip_terminate(void)
{
  fesip_ctx();
  eXosip_lock(ctx);
  fesip_terminate_nolock();
  eXosip_unlock(ctx);
  return 0;
}

void fesip_terminate_nolock(void)
{
  if (cid >= 0 || did >= 0) {
    eXosip_call_terminate(ctx, cid, did);
  }
  call_in_progress = false;
  cid = did = -1;
}

void fesip_send_dtmf(char digit)
{
  fesip_ctx();
  osip_message_t *info;
  char dtmf_body[1000];
  int i;

  if (did < 0) {
    OSIP_TRACE(osip_trace(__FILE__, __LINE__, OSIP_ERROR, NULL,
			  "No call to send DTMF digit\r\n"));
  }
  eXosip_lock(ctx);
  i = eXosip_call_build_info(ctx, did, &info);
  if (i == 0)
  {
     snprintf(dtmf_body, 999, "Signal=%c\r\nDuration=250\r\n", digit);
     osip_message_set_content_type(info, "application/dtmf-relay");
     osip_message_set_body(info, dtmf_body, strlen (dtmf_body));
     i = eXosip_call_send_request(ctx, did, info);
  }
  eXosip_unlock(ctx);
}

void fesip_cleanup(int sig)
{
  fprintf(stderr, "Received signal %d; repeat if we don't exit now...\n", sig);
  signal(sig, SIG_DFL); // Next signal of the same type will kill the program ungracefully
  clean_up_please = true;
}

// Weak methods, ready to be overridden

void __attribute__((weak)) fesip_event_answered(eXosip_event_t *UNUSED_PARAM(evt),
    const char *UNUSED_PARAM(host), int UNUSED_PARAM(port), int UNUSED_PARAM(format))
{
  OSIP_TRACE(osip_trace(__FILE__, __LINE__, OSIP_INFO3, NULL,
			"answered call\r\n"));
}

int __attribute__((weak)) fesip_event_invite(eXosip_event_t *UNUSED_PARAM(evt),
    const char *UNUSED_PARAM(host), int UNUSED_PARAM(port), int UNUSED_PARAM(format))
{
  OSIP_TRACE(osip_trace(__FILE__, __LINE__, OSIP_INFO3, NULL,
			"invite (incoming call)\r\n"));
  return SIP_RINGING;
}

void __attribute__((weak)) fesip_event_terminate(eXosip_event_t *UNUSED_PARAM(evt))
{
  OSIP_TRACE(osip_trace(__FILE__, __LINE__, OSIP_INFO3, NULL,
			"Terminate existing call\r\n"));
}
void __attribute__((weak)) fesip_event_dtmf(char digit)
{
  OSIP_TRACE(osip_trace(__FILE__, __LINE__, OSIP_INFO3, NULL,
			"received DTMF digit %c\r\n", digit));
}
