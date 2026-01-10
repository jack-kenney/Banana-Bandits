V=1
SOURCE_DIR=src
BUILD_DIR=build
ASSETS_DIR = assets
FILESYSTEM_DIR = filesystem
MODEL_LIST  = $(wildcard $(ASSETS_DIR)/*.glb)

# collect PNG images so they get converted to .sprite targets (e.g. Purple.png -> Purple.sprite)
IMAGE_LIST = $(wildcard $(ASSETS_DIR)/*.png) $(wildcard $(ASSETS_DIR)/core/*.png)

# Warn when no GLB sources found (helps explain why asset rules don't run)
ifeq ($(strip $(MODEL_LIST)),)
	$(warning No .glb files found in $(ASSETS_DIR) — ASSETS_LIST may be empty)
endif

# Add a helper target to print the model/asset lists from make
.PHONY: print-assets
print-assets:
	@echo "ASSETS_DIR = $(ASSETS_DIR)"
	@echo "FILESYSTEM_DIR = $(FILESYSTEM_DIR)"
	@echo "MODEL_LIST = $(MODEL_LIST)"
	@echo "ASSETS_LIST = $(ASSETS_LIST)"

include $(N64_INST)/include/n64.mk
include $(N64_INST)/include/t3d.mk

ASSETS_LIST += $(subst $(ASSETS_DIR),$(FILESYSTEM_DIR),$(MODEL_LIST:%.glb=%.t3dm))
ASSETS_LIST += $(subst $(ASSETS_DIR),$(FILESYSTEM_DIR),$(IMAGE_LIST:%.png=%.sprite))

# Ensure a DFS artifact is produced from the assets; included n64.mk may provide the recipe.
$(BUILD_DIR)/hello.dfs: $(ASSETS_LIST)

# Make the ROM depend on the packaged DFS so assets are packaged before z64 creation
hello.z64: $(BUILD_DIR)/hello.dfs

all: hello.z64
.PHONY: all

hello.z64: $(ASSETS_LIST)

$(FILESYSTEM_DIR)/%.sprite: $(ASSETS_DIR)/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	$(N64_MKSPRITE) $(MKSPRITE_FLAGS) -o $(dir $@) "$<"

$(FILESYSTEM_DIR)/%.t3dm: $(ASSETS_DIR)/%.glb
	@mkdir -p $(FILESYSTEM_DIR)
	@echo "    [T3D-MODEL] $@"
	$(T3D_GLTF_TO_3D) $(T3DM_FLAGS) "$<" $@
	$(N64_BINDIR)/mkasset -c 2 -o $(FILESYSTEM_DIR) $@

OBJS = $(BUILD_DIR)/main.o \
       $(BUILD_DIR)/player.o \
       $(BUILD_DIR)/weapon.o


hello.z64: N64_ROM_TITLE="Banana Bandits"

$(BUILD_DIR)/hello.elf: $(OBJS)

clean:
	rm -f $(BUILD_DIR)/* *.z64
.PHONY: clean

-include $(wildcard $(BUILD_DIR)/*.d)

