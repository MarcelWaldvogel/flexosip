#include "flexosip.h"
#include "flexosnd.h"
#include "flexortp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ini.h>
#include "unused.h"
#include <signal.h>

#define CONFIG1 "/etc/cowbell.ini"
#define CONFIG2 "cowbell.ini"

// Registration parameters
static char *uri, *registrar, *login, *password;
// Call parameters
static char *destination, *name;


static int handle_ini(void* UNUSED_PARAM(user), const char* section,
		      const char* name, const char* value)
{
#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
    if (MATCH("registration", "uri")) {
        uri = strdup(value);
    } else if (MATCH("registration", "registrar")) {
        registrar = strdup(value);
    } else if (MATCH("registration", "login")) {
        login = strdup(value);
    } else if (MATCH("registration", "password")) {
        password = strdup(value);
    } else if (MATCH("alert", "destination")) {
        destination = strdup(value);
    } else if (MATCH("alert", "name")) {
        name = strdup(value);
    } else {
    	fprintf(stderr, "Unknown config option [%s] %s=%s\n", section, name, value);
        return 0;  /* unknown section/name, error */
    }
    return 1;
}

static char dtmf;

int main(int UNUSED_PARAM(argc), char **UNUSED_PARAM(argv))
{
#ifdef ENABLE_TRACE
    TRACE_INITIALIZE(OSIP_INFO1, NULL);
#endif

  if (ini_parse(CONFIG1, handle_ini, NULL) < 0 &&
      ini_parse(CONFIG2, handle_ini, NULL)) {
    fprintf(stderr, "Cannot load either " CONFIG1 " or " CONFIG2 "\n");
    return 1;
  }
  if (uri == NULL || registrar == NULL || login == NULL || password == NULL ||
      destination == NULL) {
    fprintf(stderr, "Missing configuration value\n");
    return 1;
  }
  signal(SIGINT, fesip_cleanup);
  signal(SIGHUP, fesip_cleanup);
  signal(SIGTERM, fesip_cleanup);
  signal(SIGQUIT, fesip_cleanup);
  fesip_listen(IPPROTO_UDP, false, 0);
  fesip_register(uri, registrar, login, password);
  int i = fesip_wait_registered();
  if (i < 0) {
    fprintf(stderr, "Registration failed: %d\n", i);
    exit(1);
  } else {
    fprintf(stderr, "Registration succeeded, continuing...\n");
  }
  fesip_call(uri, destination, name, NULL);
  while (1) {
    fesip_handle_event();
    if (dtmf != '\0') {
      fprintf(stderr, "Echoback %c\n", dtmf);
      sleep(1);
      fprintf(stderr, "Echoback %c\n", dtmf);
      fesip_send_dtmf(dtmf);
      dtmf = '\0';
    }
  }
}

void fesip_event_answered(eXosip_event_t *UNUSED_PARAM(evt),
    const char *remote_host, int port, int format)
{
  extern PayloadType payload_type_pcma16000;
  // if the format is dynamic, the payload type will always be PCMA/16000
  // (as long as we just support PCMA/8000 and PCMA/16000)
  fertp_start(remote_host, port, format, &payload_type_pcma16000);
  fesip_play("media/test.ogg");
  fesip_play("media/test.ogg");
}

void fesip_event_terminate(eXosip_event_t *UNUSED_PARAM(evt))
{
  fprintf(stderr, "Call terminated, exiting\n");
  eXosip_unlock(fesip_ctx());

  exit(0);
}

void fesip_event_dtmf(char c)
{
  fprintf(stderr, "Pressed %c\n", c);
  dtmf = c;
}
