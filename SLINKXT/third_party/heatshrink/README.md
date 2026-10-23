# heatshrink

Upstream: https://github.com/atomicobject/heatshrink
Revision: `7d419e1fa4830d0b919b9b6a91fe2fb786cf3280`
Author: Scott Vokes. License: ISC, included in LICENSE.

The encoder C source and its headers are copied unchanged from that revision.
Slink-XT's Linux wrapper selects window=12, lookahead=4, and resets the encoder
for each upload block. `linux/build_codec.sh` builds the shared helper with a
system C compiler. The helper is built locally for the current Linux system.
There is no Python package dependency.

`dos/HSDEC.ASM` is a Slink-XT-specific bounded implementation of the heatshrink
bit format, informed by the upstream decoder and format. It supports initial
zero history and overlapping references, with additional exact-length and
padding checks. It is not a general streaming heatshrink decoder. Credit and
the complete upstream license accompany this distribution.

Other design references were the TI TL16C552 datasheet (16-byte transmit FIFO),
FastLynx documentation/binary analysis, and Kermit/ZMODEM transfer techniques.
No FastLynx, Kermit or ZMODEM source was copied into this release.
