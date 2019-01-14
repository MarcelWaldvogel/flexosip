#include "flexortp.h"
#include <ortp/ortp.h>
#include <ortp/payloadtype.h>

static RtpSession *session = NULL;
static int user_ts = 0;

extern char offset0xD5;
PayloadType payload_type_pcma16000={
        .type = PAYLOAD_AUDIO_CONTINUOUS,
        .clock_rate = 16000,
        .bits_per_sample = 8,
        .zero_pattern = &offset0xD5,
        .pattern_length = 1,
        .normal_bitrate = 128000,
        .mime_type = "PCMA",
        .channels = 1,
        .recv_fmtp = NULL,
        .send_fmtp = NULL,
#ifdef PAYLOAD_TYPE_AVPF_NONE
        .avpf={.features=PAYLOAD_TYPE_AVPF_NONE, .trr_interval=0},
#endif
        .flags = 0
};

void fertp_start(const char *host, int port, int format,
		 PayloadType *pt)
{
  ortp_scheduler_init();
  // Difference between the Ubuntu 18.10 bundled version
  // "libortp9 (= 3.6.1-4build1)" and the git repo version
  // "0.27.0"
#ifdef ORTP_LOG_DOMAIN
  ortp_set_log_level_mask(NULL, ORTP_MESSAGE|ORTP_WARNING|ORTP_ERROR);
#else
  ortp_set_log_level_mask(ORTP_MESSAGE|ORTP_WARNING|ORTP_ERROR);
#endif
  session=rtp_session_new(RTP_SESSION_SENDONLY);	

  rtp_session_set_scheduling_mode(session, 1);
  rtp_session_set_blocking_mode(session, 1);
  rtp_session_set_connected_mode(session, TRUE);
  rtp_session_set_local_addr(session, "0.0.0.0", 5070, -1);
  rtp_session_set_remote_addr(session, host, port);
  if (format >= 96) {
    // User-defined payload
    static RtpProfile *myProfile;
    if (myProfile == NULL) {
      myProfile = rtp_profile_clone(&av_profile);
      rtp_profile_set_name(myProfile, "withPCMA/1600");
    }
    rtp_profile_set_payload(myProfile, format, pt);
    rtp_session_set_profile(session, myProfile);
    fprintf(stderr, "My Profile payload %d %s/%d\n", format, myProfile->payload[format]->mime_type, myProfile->payload[format]->clock_rate);
  } else {
    fprintf(stderr, "AV Profile payload %d %s/%d\n", format, av_profile.payload[format]->mime_type, av_profile.payload[format]->clock_rate);
  }
  rtp_session_set_payload_type(session, format);
  fertp_resume();
}

void fertp_resume(void)
{
  user_ts = rtp_session_get_current_send_ts(session);
}

void fertp_send_alaw(const unsigned char *buf, ssize_t nbytes, ssize_t nsamples)
{
  //fprintf(stderr, "Advancing by %zd=%zd\n", nbytes, nsamples);
  rtp_session_send_with_ts(session, buf, nbytes, user_ts);
  user_ts += nsamples;
}

void fertp_stop(void)
{
  rtp_session_destroy(session);
}
