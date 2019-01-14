# API description

The API is pretty minimalistic. It is limited to a single call at a 
time and provides the following functionality:

- Register to the PBX
- Make a call or accept a call
- Send audio files
- Receive DTMF tones
- Hang up

This is all I needed and I guess it brings *ToT* devices a big step 
ahead (and also helps the beginner to softly make his feet wet with the 
complex world of SIP, SDP, and RDP).

`Flexosip` uses a minimalistic approach on callback functions. Similar 
to inheritance in object-oriented languages, `flexosip` already 
provides an implementation of all the callbacks. However, they can be 
overridden in your application by just defining them again with the 
same name. 

(There is no magic to this, but just the power of [Weak 
symbols](https://en.wikipedia.org/wiki/Weak_symbol). And no, it does 
not allow real subclassing or multiple inheritance or anything, just 
overriding the default implementation once.)

## Initialisation

```C
#include "flexosip/flexosip.h"
#include "flexosip/flexosnd.h"
#include "flexosip/flexortp.h"
```

To initialize debugging, call `TRACE_INITIALIZE(OSIP_INFO1, NULL);`, if 
you want to. (Instead of `OSIP_INFO1`, you can also use another 
verbosity level.)

If you want some cleanup to happen when you Ctrl-C the program, add the 
following:

```C
signal(SIGINT, fesip_cleanup);
signal(SIGHUP, fesip_cleanup);
signal(SIGTERM, fesip_cleanup);
signal(SIGQUIT, fesip_cleanup);
```

And then, initialize the network subsystem:

```C
fesip_listen(IPPROTO_UDP, false, 0);
```

Other options are described in [`flexosip.h`](./flexosip.h).

## Registration

Before you can make or receive a call, you need to register with your 
PBX (aka your telephone switch or VoIP exchange). The PBX is know as 
the 
[registrar](https://en.wikipedia.org/wiki/Session_Initiation_Protocol#Registrar) 
in [SIP](https://en.wikipedia.org/wiki/Session_Initiation_Protocol) 
parlance.

You do this by calling

```C
fesip_register(uri, registrar, login, password);
```

where the `uri` is typically of the form `sip:<extension>@<registrar>` 
and the registrar `sip:<hostname or domain>`, e.g. 
`sip:cow-bell@fritz.box` and `sip:fritz.box`, respectively.

The SIP registration (like almost all SIP messages) happen in the 
background while your code continues executing. If you want to be sure 
that the registration was successful (i.e., correct credentials were 
supplied) or need the successful registration to continue (namely, 
making an outgoing call), you can do

```C
ok = fesip_wait_registered();
```

which will wait and return `< 0` for failure. This will wait up to 
about 10 seconds or for two registration failures. (One registration 
failure is normal, as the first registration attempt is done without 
credentials.)

Now, your device is registered and can send and receive calls.

## Event loop

In your main code, you will need an event loop as follows:

```C
while (1) {
  // Handle SIP and RTP (audio)
  fesip_handle_event();

  // Do your own stuff
  …
}
```

`fesip_handle_event()` will sleep between 10 and 20 ms, depending on 
what it has to do.

The rest of your processing loop should not take more than 10 ms, 
unless you know that no audio is currently playing.

## Receive calls

When an incoming call arrives, the event handler will call your 
`fesip_event_invite()` callback:

```C
int fesip_event_invite(eXosip_event_t *evt,
    const char *host, int port, int format)
```

This is passed the incoming call event, the host and port that audio 
should go to, and the expected audio format (currently only PCMA/8000, 
aka 8 kHz A-Law). For now, you probably want to ignore them all.

To decline the call, just return `SIP_BUSY_HERE`.

However, if the call should be accepted, that function should return 
`SIP_RINGING` for now and set a variable, which will trigger accepting 
the connection in the event loop. This is necessary due to the locking 
that happens in `fesip_handle_event()`.

    In general, you cannot call any `fesip_*()` library functions from 
    any of the event handlers (`fesip_event_*()`). The only exception 
    currently is `fesip_play()`.

Outside the event handler, back in the event loop, you call

```C
fesip_answer();
```

to accept the call. If you want the caller to hear some ringing first, 
feel free to do this only after a few seconds.

## Play audio

You can then send an audio message with

```C
fesip_play(filename);
```

where `filename` is any 16 kHz mono file understood by 
[`libsndfile`](http://www.mega-nerd.com/libsndfile/). I use `.ogg` 
files.

`fesip_play()` actually adds the file to the end of the play queue, so 
you can call it multiple times. To stop playing and clear the play 
queue (e.g., on an incoming DTMF event), call

```C
fesnd_close();
```

(notice the `fesnd_*` prefix).

## Make calls

To make a call, use

```C
fesip_call(from, to, subject, NULL);
```

where `from` is your own SIP address, `to` is the destination's, and 
`subject` is the subject trasmitted (may or may not be displayed).

When the remote side declines, your

```C
void fesip_event_terminate(eXosip_event_t *evt);
```

Callback will be called (which you do not need to implement, if you do 
not care). This you will get on all call terminations, incoming or 
outgoing, pending or active.

When it answers, however, the callback will be

```C
void fesip_event_answered(eXosip_event_t *evt);
```

There, you can decline the call with `fesip_terminate_nolock()` 
(created especially to be called from a callback) or start playing 
audio as explained above.

## Receive DTMF

When the remote side — in an incoming or outgoing call — presses a key,

```C
void fesip_event_dtmf(char c);
```

is called with the ASCII character corresponding to the key pressed
(typically, '0'…'9', '*', '#').


## The end

That is already everything you need to know. Now you can start your own 
ToT device!
