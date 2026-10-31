# Host simulation of the parallel protocol: real pb_proto.c, simulated pins
# and a model of the Amiga transfers.  make; make PB_PROTO_C=<old file> ARG=old
PB_PROTO_C ?= ../src/pb_proto.c
ARG ?=
BUILD = BUILD

all: run

$(BUILD)/sim: sim.c $(PB_PROTO_C) stub/par_low.h
	mkdir -p $(BUILD)
	cp $(PB_PROTO_C) $(BUILD)/pb_proto.c
	cp stub/par_low.h stub/timer.h stub/stats.h stub/uartutil.h $(BUILD)/
	cc -O1 -Wall -Wno-deprecated-declarations -DF_CPU=16000000UL \
	  -I$(BUILD) -Istub -I../src -o $@ sim.c $(BUILD)/pb_proto.c

run: $(BUILD)/sim
	$(BUILD)/sim $(ARG)

clean:
	rm -rf $(BUILD)

.PHONY: all run clean
