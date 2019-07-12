**crchack** is a public domain tool to force CRC checksums to arbitrarily chosen
values. The main advantage over existing CRC alteration tools is the ability to
obtain the desired checksum by changing non-contiguous input bits. crchack
supports all commonly used CRC algorithms as well as custom parameters.

- [Usage](#usage)
- [Examples](#examples)
- [CRC algorithms](#crc-algorithms)
- [How it works?](#how-it-works)
- [Use cases](#use-cases)


# Usage

```
usage: ./crchack [options] file [new_checksum]

options:
  -o pos    byte.bit offset of mutable input bits
  -O pos    offset from the end of the input
  -b l:r:s  specify bits at offsets l..r with step s
  -h        show this help
  -v        verbose mode

CRC parameters (default: CRC-32):
  -w size   register size in bits   -p poly   generator polynomial
  -i init   initial register value  -x xor    final register XOR mask
  -r        reverse input bytes     -R        reverse final register
```

The input message is read from *file* and the adjusted message is written to
stdout. By default, crchack appends 4 bytes to the input producing a new message
that has the target CRC-32 checksum. Other CRC algorithms can be defined via the
CRC parameters. If *new_checksum* is not given, then crchack writes the CRC
checksum of the input message to stdout and exits.


# Examples

```
[crchack]$ echo "hello" > foo
[crchack]$ ./crchack foo
363a3020
[crchack]$ ./crchack foo deff1420 > bar
[crchack]$ ./crchack bar
deff1420
[crchack]$ xxd bar
00000000: 6865 6c6c 6f0a fd37 37f6                 hello..77.

[crchack]$ echo "foobar" | ./crchack - DEADBEEF | ./crchack -
deadbeef

[crchack]$ echo "PING 1234" | ./crchack -
29092540
[crchack]$ echo "PING XXXX" | ./crchack -o5 - 29092540
PING 1234
```

In order to modify non-consecutive input message bits, the modifiable bits need
to be specified with `-b start:end:step` switches, where the argument is a
Python-style *slice* representing the bits between the positions `start` and
`end` (exclusive) with successive bits `step` bits apart. By default, `start` is
the beginning of the message, `end` is the end of the message, and `step` equals
to 1 bit. For example, `-b 4:` selects all bits starting from the *byte* offset
4 (note that `-b 4` without the colon selects only the 32nd bit). Negative
offsets are from the end of the input.

```
[crchack]$ echo "1234PQPQ" | ./crchack -b 4: - 12345678
1234u>|7
[crchack]$ echo "1234PQPQ" | ./crchack -b :4 - 12345678
_MLPPQPQ
[crchack]$ echo "1234u>|7" | ./crchack - && echo "_MLPPQPQ" | ./crchack -
12345678
12345678
```

The byte offset is optionally followed by a dot-separated *bit* offset so that,
e.g., `-b0.32`, `-b2.16` and `-b4.0` select the 32nd bit. Bits within a byte are
numbered from 0 (least significant bit) to 7 (most significant bit). Finally,
`0x`-prefixed hexadecimal numbers as well as basic arithmetic operations `+-*/`
are supported by the built-in expression parser.

```
[crchack]$ echo "aXbXXcXd" | ./crchack -b1:2 -b3:5 -b6:7 - cafebabe | xxd
00000000: 61d6 6298 f763 4d64 0a                   a.b..cMd.
[crchack]$ echo -e "a\xD6b\x98\xF7c\x4Dd" | ./crchack -
cafebabe

[crchack]$ python -c 'print("A"*0x20)' | ./crchack -b "0.5:0.5+8*0x20:.8" - 1337c0de
AAAaAaaaaaAAAaAaAaAaAaaAaAaaAAaA
[crchack]$ echo "AAAaAaaaaaAAAaAaAaAaAaaAaAaaAAaA" | ./crchack -
1337c0de

[crchack]$ echo "1234567654321" | ./crchack -b .0:-1:1 -b .1:-1:1 -b .2:-1:1 - baadf00d
0713715377223
[crchack]$ echo "0713715377223" | ./crchack -
baadf00d
```

Obtaining the target checksum is impossible given an insufficient number of
mutable bits. In general, the user should provide at least *w* bits where *w* is
the width of the CRC register, e.g., 32 bits for CRC-32.


# CRC algorithms

crchack works with all CRCs that use sane generator polynomial, including all
commonly used and standardized CRCs. The program defaults to CRC-32, while other
CRC functions can be specified by passing the CRC parameters via command-line
arguments.

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
cyclic redundancy check algorithms and their parameters.


# How it works?

CRC is often described as a linear function in the literature. However, CRC
implementations used in practice often differ from the theoretical definition
and satisfy only a "weaker" linear property:

    CRC(x ^ y ^ z) = CRC(x) ^ CRC(y) ^ CRC(z), for |x| = |y| = |z|

The method can be viewed as applying this rule repeatedly to produce an
invertible system of linear equations. Solving the system tells us which bits in
the input data need to be flipped.

The intuition is that flipping each input bit causes a fixed difference in the
resulting checksum (independent of the values of the neighbouring bits). This,
in addition to knowing the required difference, gives a system of linear
equations over GF(2). The system of equations can then be expressed in a matrix
form and solved with, e.g., the Gauss-Jordan elimination algorithm. Under- and
overdetermined systems require special handling.

Note that the CRC function is treated as a "black box", i.e., the internal
parameters of the CRC are not needed except for evaluation. In fact, the same
approach is applicable to all checksum functions that satisfy the weak linearity
property.


# Use cases

So why would someone want to forge CRC checksums? Because `CRC32("It'S coOL To
wRitE meSSAGes LikE this :DddD") == 0xDEADBEEF`.

In a more serious sense, there exist a bunch of firmwares, protocols, file
formats and standards that utilize CRCs. Hacking these may involve, e.g.,
modification of data so that the original CRC checksum remains intact. One
interesting possibility is the ability to store arbitrary data (e.g., binary
code) to checksum fields in packet and file headers.
