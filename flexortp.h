#include <stddef.h>
#include <ortp/payloadtype.h>
/**
 * Start an RTP session
 *
 * @param host		The remote host's address
 * @param port		The remote host's port
 * @param format	The payload type
 * @param pt		For user-defined payload types (>=96), the description
 */
void fertp_start(const char *host, int port, int format, PayloadType *pt);
void fertp_resume(void);
void fertp_send_alaw(const unsigned char *buf, ssize_t nbytes, ssize_t nsamples);
void fertp_stop(void);

