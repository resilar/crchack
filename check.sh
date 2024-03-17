#!/bin/sh
# crchack tests
CRCHACK="${1:-"$(dirname "$(realpath "$0")")/crchack"}"
if [ ! -x "$CRCHACK" ]; then
    echo "crchack not found" >&2
    echo "usage: $0 ./crchack" >&2
    exit 1
fi

OK=0
FAIL=0
expect () {
    if [ "$1" = "$2" ]; then
        OK=$((OK + 1))
        printf " OK"
    else
        FAIL=$((FAIL + 1))
        printf " FAIL"
    fi
}
check () {
    OPTS="$1"
    EXPECT="$2"
    #NAME="$3"
    printf "CHECK %s %s ..." "$CRCHACK" "$OPTS"
    expect "$EXPECT" "$(printf 123456789 | eval "$CRCHACK" "$OPTS" - | tr -d '\r\n')"
    expect "123456789" "$(printf 023456789 | eval "$CRCHACK" "$OPTS" -b0:1 - "$EXPECT")"
    expect "123456789" "$(printf 103456789 | eval "$CRCHACK" "$OPTS" -b1.1:2 - "$EXPECT")"
    expect "123456789" "$(printf 120456789 | eval "$CRCHACK" "$OPTS" -b2:3 - "$EXPECT")"
    expect "123456789" "$(printf 123056789 | eval "$CRCHACK" "$OPTS" -b3.2:4 - "$EXPECT")"
    printf "\n"
}

