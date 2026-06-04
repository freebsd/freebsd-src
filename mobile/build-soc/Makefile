# Makefile for BSD Mobile SoC build system

.PHONY: all soc-list soc-board build help clean

# Supported SoCs
SOCs := MT6893 MT6877 MT6833 MT6895 MT6983 MT6985 MT6989 MT6991 \
        SM8450 SM8350 SM7450 SM7350 SM6375 SM6450 SM6625 MSM8998 \
        S5E9925 S5E9810 S5E8845 S5E8535 S5E9830 S5E9825 S5E5510 \
        TENSOR_REDFIN TENSOR_BLUEJAY TENSOR_ZUMA

# Board mappings
MT6893_BOARDS := X01BD mojito
MT6877_BOARDS := veux
MT6833_BOARDS := evergo
MT6895_BOARDS := plato
MT6983_BOARDS := cupid
MT6985_BOARDS := n8q
MT6989_BOARDS := houjiao
MT6991_BOARDS := v2309

SM8450_BOARDS := lavender lemonade
SM8350_BOARDS := lilac
SM7450_BOARDS := zen22
SM7350_BOARDS := rhodium
SM6375_BOARDS := pong
SM6450_BOARDS := ocean
SM6625_BOARDS := generic_lp5
MSM8998_BOARDS := dumpling

S5E9925_BOARDS := lavender g0 tq
S5E9810_BOARDS := exynos2100 o1s
S5E8845_BOARDS := a54x
S5E8535_BOARDS := a14x
S5E9830_BOARDS := s5e9830 c2s
S5E9825_BOARDS := c2 beyond
S5E5510_BOARDS := a04e

TENSOR_REDFIN_BOARDS := oriole raven bluejay-alt
TENSOR_BLUEJAY_BOARDS := panther cheetah bluejay
TENSOR_ZUMA_BOARDS := shiba felix husky

# Default target
all:
	@echo "Use 'make build' with SOC and BOARD variables"
	@echo "Example: make build SOC=MT6893 BOARD=X01BD"

# List all supported SoCs
soc-list:
	@echo "Supported SoCs:"
	@for soc in $(SOCs); do \
		echo "  - $$soc"; \
	done

# List boards for a given SoC
soc-board:
ifndef SOC
	@echo "Error: SOC variable required (e.g., make soc-board SOC=MT6893)"
	@exit 1
endif
	@echo "Boards for $(SOC):"
	@echo "$($(SOC)_BOARDS)"

# Build target
build:
ifndef SOC
	@echo "Error: SOC variable required"
	@exit 1
endif
ifndef BOARD
	@echo "Error: BOARD variable required"
	@exit 1
endif
	@./build.sh --soc $(SOC) --board $(BOARD) --output out/$(SOC)/

# Apply patches
patch:
ifndef SOC
	@echo "Error: SOC variable required"
	@exit 1
endif
ifndef BOARD
	@echo "Error: BOARD variable required"
	@exit 1
endif
	@./apply_patch.sh --soc $(SOC) --board $(BOARD)

# Generate boot image
bootimg:
	@./gen_bootimg.sh --soc $(SOC) --kernel $(KERNEL) --ramdisk $(RAMDISK) --out $(OUT)

# Create super partition
super:
	@./create_super.sh --soc $(SOC) --output $(OUTPUT) --fs $(FS)

# Flash device
flash:
ifndef SOC
	@echo "Error: SOC variable required"
	@exit 1
endif
	@./flash.sh --soc $(SOC) --mode $(MODE) --output out/$(SOC)/

# Clean output
clean:
	rm -rf out/

# Help
help:
	@echo "BSD Mobile SoC Build System"
	@echo ""
	@echo "Targets:"
	@echo "  soc-list        - Print all supported SoCs"
	@echo "  soc-board SOC=X - Print all boards for a given SoC"
	@echo "  build           - Build for specific SoC/BOARD (requires SOC and BOARD variables)"
	@echo "  patch           - Apply SoC-specific patches (requires SOC and BOARD variables)"
	@echo "  bootimg         - Generate boot image (requires KERNEL, RAMDISK, OUT variables)"
	@echo "  super           - Create super partition (requires SOC, OUTPUT, FS variables)"
	@echo "  flash           - Flash device (requires SOC and MODE variables)"
	@echo "  clean           - Clean output directory"
	@echo ""
	@echo "Supported SoCs: $(SOCs)"
	@echo ""
	@echo "Examples:"
	@echo "  make build SOC=MT6893 BOARD=X01BD"
	@echo "  make soc-board SOC=SM8450"
	@echo "  make flash SOC=MQM8998 MODE=fastboot"