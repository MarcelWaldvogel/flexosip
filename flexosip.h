#include <eXosip2/eXosip.h>
#include <ortp/ortp.h>
#include <stdbool.h>

#define ALAW8K_BUF20MS 160 // 160 bytes=160 samples≡20 ms (with A-Law 8 kHz)
#define ALAW16K_BUF20MS 320 // 320 bytes=320 samples≡20 ms (with A-Law 16 kHz)

/**
 * Obtain the context handle
 *
 * NULL means error
 */
struct eXosip_t *fesip_ctx(void);

/**
 * Terminate all eXosip activity
 * (does not need to be called explicitely)
 */
void fesip_quit(void);

/**
 * Listen on a port
 *
 * @param proto		IPPROTO_UDP or IPPROTO_TCP
 * @param secure	TRUE or FALSE
 * @param port          0 (automatic) or a fixed port number
 */
int fesip_listen(int proto, int secure, int port);

/**
 * Register a session
 *
 * @param url		The SIP user's URL, e.g. "sip:User Name <user.name@example.com>"
 * @param registrar	Host name of the registrar
 * @param login		The user's login
 * @param password	The user's password
 */
int fesip_register(const char *url,
		   const char *registrar,
		   const char *login,
		   const char *password);

/**
 * Unregister a session
 *
 * @param rid		A registration ID received by a previous fesip_register()
 */
int fesip_unregister(int rid);

/**
 * Wait for the registration to succeed
 *
 * This works only if this is the first and only registration.
 */
int fesip_wait_registered(void);

/**
 * Wait for an event, but at most the specified time
 *
 * NULL means no (interesting) event happened.
 *
 * @param seconds	Full seconds
 * @param milliseconds	Milliseconds
 */
eXosip_event_t *fesip_wait_event(int seconds, int milliseconds);

/**
 * Send audio chunk, if any, and wait for next event.
 * Return in time to be able to send another audio chunk.
 */
eXosip_event_t *fesip_handle_event(void);

/**
 * Initiate a call
 *
 * @param from		SIP source URL
 * @param to		SIP destination URL
 * @param subject	SIP subject (if desired)
 * @param reference	Application reference (if desired)
 */
int fesip_call(const char *from, const char *to, const char *subject,
	       void *reference);

/**
 * Tell the other party it is ringing here now (in response to an
 * incoming call)
 *
 * @param evt		The EXOSIP_CALL_NEW event received
 */
int fesip_ringback(eXosip_event_t *evt);

/**
 * Answer the incoming call
 */
void fesip_answer(void);

/**
 * Terminate the call:
 * - Tell the other party it is busy here (in response to an
 *   incoming call)
 * - Hangup (with ongoing call)
 */
int fesip_terminate(void);

/**
 * Terminate the call
 *
 * To be called from within an event handler
 */
void fesip_terminate_nolock(void);

/**
 * Send a file or add to FIFO queue
 *
 * @param filename	Path to WAV file (16kHz mono)
 */
void fesip_play(const char *filename);

/**
 * Send a file or add to FIFO queue
 *
 * @param milliseconds	Delay to insert before the file
 * @param filename	Path to WAV file (16kHz mono)
 */
void fesip_play_after_delay(int milliseconds, const char *filename);

/**
 * Send a DTMF digit
 *
 * @param did		The dialog id
 * @param digit		The digit to send ('0'…'9', '*', '#')
 */
void fesip_send_dtmf(char digit);

/**
 * Event handler, ready to be overridden by application
 * (weak symbol)
 *
 * @param evt		The answering event
 */
void fesip_event_answered(eXosip_event_t *evt,
    const char *host, int port, int format);

/**
 * Event handler, ready to be overridden by application
 * (weak symbol)
 *
 * Returns the SIP code to be returned to the inviter.
 *
 * @param evt		The invite event
 */
int fesip_event_invite(eXosip_event_t *evt,
    const char *host, int port, int format);

/**
 * Event handler, ready to be overridden by application
 * (weak symbol)
 *
 * @param evt		The terminating event
 */
void fesip_event_terminate(eXosip_event_t *evt);

/**
 * Event handler, ready to be overridden by application
 * (weak symbol)
 *
 * @param digit		The digit received
 */
void fesip_event_dtmf(char digit);

/**
 * Signal handler for cleanup
 *
 * @param sig		Signal it was called from
 */
void fesip_cleanup(int sig);
