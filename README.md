**crchack** is a public domain tool to force CRC checksums to arbitrary values.
The main advantage over existing CRC alteration tools is the ability to obtain
the desired checksum by changing non-contigous input bits. crchack supports all
commonly used CRC algorithms as well as custom parameters.

- [Usage](#usage)
- [Supported CRC algorithms](#supported-crc-algorithms)
- [How it works?](#how-it-works)
- [Use cases](#use-cases)


# Usage

```
usage: ./crchack [options] file [new_checksum]

options:
  -o pos   starting bit offset of the mutable input bits
  -O pos   offset from the end of the input message
  -b file  read bit offsets from a file
  -h       show this help

CRC options (default: CRC-32):
  -w size  register size in bits   -r       reverse input bits
  -p poly  generator polynomial    -R       reverse final register
  -i init  initial register        -x xor   final register XOR mask
```

The input message is read from *file* and the adjusted message is written to
stdout. By default, crchack appends 4 bytes to the input producing a new message
that has the specified new CRC-32 checksum. Other CRC algorithms can be defined
via the CRC options. If *new_checksum* is not given, then crchack writes the CRC
checksum of the input message to stdout and exits.


## Examples

```
[crchack]% echo "hello" > foo
[crchack]% ./crchack foo 
363a3020
[crchack]% ./crchack foo deff1420 > bar
[crchack]% ./crchack bar
deff1420
[crchack]% xxd bar
00000000: 6865 6c6c 6f0a fd37 37f6                 hello..77.
```

```
[crchack]% echo "foobar" | ./crchack - 1337C0DE | ./crchack -
1337c0de
```

## Bits file format

**Warning** This is going to change in the future (to something that sucks less)

In order to modify non-consecutive input message bits, the bit positions need to
be defined in an external file referred to as *the bits file* (passed in via the
``-b`` switch). The file should begin with the text string "bits" followed by
whitespace separated list of bit indices. The first bytes start at positions 0,
8, 16, ... and bits are numbered from the LSB to MSB (e.g., index 10 corresponds
to the third least significant bit of the second byte).

The bit indices are presented in decimal or 0x-prefixed hexadecimal format.
Alternatively, the bit indices can be given as two comma separated values, where
the first is a byte offset in the message and the second selects bit(s) in that
byte. The second value is treated as a bit mask if it is a 0x-prefixed
hexadecimal value, otherwise the value is a bit index in range 0-7.

For example, the following file selects bits 0, 11, 16, the top four bits of
byte 20, and all bits of byte 31.

```
bits 0 1,3 0x10 20,0xF0 0x1F,0xFF
```

The file should specify **at least** *w* bits where *w* is the width of the CRC
register, e.g., 32 for CRC-32. Obtaining the desired checksum is impossible if
an insufficient number of mutable bits is given. In general, a few bits more
than *w* tends to be enough if the bits are non-contiguous.


# Supported CRC algorithms

crchack works with all CRCs that use sane generator polynomial, i.e., all
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

The intuition is that inverting each input bit causes a fixed difference in the
resulting checksum (independent of the values of the neighbouring bits). This,
in addition to knowing the required difference, gives a system of linear
equations over GF(2). The system of equations can then be expressed in a matrix
form and solved with, e.g., the Gauss-Jordan elimination algorithm. Under- and
overdetermined systems need special handling.

Note that the CRC function is treated as a "black box", i.e., the internal
parameters of the CRC are not needed (except for evaluation). In fact, the same
approach is applicable to all checksum functions that satisfy the weak linearity
property.


# Use cases

So why would someone want to forge CRC checksums? ``CRC32("It'S coOL To wRitE
meSSAGes LikE this :DddD") == 0xDEADBEEF``.

In a more serious sense, there exist a bunch of firmwares, protocols, file
formats and standards that utilize CRCs. Hacking these may involve, e.g.,
modification of data so that the original CRC checksum remains intact. One
interesting possibility is the ability to store arbitrary data (e.g., binary
code) to checksum fields in packet and file headers.


  [1]: http://reveng.sourceforge.net/ "CRC RevEng"
  [2]: http://reveng.sourceforge.net/crc-catalogue/ "CRC catalogue"
