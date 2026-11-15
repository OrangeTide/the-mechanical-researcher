# Lantern: static reliable-channel library for FreePascal.
# Builds the smoke test and the lanterngen IDL tool. Pure FreePascal, no
# external dependencies; the same recipe works on Linux, macOS, and Windows
# with fpc on PATH.

FPC      ?= fpc
BUILD    := build
FPCFLAGS := -Mobjfpc -O2 -Fusrc -FU$(BUILD)

.PHONY: all check gen clean

all: $(BUILD)/smoke $(BUILD)/lanterngen

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/smoke: examples/smoke.pas src/*.pas | $(BUILD)
	$(FPC) $(FPCFLAGS) examples/smoke.pas -o$(BUILD)/smoke

$(BUILD)/lanterngen: tools/lanterngen.pas | $(BUILD)
	$(FPC) $(FPCFLAGS) tools/lanterngen.pas -o$(BUILD)/lanterngen

# Run the sanity test.
check: $(BUILD)/smoke
	$(BUILD)/smoke

# Compile the example schema to a FreePascal unit.
gen: $(BUILD)/lanterngen
	$(BUILD)/lanterngen examples/game.pidl -o examples/GameProto.pas
	@echo "--- examples/GameProto.pas ---"
	@cat examples/GameProto.pas

clean:
	rm -rf $(BUILD) examples/GameProto.pas
