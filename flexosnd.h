/* flexosnd — The sound library for flexoSIP
 * 
 * Provides a simple, single-file interface
 * for reading/writing A-Law 8kHz encoded files
 */
#include <sndfile.h>
#define FESND_MAX_DEPTH 32 // # of pending fesnd_push()es

/**
 * Open a sound file and check format
 * 
 * Returns != 0 on error and prints diagnostic to stderr
 * 
 * @param path		File name
 */
int fesnd_open(const char *path);

/**
 * Enqueue the next file, which should be automatically opened
 * 
 * Otherwise behaves as fesnd_open()
 */
 int fesnd_add(const char *path);

/**
 * Enqueue the next file, which should be automatically opened
 * 
 * Otherwise behaves as fesnd_open().
 * @param	delay		Number of milliseconds of silence to play before the file. Only multiples of 20ms are accepted.
 * @param	path		The sound file to play
 */
 int fesnd_add_after_delay(int delay, const char *path);

/**
 * Read from an opened sound file
 * 
 * Returns numbers of bytes read, 0 on EOF/error.
 * 
 * Reading EOF closes the file (and continues with the next file
 * in the FIFO, if any). Each file must have at least as many
 * samples as the maximum `len` requested, otherwise the auto-next
 * feature will return short reads.
 * 
 * @param buf		The buffer to read into
 * @param nsamples	The number of samples to read
 */
ssize_t fesnd_read(short *buf, ssize_t nsamples);

/**
 * Close the previously opened sound file
 * 
 * Returns != 0 on error
 */
int fesnd_close(void);

// Sound encoding functions

/**
 * Encode 16bit PCM to 8bit ALAW
 *
 * Returns the number of samples (which equals bytes here) that
 * have been converted.
 *
 * @param outbuf	Where ALAW will end up at
 * @param inbuf		Where PCM16 is taken from
 * @param nsamples	How many samples to convert
 * @param downsample	Skip every second sample?
 */
ssize_t fesnd_encode_alaw(unsigned char *outbuf, short *inbuf,
			  ssize_t nsamples, _Bool downsample);
