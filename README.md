# Intel 8080 Emulator

Intel 8080 emulator written in C.

This emulator was meant to be a part of a bigger project - creating an Alatir8800 "replica" running on some microcontroller. The memory and IO are stored in separate files so they can be easily developed to be used with external hardware.

The emulator has no external dependencies, you just need `cmake`, `make` and `GCC` to compile it.

## Development status
The emulated CPU passes all of the tests I managed to find.
```
====== Running test ../programs/8080PRE.COM ======
8080 Preliminary tests complete
====== Elapsed CPU cycles: 7797 ======


====== Running test ../programs/CPUTEST.COM ======

DIAGNOSTICS II V1.2 - CPU TEST
COPYRIGHT (C) 1981 - SUPERSOFT ASSOCIATES

ABCDEFGHIJKLMNOPQRSTUVWXYZ
CPU IS 8080/8085
BEGIN TIMING TEST
END TIMING TEST
CPU TESTS OK

====== Elapsed CPU cycles: 255660240 ======


====== Running test ../programs/TST8080.COM ======
MICROCOSM ASSOCIATES 8080/8085 CPU DIAGNOSTIC
 VERSION 1.0  (C) 1980

 CPU IS OPERATIONAL
====== Elapsed CPU cycles: 4894 ======


====== Running test ../programs/8080EXER.COM ======
8080 instruction exerciser
dad <b,d,h,sp>................  OK
aluop nn......................  OK
aluop <b,c,d,e,h,l,m,a>.......  OK
<daa,cma,stc,cmc>.............  OK
<inr,dcr> a...................  OK
<inr,dcr> b...................  OK
<inx,dcx> b...................  OK
<inr,dcr> c...................  OK
<inr,dcr> d...................  OK
<inx,dcx> d...................  OK
<inr,dcr> e...................  OK
<inr,dcr> h...................  OK
<inx,dcx> h...................  OK
<inr,dcr> l...................  OK
<inr,dcr> m...................  OK
<inx,dcx> sp..................  OK
lhld nnnn.....................  OK
shld nnnn.....................  OK
lxi <b,d,h,sp>,nnnn...........  OK
ldax <b,d>....................  OK
mvi <b,c,d,e,h,l,m,a>,nn......  OK
mov <bcdehla>,<bcdehla>.......  OK
sta nnnn / lda nnnn...........  OK
<rlc,rrc,ral,rar>.............  OK
stax <b,d>....................  OK
Tests complete
====== Elapsed CPU cycles: 23835554000 ======


====== Running test ../programs/8080EXM.COM ======
8080 instruction exerciser
dad <b,d,h,sp>................  PASS! crc is:14474ba6
aluop nn......................  PASS! crc is:9e922f9e
aluop <b,c,d,e,h,l,m,a>.......  PASS! crc is:cf762c86
<daa,cma,stc,cmc>.............  PASS! crc is:bb3f030c
<inr,dcr> a...................  PASS! crc is:adb6460e
<inr,dcr> b...................  PASS! crc is:83ed1345
<inx,dcx> b...................  PASS! crc is:f79287cd
<inr,dcr> c...................  PASS! crc is:e5f6721b
<inr,dcr> d...................  PASS! crc is:15b5579a
<inx,dcx> d...................  PASS! crc is:7f4e2501
<inr,dcr> e...................  PASS! crc is:cf2ab396
<inr,dcr> h...................  PASS! crc is:12b2952c
<inx,dcx> h...................  PASS! crc is:9f2b23c0
<inr,dcr> l...................  PASS! crc is:ff57d356
<inr,dcr> m...................  PASS! crc is:92e963bd
<inx,dcx> sp..................  PASS! crc is:d5702fab
lhld nnnn.....................  PASS! crc is:a9c3d5cb
shld nnnn.....................  PASS! crc is:e8864f26
lxi <b,d,h,sp>,nnnn...........  PASS! crc is:fcf46e12
ldax <b,d>....................  PASS! crc is:2b821d5f
mvi <b,c,d,e,h,l,m,a>,nn......  PASS! crc is:eaa72044
mov <bcdehla>,<bcdehla>.......  PASS! crc is:10b58cee
sta nnnn / lda nnnn...........  PASS! crc is:ed57af72
<rlc,rrc,ral,rar>.............  PASS! crc is:e0d89235
stax <b,d>....................  PASS! crc is:2b0471e9
Tests complete
====== Elapsed CPU cycles: 23835625882 ======

```
