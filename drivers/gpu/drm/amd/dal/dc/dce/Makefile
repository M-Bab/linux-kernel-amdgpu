#
# Makefile for common 'dce' logic
# HW object file under this folder follow similar pattern for HW programming
#   - register offset and/or shift + mask stored in the dec_hw struct
#   - register programming through common macros that look up register 
#     offset/shift/mask stored in dce_hw struct

DCE = dce_audio.o

AMD_DAL_DCE = $(addprefix $(AMDDALPATH)/dc/dce/,$(DCE))

AMD_DAL_FILES += $(AMD_DAL_DCE)