# curl -s http://reveng.sourceforge.net/crc-catalogue/all.htm        \
#  | grep -o 'width=.*name="[^"]*"' | sed 's/\s\+/ /g'               \
#  | sed 's/width=/-w/' | sed 's/poly=0x/-p/' | sed 's/init=0x/-i/'  \
#  | sed 's/refin=false //' | sed 's/refin=true/-r/'                 \
#  | sed 's/refout=false //' | sed 's/refout=true/-R/'               \
#  | sed 's/xorout=0x/-x/'                                           \
#  | sed 's/ -[xi]0\+\>//g'                                          \
#  | sed 's/-r -R/-rR/g'                                             \
#  | sed 's/^\(.*\) check=0x\([0-9A-Fa-f]*\).* name="\([^"]*\)".*$/check "\1" "\2" "\3"/'
check "-w3 -p3 -x7" "4" "CRC-3/GSM"
check "-w3 -p3 -i7 -rR" "6" "CRC-3/ROHC"
check "-w4 -p3 -rR" "7" "CRC-4/G-704"
check "-w4 -p3 -if -xf" "b" "CRC-4/INTERLAKEN"
check "-w5 -p09 -i09" "00" "CRC-5/EPC-C1G2"
check "-w5 -p15 -rR" "07" "CRC-5/G-704"
check "-w5 -p05 -i1f -rR -x1f" "19" "CRC-5/USB"
check "-w6 -p27 -i3f" "0d" "CRC-6/CDMA2000-A"
check "-w6 -p07 -i3f" "3b" "CRC-6/CDMA2000-B"
check "-w6 -p19 -rR" "26" "CRC-6/DARC"
check "-w6 -p03 -rR" "06" "CRC-6/G-704"
check "-w6 -p2f -x3f" "13" "CRC-6/GSM"
check "-w7 -p09" "75" "CRC-7/MMC"
check "-w7 -p4f -i7f -rR" "53" "CRC-7/ROHC"
check "-w7 -p45" "61" "CRC-7/UMTS"
check "-w8 -p2f -iff -xff" "df" "CRC-8/AUTOSAR"
check "-w8 -pa7 -rR" "26" "CRC-8/BLUETOOTH"
check "-w8 -p9b -iff" "da" "CRC-8/CDMA2000"
check "-w8 -p39 -rR" "15" "CRC-8/DARC"
check "-w8 -pd5" "bc" "CRC-8/DVB-S2"
check "-w8 -p1d" "37" "CRC-8/GSM-A"
check "-w8 -p49 -xff" "94" "CRC-8/GSM-B"
check "-w8 -p07 -x55" "a1" "CRC-8/I-432-1"
check "-w8 -p1d -ifd" "7e" "CRC-8/I-CODE"
check "-w8 -p9b" "ea" "CRC-8/LTE"
check "-w8 -p31 -rR" "a1" "CRC-8/MAXIM-DOW"
check "-w8 -p1d -ic7" "99" "CRC-8/MIFARE-MAD"
check "-w8 -p31 -iff" "f7" "CRC-8/NRSC-5"
check "-w8 -p2f" "3e" "CRC-8/OPENSAFETY"
check "-w8 -p07 -iff -rR" "d0" "CRC-8/ROHC"
check "-w8 -p1d -iff -xff" "4b" "CRC-8/SAE-J1850"
check "-w8 -p07" "f4" "CRC-8/SMBUS"
check "-w8 -p1d -iff -rR" "97" "CRC-8/TECH-3250"
check "-w8 -p9b -rR" "25" "CRC-8/WCDMA"
check "-w10 -p233" "199" "CRC-10/ATM"
check "-w10 -p3d9 -i3ff" "233" "CRC-10/CDMA2000"
check "-w10 -p175 -x3ff" "12a" "CRC-10/GSM"
check "-w11 -p385 -i01a" "5a3" "CRC-11/FLEXRAY"
check "-w11 -p307" "061" "CRC-11/UMTS"
check "-w12 -pf13 -ifff" "d4d" "CRC-12/CDMA2000"
check "-w12 -p80f" "f5b" "CRC-12/DECT"
check "-w12 -pd31 -xfff" "b34" "CRC-12/GSM"
check "-w12 -p80f -R" "daf" "CRC-12/UMTS"
check "-w13 -p1cf5" "04fa" "CRC-13/BBC"
check "-w14 -p0805 -rR" "082d" "CRC-14/DARC"
check "-w14 -p202d -x3fff" "30ae" "CRC-14/GSM"
check "-w15 -p4599" "059e" "CRC-15/CAN"
check "-w15 -p6815 -x0001" "2566" "CRC-15/MPT1327"
check "-w16 -p8005 -rR" "bb3d" "CRC-16/ARC"
check "-w16 -pc867 -iffff" "4c06" "CRC-16/CDMA2000"
check "-w16 -p8005 -iffff" "aee7" "CRC-16/CMS"
check "-w16 -p8005 -i800d" "9ecf" "CRC-16/DDS-110"
check "-w16 -p0589 -x0001" "007e" "CRC-16/DECT-R"
check "-w16 -p0589" "007f" "CRC-16/DECT-X"
check "-w16 -p3d65 -rR -xffff" "ea82" "CRC-16/DNP"
check "-w16 -p3d65 -xffff" "c2b7" "CRC-16/EN-13757"
check "-w16 -p1021 -iffff -xffff" "d64e" "CRC-16/GENIBUS"
check "-w16 -p1021 -xffff" "ce3c" "CRC-16/GSM"
check "-w16 -p1021 -iffff" "29b1" "CRC-16/IBM-3740"
check "-w16 -p1021 -iffff -rR -xffff" "906e" "CRC-16/IBM-SDLC"
check "-w16 -p1021 -ic6c6 -rR" "bf05" "CRC-16/ISO-IEC-14443-3-A"
check "-w16 -p1021 -rR" "2189" "CRC-16/KERMIT"
check "-w16 -p6f63" "bdf4" "CRC-16/LJ1200"
check "-w16 -p8005 -rR -xffff" "44c2" "CRC-16/MAXIM-DOW"
check "-w16 -p1021 -iffff -rR" "6f91" "CRC-16/MCRF4XX"
check "-w16 -p8005 -iffff -rR" "4b37" "CRC-16/MODBUS"
check "-w16 -p080b -iffff -rR" "a066" "CRC-16/NRSC-5"
check "-w16 -p5935" "5d38" "CRC-16/OPENSAFETY-A"
check "-w16 -p755b" "20fe" "CRC-16/OPENSAFETY-B"
check "-w16 -p1dcf -iffff -xffff" "a819" "CRC-16/PROFIBUS"
check "-w16 -p1021 -ib2aa -rR" "63d0" "CRC-16/RIELLO"
check "-w16 -p1021 -i1d0f" "e5cc" "CRC-16/SPI-FUJITSU"
check "-w16 -p8bb7" "d0db" "CRC-16/T10-DIF"
check "-w16 -pa097" "0fb3" "CRC-16/TELEDISK"
check "-w16 -p1021 -i89ec -rR" "26b1" "CRC-16/TMS37157"
check "-w16 -p8005" "fee8" "CRC-16/UMTS"
check "-w16 -p8005 -iffff -rR -xffff" "b4c8" "CRC-16/USB"
check "-w16 -p1021" "31c3" "CRC-16/XMODEM"
check "-w17 -p1685b" "04f03" "CRC-17/CAN-FD"
check "-w21 -p102899" "0ed841" "CRC-21/CAN-FD"
check "-w24 -p00065b -i555555 -rR" "c25a56" "CRC-24/BLE"
check "-w24 -p5d6dcb -ifedcba" "7979bd" "CRC-24/FLEXRAY-A"
check "-w24 -p5d6dcb -iabcdef" "1f23b8" "CRC-24/FLEXRAY-B"
check "-w24 -p328b63 -iffffff -xffffff" "b4f3e6" "CRC-24/INTERLAKEN"
check "-w24 -p864cfb" "cde703" "CRC-24/LTE-A"
check "-w24 -p800063" "23ef52" "CRC-24/LTE-B"
check "-w24 -p864cfb -ib704ce" "21cf02" "CRC-24/OPENPGP"
check "-w24 -p800063 -iffffff -xffffff" "200fa5" "CRC-24/OS-9"
check "-w30 -p2030b9c7 -i3fffffff -x3fffffff" "04c34abf" "CRC-30/CDMA"
check "-w31 -p04c11db7 -i7fffffff -x7fffffff" "0ce9e46c" "CRC-31/PHILIPS"
check "-w32 -p814141ab" "3010bf7f" "CRC-32/AIXM"
check "-w32 -pf4acfb13 -iffffffff -rR -xffffffff" "1697d06a" "CRC-32/AUTOSAR"
check "-w32 -pa833982b -iffffffff -rR -xffffffff" "87315576" "CRC-32/BASE91-D"
check "-w32 -p04c11db7 -iffffffff -xffffffff" "fc891918" "CRC-32/BZIP2"
check "-w32 -p8001801b -rR" "6ec2edc4" "CRC-32/CD-ROM-EDC"
check "-w32 -p04c11db7 -xffffffff" "765e7680" "CRC-32/CKSUM"
check "-w32 -p1edc6f41 -iffffffff -rR -xffffffff" "e3069283" "CRC-32/ISCSI"
check "-w32 -p04c11db7 -iffffffff -rR -xffffffff" "cbf43926" "CRC-32/ISO-HDLC"
check "-w32 -p04c11db7 -iffffffff -rR" "340bc6d9" "CRC-32/JAMCRC"
check "-w32 -p04c11db7 -iffffffff" "0376e6e7" "CRC-32/MPEG-2"
check "-w32 -p000000af" "bd0be338" "CRC-32/XFER"
check "-w40 -p0004820009 -xffffffffff" "d4164fc646" "CRC-40/GSM"
check "-w64 -p42f0e1eba9ea3693" "6c40df5f0b497347" "CRC-64/ECMA-182"
check "-w64 -p000000000000001b -iffffffffffffffff -rR -xffffffffffffffff" "b90956c775a41001" "CRC-64/GO-ISO"
check "-w64 -p42f0e1eba9ea3693 -iffffffffffffffff -xffffffffffffffff" "62ec59e3f1a4f00a" "CRC-64/WE"
check "-w64 -p42f0e1eba9ea3693 -iffffffffffffffff -rR -xffffffffffffffff" "995dc9bbdf1939fa" "CRC-64/XZ"
check "-w82 -p0308c0111011401440411 -rR" "09ea83f625023801fd612" "CRC-82/DARC"

printf 'SOLVE %s Google CTF 2018 (Quals) task "Tape, misc, 355p" ...' "$CRCHACK"
expect ': You probably just want the flag.  So here it is: CTF{dZXicOXLaMumrTPIUTYMI}. :' "$(printf ': You probably just want the flag.  So here it is: CTF{dZXi__________PIUTYMI}. :' | eval "$CRCHACK" -b '59.{0-5}:69:1' -w64 -p0x42F0E1EBA9EA3693 -rR - 0x30d498cbfb871112)"
expect "30d498cbfb871112" "$(printf ': You probably just want the flag.  So here it is: CTF{dZXi__________PIUTYMI}. :' | eval "$CRCHACK" -b '59.{0-5}:69:1' -w64 -p0x42F0E1EBA9EA3693 -rR - 0x30d498cbfb871112 | eval "$CRCHACK" -w64 -p0x42F0E1EBA9EA3693 -rR -)"
printf "\n"
