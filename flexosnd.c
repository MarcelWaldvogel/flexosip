#include "flexosnd.h"
#include <sndfile.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <alloca.h>
#include "unused.h"

// FIFO
static SNDFILE *sf[FESND_MAX_DEPTH];
static int waittime[FESND_MAX_DEPTH];
static int head, tail;

static int fesnd_close_all(const char *message);

int fesnd_add(const char *path)
{
  return fesnd_add_after_delay(0, path);
}

int fesnd_add_after_delay(int delay, const char *path)
{
  int nexthead = (head+1) % FESND_MAX_DEPTH;
  if (nexthead == tail) {
    fprintf(stderr, "fesnd_add(%s) ignored: FIFO full\n",
	    path);
    return 1;
  }
  
  int retval = 0;
  SF_INFO info;
  info.format = 0; // Auto-determine
  sf[head] = sf_open(path, SFM_READ, &info);
  waittime[head] = delay / 20; // Number of delay segments
  if (sf[head] == NULL) {
    fprintf(stderr, "Cannot open sound file %s\n", path);
    return 1; // Return directly, no file to close
  }
  if (info.channels != 1) {
    fprintf(stderr, "Sound file %p has %d channels, should be 1\n",
	    path, info.channels);
    retval = 1;
  }
  if (info.samplerate != 16000) {
    fprintf(stderr, "Sound file %p has %d samples/s, should be 16000\n",
	    path, info.samplerate);
    retval = 1;
  }
  if (retval == 0) {
    head = nexthead; // "Commit"
  } else {
    sf_close(sf[head]); // "Rollback"
    sf[head] = NULL;
  }
  return retval;
}

int fesnd_open(const char *path)
{
  fesnd_close_all("fesnd_open(): Still files in FIFO\n");
  return fesnd_add(path);
}

ssize_t fesnd_read(short *buf, ssize_t nsamples)
{
  if (head == tail) {
    fprintf(stderr, "Reading without open sound file\n");
    return 0;
  }
  // Pause first?
  if (waittime[tail] > 0) {
    waittime[tail]--;
    memset(buf, 0, sizeof(short)*nsamples);
    return nsamples;
  }
  // Pause done, send real file bytes
  int retval = sf_read_short(sf[tail], buf, nsamples);
  if (retval == nsamples) {
    // All is well
    return retval;
  } else {
    sf_close(sf[tail]);
    sf[tail] = NULL;
    // Proceed to next FIFO entry, if any
    tail = (tail +  1) % FESND_MAX_DEPTH;
    return retval; // No more files
  }
}

int fesnd_close(void)
{
  return fesnd_close_all(NULL);
}

static int fesnd_close_all(const char *message)
{
  int retval = 0;
  while (head != tail) {
    if (message != NULL)
      fputs(message, stderr);
    retval = sf_close(sf[tail]);
    sf[tail] = NULL;
    tail = (tail +  1) % FESND_MAX_DEPTH;
  }
  return retval;
}

// ------------- Sound encoding -----------------

typedef struct {
  unsigned char *buf;
  ssize_t len;
} userdata;

sf_count_t fesnd_vwrite(const void *ptr, sf_count_t count, void *user_data)
{
  userdata *ud = user_data;
  if (count > ud->len) {
    fprintf(stderr, "fesnd_vwrite() count %zd > ud->len %zd\n", (ssize_t)count, ud->len);
    count = ud->len;
  }
  memcpy(ud->buf, ptr, count);
  return count;
}

sf_count_t fesnd_vfilelen(void *UNUSED_PARAM(user_data))
{
  return 0;
}

sf_count_t fesnd_vtell(void *UNUSED_PARAM(user_data))
{
  return 0;
}

sf_count_t fesnd_vseek(sf_count_t UNUSED_PARAM(offset), int UNUSED_PARAM(whence), void *UNUSED_PARAM(user_data))
{
  return 0;
}

static SF_VIRTUAL_IO vio = {
  .get_filelen = fesnd_vfilelen,
  .write = fesnd_vwrite,
  // The following are included only for compatbility with
  // libsndfile < git-c11deaa (will probably go into v1.0.29)
  .seek = fesnd_vseek,
  .tell = fesnd_vtell
};

static userdata ud;

// Sample rate does not really matter here,
// can also be used for 8kHz
static SF_INFO alaw8k = {
  .frames = 0,
  .samplerate = 8000,
  .channels = 1,
  .format = SF_FORMAT_RAW | SF_FORMAT_ALAW | SF_ENDIAN_LITTLE,
  .sections = 0,
  .seekable = 0
};
static SNDFILE *viof;

ssize_t fesnd_encode_alaw(unsigned char *outbuf, short *inbuf,
    ssize_t nsamples, _Bool downsample)
{
  if (viof == NULL) {
    viof = sf_open_virtual(&vio, SFM_WRITE, &alaw8k, &ud);
    if (viof == NULL) {
      fprintf(stderr, "fesnd_encode_alaw(): Could not create vio\n");
      exit(1);
    }
  }
  if (downsample) {
    nsamples /= 2;
    short *downbuf = alloca(nsamples * sizeof(short));
    for (int i = 0; i < nsamples; i++) {
      // Downsampling by selection of every second sample would result in
      // frequency aliasing errors (any signal with f>4 kHz would not be
      // eliminated, but transformed into one with f-4 kHz, resulting in
      // more or less metallic sounds)
      downbuf[i] = (*inbuf + *(inbuf+1))/2;
      inbuf += 2;
    }
    // This is the new input to sf_write_short, along with the new sample count
    inbuf = downbuf;
  }

  ud.buf = outbuf;
  ud.len = nsamples; // should be so many bytes
  return sf_write_short(viof, inbuf, nsamples);
}
