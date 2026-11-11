# Requiere ps2sdk (PS2SDK y PS2DEV definidos) y gsKit instalado en $(PS2DEV)/gsKit
GSKIT ?= $(PS2DEV)/gsKit

EE_BIN = prisma.elf
EE_OBJS = main.o model_data.o
EE_INCS += -I$(GSKIT)/include
EE_LDFLAGS += -L$(GSKIT)/lib
EE_LIBS += -lgskit -ldmakit -lc -lm

all: $(EE_BIN)

model_data.c: model.pm
	$(PS2SDK)/bin/bin2c $< $@ model_pm

clean:
	rm -f $(EE_BIN) $(EE_OBJS) model_data.c

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
