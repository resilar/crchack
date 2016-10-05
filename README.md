**crchack** is a public domain tool to force CRC checksums to arbitrary values.
The main advantage over existing CRC alteration tools is the ability to obtain
the desired checksum by changing non-contiguous input bits. crchack supports all
commonly used CRC algorithms as well as custom parameters.

- [Usage](#usage)
- [Examples](#exampples)
- [CRC algorithms](#crc-algorithms)
- [How it works?](#how-it-works)
- [Use cases](#use-cases)


# Usage

```
usage: ./crchack [options] file [new_checksum]

options:
  -o pos    byte.bit offset of mutable input bits
  -O pos    offset from the end of the input
  -b l:r:s  specify bits at offsets l..r with a step s
  -h        show this help
  -v        verbose mode

CRC parameters (default: CRC-32):
  -w size   register size in bits   -p poly   generator polynomial
  -i init   initial register value  -x xor    final register XOR mask
  -r        reverse input bits      -R        reverse final register
```

The input message is read from *file* and the adjusted message is written to
stdout. By default, crchack appends 4 bytes to the input producing a new message
that has the target CRC-32 checksum. Other CRC algorithms can be defined via the
CRC parameters. If *new_checksum* is not given, then crchack writes the CRC
checksum of the input message to stdout and exits.


# Examples

```
[crchack]% echo "hello" > foo
[crchack]% ./crchack foo
363a3020
[crchack]% ./crchack foo deff1420 > bar
[crchack]% ./crchack bar
deff1420
[crchack]% xxd bar
00000000: 6865 6c6c 6f0a fd37 37f6                 hello..77.

[crchack]% echo "foobar" | ./crchack - DEADBEEF | ./crchack -
deadbeef

[crchack]% echo "PING 1234" | ./crchack -
29092540
[crchack]% echo "PING XXXX" | ./crchack -o5 - 29092540
PING 1234
```

In order to modify non-consecutive input message bits, the bit positions need
to be specified with the `-b` switches. The argument for `-b` is a Python-style
*slice* in format `start:end:step`, representing bits between the positions
`start` and `end` (exclusive) with successive bits `step` bits apart. By
default, `start` is the beginning of the message, `end` is the end of the
message, and `step` equals to 1 bit. For example, `-b 4:` selects all bits
starting from the *byte* index 4, that is, the 32nd and subsequent bits of the
input. However, `-b 4` without a colon selects only the 32nd bit.

```
[crchack]% echo "1234PQPQ" | ./crchack -b 4: - 12345678
1234u>|7
[crchack]% echo "1234PQPQ" | ./crchack -b :4 - 12345678
_MLPPQPQ
[crchack]% echo "1234u>|7" | ./crchack - && echo "_MLPPQPQ" | ./crchack -
12345678
12345678
```

The byte index can be followed by a dot `.` and a *bit* index so that, e.g.,
`-b0.32`, `-b2.16` and `-b4.0` select the 32nd bit. Bits within a byte are
numbered from 0 (least significant bit) to 7 (most significant bit). Negative
indices are from the end of the input message. Finally, notice that the crchack
includes an expression parser that supports basic arithmetic operations.

```
[crchack]% echo "aXbXXcXd" | ./crchack -b1:2 -b3:5 -b6:7 - cafebabe | xxd
00000000: 61d6 6298 f763 4d64 0a                   a.b..cMd.
[crchack]% echo -e "a\xD6b\x98\xF7c\x4Dd" | ./crchack -
cafebabe

[crchack]% python -c 'print("A"*32)' | ./crchack -b "0.5:0.5+8*32:.8" - 1337c0de
AAAaAaaaaaAAAaAaAaAaAaaAaAaaAAaA
[crchack]% echo "AAAaAaaaaaAAAaAaAaAaAaaAaAaaAAaA" | ./crchack -
1337c0de

[crchack]% echo "1234567654321" | ./crchack -b .0:-1:1 -b .1:-1:1 -b .2:-1:1 - baadf00d
0713715377223
[crchack]% echo "0713715377223" | ./crchack -
baadf00d
```

In general, the user should provide **at least** *w* bits where *w* is the width
of the CRC register, e.g., 32 for CRC-32. Obtaining the desired checksum is
impossible if an insufficient number of mutable bits is given. In practice, a
few bits more than *w* tends to be enough if the bits are non-contiguous.


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

[CRC RevEng] [1] (by Greg Cook) includes a comprehensive list of cyclic
redundancy check algorithms and their parameters: [Catalogue of parametrised CRC
algorithms] [2].


# How it works?

CRC is often described as a linear function in the literature. However, CRC
implementations used in practice often differ from the theoretical definition
and satisfy only a "weaker" linear property:

    CRC(x ^ y ^ z) = CRC(x) ^ CRC(y) ^ CRC(z), for |x| = |y| = |z|

The method can be viewed as applying this rule repeatedly to produce an
invertible system of linear equations. Solving the system tells us which bits in
the input data need to be modified.

The intuition is that flipping each input bit causes a fixed difference in the
resulting checksum (independent of the values of the neighbouring bits). This,
in addition to knowing the required difference, gives a system of linear
equations over GF(2). The system of equations can then be expressed in a matrix
form and solved with, e.g., the Gauss-Jordan elimination algorithm. Under- and
overdetermined systems need special handling.

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


  [1]: http://reveng.sourceforge.net/ "CRC RevEng"
  [2]: http://reveng.sourceforge.net/crc-catalogue/ "CRC catalogue"
