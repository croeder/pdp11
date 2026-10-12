// pdp11.go - translation of pdp11.py / pdp11.c: the machine, instruction
// dispatch, and the processor loop.
//
// As in the fixed C version: CPU_cycles counts each instruction once (the
// Python counted twice), and a jump to itself is logged but does not halt.
// The PySimpleGUI consoles are not translated; Run uses the stdio Terminal.
package pdp11

import "fmt"

type PDP11 struct {
	SW        *StopWatches
	lock      *Lock
	Reg       *Registers
	Ram       *Ram
	PSW       *PSW
	Stack     *Stack
	AM        *AddressModes
	Br        *BrOps
	Noopr     *NooprOps
	Ss        *SsOps
	Ssdd      *SsddOps
	Rss       *RssOps
	Other     *OtherOps
	Cc        *CcOps
	executed  map[int]string // instruction -> "instruction,assembly"
	order     []int          // first-seen order, like a Python dict
	CPUCycles int
	run       bool
	Boot      *Boot
	M9301     *M9301
	DL11      *DL11
	Terminal  *Terminal
}

// New builds the machine: loads config_default.json, the M9301-YA ROM and
// a DL11 at 0o177560.  ui is logged only.
func New(ui string) *PDP11 {
	Logger()
	logInfof("pdp11CPU initializing with ui=%s", ui)
	p := &PDP11{SW: NewStopWatches(), lock: &Lock{}, executed: map[int]string{}}
	config := NewConfig("")
	p.Reg = NewRegisters()
	p.Ram = NewRam(p.lock, p.Reg, configInt(config.Lookup("ram", "bits")))
	p.PSW = NewPSW(p.Ram)
	p.Stack = NewStack(p.Reg, p.Ram, p.PSW)
	p.AM = NewAddressModes(p.Reg, p.Ram, p.PSW)
	p.Br = NewBrOps(p.Reg, p.PSW, p.SW)
	p.Noopr = NewNooprOps(p.Reg, p.PSW, p.Stack, p.SW)
	p.Ss = NewSsOps(p.Reg, p.PSW, p.AM, p.SW)
	p.Ssdd = NewSsddOps(p.Reg, p.PSW, p.AM, p.SW)
	p.Rss = NewRssOps(p.Reg, p.PSW, p.AM, p.SW)
	p.Other = NewOtherOps(p.Reg, p.Ram, p.PSW, p.AM, p.SW)
	p.Cc = NewCcOps(p.PSW, p.SW)
	p.Boot = NewBoot(p.Reg, p.Ram)
	p.M9301 = NewM9301(p.Reg, p.Ram, p.Boot)
	logInfof("pdp11CPU setting up DL111 at 0o177560")
	p.DL11 = NewDL11(p.Ram, 0177560)
	p.Terminal = NewTerminal(p.DL11, p.SW)
	logInfof("pdp11CPU initializing done")
	return p
}

func (p *PDP11) SetRun(run bool) {
	if p.run != run {
		p.run = run
		logInfof("set_run(%v)", run)
	}
}

func (p *PDP11) Running() bool { return p.run }

// Dispatch is dispatch_opcode.
func (p *PDP11) Dispatch(instruction int) OpResult {
	switch {
	case p.Cc.IsCcOp(instruction):
		return p.Cc.DoCcOp(instruction)
	case p.Br.IsBrOp(instruction):
		return p.Br.DoBrOp(instruction)
	case p.Noopr.IsNooprOp(instruction):
		return p.Noopr.DoNooprOp(instruction)
	case p.Ss.IsSsOp(instruction):
		return p.Ss.DoSsOp(instruction)
	case p.Rss.IsRssOp(instruction):
		return p.Rss.DoRssOp(instruction)
	case p.Ssdd.IsSsddOp(instruction):
		return p.Ssdd.DoSsddOp(instruction)
	}
	return p.Other.DoOtherOp(instruction)
}

// InstructionCycle runs one fetch-decode-execute cycle and reports whether
// the CPU should keep running.
func (p *PDP11) InstructionCycle() bool {
	p.SW.Start("instruction_cycle")
	pc := p.Reg.GetPC()
	instruction := p.Ram.ReadWordFromPC()
	r := p.Dispatch(instruction)
	if logEnabled(logDebug) {
		logDebugf("instruction_cycle: %d %s %s %s;%s NZVC:%s", p.CPUCycles, Oct6(pc), Oct6(instruction),
			Pad(r.Assembly, 20), p.Reg, p.PSW.NZVC())
	}
	if pc == p.Reg.GetPC() {
		// A jump to itself is a legitimate wait loop, so this does not halt.
		logInfof("instruction_cycle: pc was not changed at %s", PyOct(pc))
	}
	p.SW.Stop("instruction_cycle")
	key := instruction & MaskWord
	if _, seen := p.executed[key]; !seen {
		p.order = append(p.order, key)
	}
	p.executed[key] = fmt.Sprintf("%d,%s", instruction, r.Assembly)
	p.CPUCycles++
	return r.Run
}

// Run is pdp11Run.run: run until the CPU stops or limit instructions
// (limit < 0: no limit), cycling the terminal after each instruction.
func (p *PDP11) Run(limit int) {
	logInfof("run: begin PDP11 emulator")
	logInfof("%s NZVC:%s", p.Reg, p.PSW.NZVC())
	p.SetRun(true)
	p.CPUCycles = 0
	p.SW.Start("CPU")
	for p.Running() {
		p.SetRun(p.InstructionCycle())
		p.Terminal.Cycle()
		if limit >= 0 && p.CPUCycles > limit {
			logInfof("run: instruction limit reached")
			p.SetRun(false)
		}
	}
	p.SW.Stop("CPU")
	logInfof("run: stop PDP11 emulator")
	p.AM.Report()
	p.SW.Report()
	runTime := float64(p.SW.Watch("CPU").Sum) / 1e9
	logInfof("run_time:%g", runTime)
	speed := 0.0
	if runTime > 0 {
		speed = float64(p.CPUCycles) / runTime
	}
	logInfof("self.pdp11.CPU_cycles:%d", p.CPUCycles)
	logInfof("processor speed: %d cycles / %g seconds = %5.0f instructions per second", p.CPUCycles, runTime, speed)
	logInfof("instructions executed:")
	for _, k := range p.order {
		logInfof("%s", p.executed[k])
	}
	logInfof("instructions executed report ends")
}
