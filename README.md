**crchack** is a free tool that forces CRC checksum of a file to any given value
by modifying chosen input bits. The main advantage over existing CRC alteration
tools is the ability to obtain the target checksum by changing non-contiguous
bits of the input message. crchack supports all commonly used CRC algorithms.

- [Usage](#usage)
- [Supported CRC algorithms](#supported-crc-algorithms)
- [How it works?](#how-it-works)
- [Use cases](#use-cases)


# Usage

<pre>
usage: ./crchack [options] desired_checksum [file]

options:
  -c       write CRC checksum to stdout and exit
  -o pos   starting bit offset of the mutable input bits
  -O pos   offset from the end of the input message
  -b file  read bit offsets from a file
  -h       show this help

CRC options (CRC-32 if none given):
  -w size  register size in bits   -x xor   final register XOR mask
  -p poly  generator polynomial    -r       reverse input bits
  -i init  initial register        -R       reverse final register
</pre>

The input message is read from *file* (or stdin if not given) and the updated
message is written to stdout. By default, crchack appends 4 bytes to the input
producing a new message that has the desired CRC-32 checksum value. Other CRC
algorithms can be specified via the CRC options. crchack can also be used to
calculate CRC checksums by setting the switch ``-c`` and leaving out the
*desired_checksum* argument.

## Example

```
[crchack]$ echo -ne "hello" > test
[crchack]$ ./crchack -c test
3610a686
[crchack]$ ./crchack deff1420 test > test2
[crchack]$ ./crchack -c test2
deff1420
[crchack]$ hexdump -C test2
00000000  68 65 6c 6c 6f 5b a1 1d  f6                       |hello[...|
00000009
```

## Bits file format

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
[crchack]$ echo -ne "123456789" > msg
[crchack]$ ./crchack -c -w8 -p7 msg                                     # CRC-8
f4
[crchack]$ ./crchack -c -w16 -p8005 -rR msg                             # CRC-16
bb3d
[crchack]$ ./crchack -c -w16 -p8005 -iffff -rR msg                      # MODBUS
4b37
[crchack]$ ./crchack -c -w32 -p04c11db7 -iffffffff -xffffffff -rR msg   # CRC-32
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

For more details, see the file [forge32.c](forge32.c).


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
