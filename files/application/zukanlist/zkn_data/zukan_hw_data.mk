ZUKAN_HW_DATA_DIR := files/application/zukanlist/zkn_data/zukan_hw_data
ZUKAN_HW_DATA_NARC := $(ZUKAN_HW_DATA_DIR)_$(shortname).narc
ZUKAN_HW_DATA_VER_NAIX := $(ZUKAN_HW_DATA_DIR)_$(shortname).naix
ZUKAN_HW_DATA_NAIX := $(ZUKAN_HW_DATA_DIR).naix

# The NARC tool writes the version NAIX as a side effect of creating the narc, so both are outputs of
# this recipe.  Naming only the narc left the Silver build stuck: that narc is committed, so the recipe
# is up to date, the index is never produced, and the NAIX rule below has no input.
$(ZUKAN_HW_DATA_NARC) $(ZUKAN_HW_DATA_VER_NAIX): $(ZUKAN_HW_DATA_DIR)/zukan_hw_data_0.bin $(ZUKAN_HW_DATA_DIR)/zukan_hw_data_1_$(shortname).bin
	ln -sf zukan_hw_data_1_$(shortname).bin $(ZUKAN_HW_DATA_DIR)/zukan_hw_data_1.bin
	$(NARC) -cf $(ZUKAN_HW_DATA_NARC) --index-namespace --exclude="zukan_hw_data_1_*.bin" $(ZUKAN_HW_DATA_DIR)

# Normalize the NAIX to version-agnostic enums.  The version NAIX is the real input here (the recipe
# above produces it), so it is the prerequisite: depending on the narc instead left the Silver build
# reading an index that nothing had asked make to produce.
$(ZUKAN_HW_DATA_NAIX): $(ZUKAN_HW_DATA_VER_NAIX)
	$(SED) 's/_$(shortname)//g' $< > $@
filesystem files_for_compile: $(ZUKAN_HW_DATA_NAIX)

clean-zukan-hw-data:
	$(RM) $(ZUKAN_HW_DATA_NARC) $(ZUKAN_HW_DATA_NAIX)

.PHONY: clean-zukan-hw-data
clean-filesystem: clean-zukan-hw-data
