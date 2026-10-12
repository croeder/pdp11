// stopwatches.go - translation of stopwatches: per-name timing of code sections.
package pdp11

import (
	"fmt"
	"time"
)

type StopWatch struct {
	ID        string
	Count     int
	Min       int64
	Max       int64
	Sum       int64
	Mean      float64
	startTime time.Time
	Duration  int64
}

func newStopWatch(id string) *StopWatch {
	return &StopWatch{ID: id, Min: 1000000000000, Max: -1, Mean: -1, startTime: time.Now()}
}

func (w *StopWatch) stop(t time.Time) {
	w.Duration = t.Sub(w.startTime).Nanoseconds()
	if w.Duration < w.Min {
		w.Min = w.Duration
	}
	if w.Duration > w.Max {
		w.Max = w.Duration
	}
	w.Sum += w.Duration
	w.Count++
	w.Mean = float64(w.Sum) / float64(w.Count)
}

func (w *StopWatch) String() string {
	return fmt.Sprintf("%-20s%12.0f%12.0f%12.0f%12.0f%12d", w.ID, float64(w.Min)/1000, w.Mean/1000,
		float64(w.Max)/1000, float64(w.Sum)/1000, w.Count)
}

// StopWatches keeps watches by name, reported in first-started order.
type StopWatches struct {
	watches map[string]*StopWatch
	order   []string
}

func NewStopWatches() *StopWatches { return &StopWatches{watches: map[string]*StopWatch{}} }

func (s *StopWatches) Start(id string) {
	w, ok := s.watches[id]
	if !ok {
		w = newStopWatch(id)
		s.watches[id] = w
		s.order = append(s.order, id)
	}
	w.startTime = time.Now()
}

func (s *StopWatches) Stop(id string) {
	t := time.Now()
	if w, ok := s.watches[id]; ok {
		w.stop(t)
	} else {
		logInfof("WARN: StopWatches stop could not find id %s", id)
	}
}

func (s *StopWatches) Watch(id string) *StopWatch {
	w, ok := s.watches[id]
	if !ok {
		raise("KeyError", id)
	}
	return w
}

func (s *StopWatches) Report() {
	logInfof("StopWatches Report (times in microseconds)")
	logInfof("id                      min        mean         max         sum       count")
	for _, id := range s.order {
		logInfof("%s", s.watches[id])
	}
	logInfof("StopWatches Report end")
}
