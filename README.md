# What is this?

ayumi-lv2 is an LV2 plugin implementation of [ayumi](https://github.com/true-grue/ayumi).

ayumi parameters are controlled via MIDI messages.

## MIDI mappings

ayumi is an PSG (SSG) emulator for AY-3-8910 or YM2149, and therefore handles at most 3 monophonic channels. Since each channel can be configured with mixer, volume, pan etc., every PSG channel is assigned an entire MIDI channel that are 0, 1, or 2. Other channels are ignored, and you cannot have any polyphonic outputs for one single channel.

The accepted MIDI messages are:

| MIDI message | Ayumi message | MIDI to ayumi mappings |
|-|-|-|
| Note Off (8xh) | note off (set_mixer) | - |
| Note On (9xh) | note on (set mixer,_set tone) | tone: 0-127 -> 0-4095 (indirect) |
| Program Change (Cxh) | set noise, set mixer | noise: 0-31 + mixer |
| CC - Bank MSB (Bxh-00h) | set mixer | 1: tone off, 2: noise off, 4: envelope on |
| CC - Volume (Bxh-07h) | set volume | 0-127 -> 0-14 |
| CC - Pan MSB (Bxh-0Ah) | set pan | 0-127 -> 0.0-1.0 |
| CC - 10h | envelope MSB | 0-65535 with LSB |
| CC - 11h | envelope LSB | 0-65535 with MSB |
| CC - 12h | envelope LSB | 0-65535 with MSB |
| CC - 13h | envelope shape | 0-15 |
| CC - 50h | remove dc | |

On Program Change messages, partial mixer settings can be added to noise as follows:

- tone off: +32
- noise off: +64

The same can be specified by CC Bank Change, which also supports envelope switch.

For some reason, ayumi does not process volume 15 as expected. Therefore it is rounded to 14.

## Licenses

The plugin is distributed under the MIT license, as well as ayumi itself.

