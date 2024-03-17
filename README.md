**crchack** is a public domain tool to force CRC checksums of input messages to
arbitrarily chosen values. The main advantage over existing CRC alteration
tools is the ability to obtain the target checksum by changing non-contiguous
input bits. Furthermore, crchack supports *all* CRC algorithms including custom
CRC parameters. crchack is also an arbitrary-precision CRC calculator.

- [Usage](#usage)
- [Examples](#examples)
- [CRC algorithms](#crc-algorithms)
- [How it works?](#how-it-works)
- [Use cases](#use-cases)


# Usage

```
usage: ./crchack [options] file [target_checksum]

options:
  -o pos    byte.bit position of mutable input bits
  -O pos    position offset from the end of the input
  -b l:r:s  specify bits at positions l..r with step s
  -h        show this help
  -v        verbose mode

CRC parameters (default: CRC-32):
  -p poly   generator polynomial    -w size   register size in bits
  -i init   initial register value  -x xor    final register XOR mask
  -r        reverse input bytes     -R        reverse final register
```

Input message is read from *file* and the patched message is written to stdout.
By default, crchack appends 4 bytes to the input producing a new message with
the target CRC-32 checksum. Other CRC algorithms are defined using the CRC
parameters. If *target_checksum* is not given, then crchack calculates the CRC
checksum of the input message and writes the result to stdout.


# Examples

```
[crchack]$ echo "hello" > foo
[crchack]$ ./crchack foo
363a3020
[crchack]$ ./crchack foo 42424242 > bar
[crchack]$ ./crchack bar
42424242
[crchack]$ xxd bar
00000000: 6865 6c6c 6f0a d2d1 eb7a                 hello....z

[crchack]$ echo "foobar" | ./crchack - DEADBEEF | ./crchack -
deadbeef

[crchack]$ echo "PING 1234" | ./crchack -
29092540
[crchack]$ echo "PING XXXX" | ./crchack -o5 - 29092540
PING 1234
```

In order to modify non-consecutive input message bits, specify the mutable bits
with `-b start:end:step` switches using Python-style *slices* which represent
the bits between positions `start` (inclusive) and `end` (exclusive) with
successive bits `step` bits apart. If empty, `start` is the beginning of the
message, `end` is the end of the message, and `step` equals 1 bit selecting all
bits in between. For example, `-b 4:` selects all bits starting from the *byte*
position 4 (note that `-b 4` without the colon selects only the 32nd bit).

```
[crchack]$ echo "aXbXXcXd" | ./crchack -b1:2 -b3:5 -b6:7 - cafebabe | xxd
00000000: 61d6 6298 f763 4d64 0a                   a.b..cMd.
[crchack]$ echo -e "a\xD6b\x98\xF7c\x4Dd" | ./crchack -
cafebabe

[crchack]$ echo "1234PQPQ" | ./crchack -b 4: - 12345678
1234u>|7
[crchack]$ echo "1234PQPQ" | ./crchack -b :4 - 12345678
_MLPPQPQ
[crchack]$ echo "1234u>|7" | ./crchack - && echo "_MLPPQPQ" | ./crchack -
12345678
12345678
```

The byte position is optionally followed by a dot-separated *bit* position. For
instance, `-b0.32`, `-b2.16` and `-b4.0` all select the same 32nd bit. Within a
byte, bits are numbered from the least significant bit (0) to the most
significant bit (7). Negative positions are treated as offsets from the end of
the input. Built-in expression parser supports `0x`-prefixed hexadecimal
numbers as well as basic arithmetic operations `+-*/`. Finally, `end` can be
defined relative to `start` by starting the position expression with unary `+`
operator.

```
[crchack]$ python -c 'print("A"*32)' | ./crchack -b "0.5:+.8*32:.8" - 1337c0de
AAAaAaaaaaAAAaAaAaAaAaaAaAaaAAaA
[crchack]$ echo "AAAaAaaaaaAAAaAaAaAaAaaAaAaaAAaA" | ./crchack -
1337c0de

[crchack]$ echo "1234567654321" | ./crchack -b .0:-1:1 -b .1:-1:1 -b .2:-1:1 - baadf00d
0713715377223
[crchack]$ echo "0713715377223" | ./crchack -
baadf00d
```

In the latter example, repetition can be avoided by utilizing brace expansions:

```
[crchack]$ echo "1234567654321" | ./crchack -b ".{0-2}:-1:1" - baadf00d
0713715377223
```

Obtaining the target checksum is impossible if given an insufficient number of
mutable bits. In general, the user should provide at least *w* bits where *w*
is the width of the CRC register, e.g., 32 bits for CRC-32.


# CRC algorithms

crchack works with all CRCs that use sane parameters, including all commonly
used standardized CRCs. crchack defaults to CRC-32 and other CRC functions can
be specified by passing the CRC parameters via command-line arguments.

```
[crchack]$ printf "123456789" > msg
[crchack]$ ./crchack -w8 -p7 msg                                     # CRC-8
f4
[crchack]$ ./crchack -w16 -p8005 -rR msg                             # CRC-16
bb3d
[crchack]$ ./crchack -w16 -p8005 -iffff -rR msg                      # MODBUS
4b37
[crchack]$ ./crchack -w32 -p04c11db7 -iffffffff -xffffffff -rR msg   # CRC-32
cbf43926
```

[CRC RevEng](http://reveng.sourceforge.net/) (by Greg Cook) includes a
comprehensive [catalogue](http://reveng.sourceforge.net/crc-catalogue/) of
cyclic redundancy check algorithms and their parameters. [check.sh](check.sh)
illustrates how to convert the CRC catalogue entries to crchack CLI flags.


# How it works?

CRC is often described as a linear function in the literature. However, CRC
implementations used in practice often differ from the theoretical definition
and satisfy only a *weak* linearity property (`^` denotes XOR operator):

    CRC(x ^ y ^ z) = CRC(x) ^ CRC(y) ^ CRC(z), for |x| = |y| = |z|

crchack applies this rule to construct a system of linear equations such that
the solution is a set of input bits which inverted yield the target checksum.

The intuition is that each input bit flip causes a fixed difference in the
resulting checksum (independent of the values of the neighbouring bits). This,
in addition to knowing the required difference, produces a system of linear
equations over [GF(2)](https://en.wikipedia.org/wiki/Finite_field). crchack
expresses the system in a matrix form and solves it with the Gauss-Jordan
elimination algorithm.

Notice that the CRC function is treated as a "black box", i.e., the internal
CRC parameters are used only for evaluation. Therefore, the same approach is
applicable to any function that satisfies the weak linearity property.


# Use cases

So why would someone want to forge CRC checksums? Because `CRC32("It'S coOL To
wRitE meSSAGes LikE this :DddD") == 0xDEADBEEF`.

In a more serious sense, there exist a bunch of firmwares, protocols, file
formats and standards that utilize CRCs. Hacking these may involve, e.g.,
modification of data so that the original CRC checksum remains intact. One
interesting possibility is the ability to store arbitrary data (e.g., binary
code) to checksum fields in packet and file headers.
