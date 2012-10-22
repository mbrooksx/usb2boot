IDIR =
CC=gcc
CFLAGS=

ODIR=obj
BINDIR=$(ODIR)

LDIR =

LIBS=

MKDIR=mkdir
.PHONY: dirs

_OBJ = usb2boot.o usb_linux.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: dirs usb2boot
usb2boot: $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -rf $(ODIR) *~ core $(INCDIR)/*~
	rm usb2boot

dirs: ${ODIR}

${ODIR}:
	mkdir -p ${ODIR}
