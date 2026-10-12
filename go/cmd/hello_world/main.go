// hello_world - translation of hello_world.py: load the "Hello, World!"
// program and run it until it halts.
package main

import "pdp11emu/pdp11"

// HelloWorld is the program from
// http://www.retrocmp.com/how-tos/interfacing-to-a-pdp-1105/146-interfacing-with-a-pdp-1105-test-programs-and-qhello-worldq
var HelloWorld = []int{
	0012702, // 2000 start:  MOV
	0177564, // 2002         serial+4,r2    ; r2 points to DL11
	0012701, // 2004         MOV
	0002032, // 2006         string,r1      ; r1 points to current char
	0112100, // 2010 nxtchr: MOVB (r1)+,r0  ; load xmt char
	0001405, // 2012         BEQ done       ; string is terminated with 0
	0110062, // 2014         MOVB r0,2(r2)  ; write char to transmit buffer
	0000002, // 2016
	0105712, // 2020 wait:   TSTB (r2)      ; character transmitted?
	0100376, // 2022         BPL wait       ; no, loop
	0000771, // 2024         BR nxtchr      ; transmit next character
	0000000, // 2026         halt
	0000763} // 2030         br start
// 2032         "Hello, World!" ; message

func main() {
	p := pdp11.New("True")
	boot := pdp11.NewBoot(p.Reg, p.Ram)
	boot.LoadMachineCode(HelloWorld, 02000)
	address := 02032
	for _, c := range []byte("Hello, World!\nHello, World!\nHello, World!\x00") {
		p.Ram.WriteByteAt(address, int(c))
		address++
	}
	p.Ram.Dump(02000, address)
	p.Reg.SetPC(02000)
	p.Run(100000)
}
