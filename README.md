# FleXoSIP — A comfortable High-Level SIP API to eXosip and osip

The goal of *FleXoSIP — Fast Lane for eXosip* is to make `eXosip` and 
`osip` easy to use for common tasks involving a single inbound or 
outbound voice channel.

Even though the name might imply flexibility, it does not add any
flexibility, as `eXosip` and `osip` already provide it all.
Instead, it limits the use cases, trying to make the interface
easier. Unfortunately, I did not find anything pronouncible
denoting comfort and ending in *-exo*.

It uses [`eXosip`](https://www.antisip.com/doc/exosip2),
[`oRTP`](https://www.linphone.org/docs/ortp/), and
[`libsndfile`](http://www.mega-nerd.com/libsndfile/) to provide
basic single-channel SIP functionality for building your own simple
devices and/or applications.

For your convenience, here are also links to the source code:
* [`eXosip` source](https://savannah.nongnu.org/git/?group=exosip)
* [`oRTP` source](https://gitlab.linphone.org/BC/public/ortp),
  [mirror](https://github.com/BelledonneCommunications/ortp)
* [`libsndfile` source](https://github.com/erikd/libsndfile/)

## Installation: Preconditions and building

To create the library, just run `make`. However, installing the dependencies
**beforehand** is — depending on the distribution you are running — more
complicated:

### Installation on Ubuntu 20.04

The [eXoSIP library](https://savannah.nongnu.org/projects/exosip/) was dropped
from the repository, so you need to
[download](https://download.savannah.nongnu.org/releases/exosip/) or
[`git clone`](https://savannah.nongnu.org/git/?group=exosip) it yourself and
then compile and install it yourself.

But first, we must install the dependent libraries:
```sh
sudo apt install libosip2-dev libc-ares-dev libortp-dev libsndfile1-dev libinih-dev automake
```

Building is complicated by the fact that `libosip2` shipped in an OS released
in 2020 is still version 4.1.0 from 2013, so we need to check out a compatible
version as well…

```sh
git clone https://git.savannah.nongnu.org/git/exosip.git
cd exosip && git checkout 4.1.0 && ./configure && make && make install
```

### Installation on Ubuntu 18.04/18.10/19.10

```sh
sudo apt install libexosip2-dev libc-ares-dev libortp-dev libsndfile1-dev libinih-dev
```

### Installation on Raspbian Buster

```sh
sudo apt install libexosip2-dev libc-ares-dev libortp-dev libsndfile1-dev libinih-dev
```

### Installation on Raspbian Stretch

`libinih` is not part of Debian Stretch (and thus, Raspbian Stretch). Having
the source code in an `inih/` **sub**subdirectory will build and link it as
part of `make` in this directory.

```sh
sudo apt install libexosip2-dev libc-ares-dev libortp-dev libsndfile1-dev
git clone https://github.com/benhoyt/inih
```

Optional: If you also want to make `inih` available system-wide, you can run:
```sh
sudo apt install meson
cd inih && meson setup build -Ddistro_install=true && cd build && sudo ninja install
```

## Why another SIP library?

I was looking for a simple, high-level SIP library for an *ToT* 
(Telephony of Things 😉) device, a motion detector who could be 
controlled by SIP calls and would call back on events. (The result is 
[`cowbell`](https://github.com/MarcelWaldvogel/cowbell).) If you 
insist, you may also call it an IoT (Internet of Things) device…

Inspired by the ease of a solution based on Asterisk, I have been 
trying to use [PJSIP (PJSUA2)](https://www.pjsip.org/) with Python, but 
I never got beyond crashes. Nothing else seemed to be available in 
Python, so I looked for a plain C solution. I found 
[`eXosip`](https://www.antisip.com/eXosip2-toolkit), but couldn't make 
head or tails of the documentation and did not consider the API to be 
newbie-friendly. So I decided to start my own library, `flexosip`, 
following a simple [API](./API.md) of

- Register to the PBX
- Make a call or accept a call (only one call at a time)
- Send audio files
- Receive DTMF tones
- Hang up

This is all I needed and I guess it brings *ToT* devices a big step 
ahead (and also helps the beginner to softly make his feet wet with the 
complex world of SIP, SDP, and RDP).

## API

[The API is described in `API.md`](./API.md)

## Bugs

- Currently only handles IPv4 addresses (partly, because global c=/o= parameters do not allow multiple addresses)
- DTMF digits are only sent/received in outdated INFO format, not in RTP telephone-event format
- Does not yet do audio format negotiation (requires the caller to be able to receive PCMA/8000 aka A-Law 8 kHz audio)
