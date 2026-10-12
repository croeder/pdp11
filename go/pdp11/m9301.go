// m9301.go - translation of pdp11_m9301 (M9301 boot ROM card) and
// pdp11_rk11 (the stubbed RK11 disk interface).
package pdp11

// M9301 loads the M9301-YA ROM image and starts at its console / CPU
// diagnostics entry point (no DIP switches on, so PC = 0o173000).  Reads of
// 0o173024 return the switch settings.
type M9301 struct {
	BaseAddress    int
	SwitchAddress  int
	SwitchSettings int
}

func NewM9301(reg *Registers, ram *Ram, boot *Boot) *M9301 {
	logInfof("initializing M9301")
	m := &M9301{BaseAddress: 0173000, SwitchAddress: 0173024}
	const powerUpReboot = true // switch 81-2
	on := 0                    // the address switches 81-3 .. 81-10 are all off
	if powerUpReboot {
		boot.ReadPDP11AssemblyFile("source/M9301-YA.txt")
		m.SwitchSettings = (0400 & on) + (0200 & on) + (0100 & on) + (0040 & on) +
			(0020 & on) + (0010 & on) + (0004 & on) + (0002 & on) + m.BaseAddress
		logInfof("switch_address:%s switch_settings:%s", PyOct(m.SwitchAddress), PyOct(m.SwitchSettings))
		reg.SetPC(m.SwitchSettings)
	} else {
		reg.SetPC(024)
	}
	ram.RegisterIOReader(m.SwitchAddress, m.readDIPSwitches, "read_dip_switches")
	logInfof("initializing M9301 done. PC:%s", PyOct(reg.GetPC()))
	return m
}

func (m *M9301) readDIPSwitches() int {
	logInfof("                                  ; read_dip_switches returns %s", PyOct(m.SwitchSettings))
	return m.SwitchSettings
}

// RK11 is the DEC RK11 disk interface.  *** stubbed out. Inoperative.
type RK11 struct {
	RKCS, RKWC, RKBA, RKDA, RKMR, RKDB int
}

func NewRK11(ram *Ram) *RK11 {
	logInfof("initializing RK11")
	k := &RK11{}
	base := 0177400
	regs := []struct {
		offset int
		name   string
		field  *int
	}{{004, "RKCS", &k.RKCS}, {006, "RKWC", &k.RKWC}, {010, "RKBA", &k.RKBA},
		{012, "RKDA", &k.RKDA}, {014, "RKMR", &k.RKMR}, {016, "RKDB", &k.RKDB}}
	for _, r := range regs {
		field, name := r.field, r.name
		ram.RegisterIOWriter(base+r.offset, func(v int) { logInfof("RK11 write_%s", name); *field = v }, "write_"+name)
		ram.RegisterIOReader(base+r.offset, func() int { logInfof("RK11 read_%s", name); return *field }, "read_"+name)
	}
	ram.RegisterIOReader(base, func() int { logInfof("RK11 read_RKDS"); return 0 }, "read_RKDS")
	ram.RegisterIOReader(base+002, func() int { logInfof("RK11 read_RKER"); return 0 }, "read_RKER")
	logInfof("initializing RK11 done")
	return k
}
