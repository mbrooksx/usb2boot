IDIR =
CC=gcc
CFLAGS=

ODIR=obj
BINDIR=$(ODIR)

LDIR =

LIBS=

MKDIR=mkdir

_OBJ = usb2boot.o usb_linux.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

usb2boot: $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~ 


