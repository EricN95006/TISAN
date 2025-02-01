#
# This is the Makefile for the TISAN programs.  This
# Typing 'make' will compiler everything.
#
# Use 'make index' to rebuild the inputs index file, which is not done by default
#
# The files TISAN.HLP and TISAN.IDX are created here and the copied up one level. The images in this
# directory are the ones used for checking if the need to be remade. 
#

# define some Makefile variables for the compiler and compiler flags
# to use Makefile variables later in the Makefile: $()
#
#  -g    adds debugging information to the executable file
#  -Wall turns on most, but not all, compiler warnings
#
# for C++ define  CC = g++
CC = gcc
CFLAGS  = -Wall
OBJECTS = tisanlib.o dos.o

# typing 'make' will invoke the first target entry in the file 
# (in this case the default target entry)
# you can name this target entry anything, but "default" or "all"
# are the most commonly used names by convention
#
default: help TISAN.HLP ../tisan ../DBCALC ../DBMOD ../DBTRANS ../DBX ../DBBLOCK ../PGRAM ../DCDFT ../DFT ../FFT ../KALMAN ../FIT ../DBSMOOTH ../DBCMB ../DBSUBSET ../HISTO ../DBCON ../DBLIST ../DBPLOT ../DBSORT ../DBSCALE ../IMEAN ../DBFIT ../DBBUILD ../TAFFY ../CANDY ./SUPPORT/addtask


# To create the executable file tisan we need the object files
# tisan.o tisanlib.o dos.o verbs.o pverbs.o
#
../tisan: tisan.o tisanlib.o dos.o verbs.o pverbs.o
	$(CC) $(CFLAGS) -o ../tisan tisan.o tisanlib.o dos.o verbs.o pverbs.o

# To create the object files above we need
# files tisan.c atcs.h dos.h main.h tisan.h
#
tisan.o: tisan.c atcs.h dos.h main.h tisan.h
	$(CC) $(CFLAGS) -c tisan.c

tisanlib.o: tisanlib.c atcs.h dos.h main.h tisan.h
	$(CC) $(CFLAGS) -c tisanlib.c

dos.o: dos.c atcs.h dos.h main.h tisan.h
	$(CC) $(CFLAGS) -c dos.c

verbs.o: verbs.c atcs.h dos.h main.h tisan.h
	$(CC) $(CFLAGS) -c verbs.c

pverbs.o: pverbs.c atcs.h dos.h main.h tisan.h
	$(CC) $(CFLAGS) -c pverbs.c
#
# Tasks
#
../DBCALC: dbcalc.c $(OBJECTS)
	$(CC) $(FLAGS) dbcalc.c  -Wall -o ../DBCALC $(OBJECTS)

../DBMOD: dbmod.c $(OBJECTS)
	$(CC) $(FLAGS) dbmod.c  -Wall -o ../DBMOD $(OBJECTS)

../DBTRANS: dbtrans.c $(OBJECTS)
	$(CC) $(FLAGS) dbtrans.c  -Wall -o ../DBTRANS $(OBJECTS)

../DBX: dbx.c $(OBJECTS)
	$(CC) $(FLAGS) dbx.c  -Wall -o ../DBX $(OBJECTS)

../DBBLOCK: dbblock.c $(OBJECTS)
	$(CC) $(FLAGS) dbblock.c  -Wall -o ../DBBLOCK $(OBJECTS)

../PGRAM: pgram.c $(OBJECTS)
	$(CC) $(FLAGS) pgram.c  -Wall -o ../PGRAM $(OBJECTS)

../DCDFT: dcdft.c $(OBJECTS)
	$(CC) $(FLAGS) dcdft.c  -Wall -o ../DCDFT $(OBJECTS)

../DFT: dft.c $(OBJECTS)
	$(CC) $(FLAGS) dft.c  -Wall -o ../DFT $(OBJECTS)

../FFT: fft.c $(OBJECTS)
	$(CC) $(FLAGS) fft.c  -Wall -o ../FFT $(OBJECTS)

../KALMAN: kalman.c $(OBJECTS)
	$(CC) $(FLAGS) kalman.c  -Wall -o ../KALMAN $(OBJECTS)

../FIT: fit.c $(OBJECTS)
	$(CC) $(FLAGS) fit.c  -Wall -o ../FIT $(OBJECTS)

../DBSMOOTH: dbsmooth.c $(OBJECTS)
	$(CC) $(FLAGS) dbsmooth.c  -Wall -o ../DBSMOOTH $(OBJECTS)

../DBCMB: dbcmb.c $(OBJECTS)
	$(CC) $(FLAGS) dbcmb.c  -Wall -o ../DBCMB $(OBJECTS)

../DBSUBSET: dbsubset.c $(OBJECTS)
	$(CC) $(FLAGS) dbsubset.c  -Wall -o ../DBSUBSET $(OBJECTS)

../HISTO: histo.c $(OBJECTS)
	$(CC) $(FLAGS) histo.c  -Wall -o ../HISTO $(OBJECTS)

../DBCON: dbcon.c $(OBJECTS)
	$(CC) $(FLAGS) dbcon.c  -Wall -o ../DBCON $(OBJECTS)

../DBLIST: dblist.c $(OBJECTS)
	$(CC) $(FLAGS) dblist.c  -Wall -o ../DBLIST $(OBJECTS)

../DBPLOT: dbplot.c tisanfnt.h $(OBJECTS)
	$(CC) $(FLAGS) dbplot.c  -Wall -o ../DBPLOT $(OBJECTS)

../DBSORT: dbsort.c $(OBJECTS)
	$(CC) $(FLAGS) dbsort.c  -Wall -o ../DBSORT $(OBJECTS)

../DBSCALE: dbscale.c $(OBJECTS)
	$(CC) $(FLAGS) dbscale.c  -Wall -o ../DBSCALE $(OBJECTS)

../IMEAN: imean.c $(OBJECTS)
	$(CC) $(FLAGS) imean.c  -Wall -o ../IMEAN $(OBJECTS)

../DBFIT: dbfit.c $(OBJECTS)
	$(CC) $(FLAGS) dbfit.c  -Wall -o ../DBFIT $(OBJECTS)

../DBBUILD: dbbuild.c $(OBJECTS)
	$(CC) $(FLAGS) dbbuild.c  -Wall -o ../DBBUILD $(OBJECTS)

../TAFFY: taffy.c $(OBJECTS)
	$(CC) $(FLAGS) taffy.c  -Wall -o ../TAFFY $(OBJECTS)

../CANDY: candy.c $(OBJECTS)
	$(CC) $(FLAGS) candy.c  -Wall -o ../CANDY $(OBJECTS)

#
# Help Files
#
help: help.o dos.o tisanlib.o
	$(CC) $(CFLAGS) -o help help.o $(OBJECTS)

help.o: help.c atcs.h dos.h
	$(CC) $(CFLAGS) -c help.c

TISAN.HLP: help.txt
	./help
	cp TISAN.HLP ..
#
# Task management files
#
./SUPPORT/addtask: addtask.c $(OBJECTS)
	$(CC) $(FLAGS) addtask.c  -Wall -o ./SUPPORT/addtask $(OBJECTS)

#
# Inputs Index file must be created with 'make index'. It is not created automatically based on editing.
#
index: ./SUPPORT/*.TXT
	$(RM) TISAN.IDX
	./SUPPORT/addtask ./SUPPORT/DBX
	./SUPPORT/addtask ./SUPPORT/DBCALC
	./SUPPORT/addtask ./SUPPORT/DBMOD
	./SUPPORT/addtask ./SUPPORT/DBSUBSET
	./SUPPORT/addtask ./SUPPORT/DBCMB
	./SUPPORT/addtask ./SUPPORT/DBTRANS
	./SUPPORT/addtask ./SUPPORT/DBSUBSET
	./SUPPORT/addtask ./SUPPORT/HISTO
	./SUPPORT/addtask ./SUPPORT/DBSORT
	./SUPPORT/addtask ./SUPPORT/DBSCALE
	./SUPPORT/addtask ./SUPPORT/DBCON
	./SUPPORT/addtask ./SUPPORT/DBLIST
	./SUPPORT/addtask ./SUPPORT/IMEAN
	./SUPPORT/addtask ./SUPPORT/PGRAM
	./SUPPORT/addtask ./SUPPORT/DCDFT
	./SUPPORT/addtask ./SUPPORT/DBMOD
	./SUPPORT/addtask ./SUPPORT/DBBLOCK
	./SUPPORT/addtask ./SUPPORT/DBPLOT
	./SUPPORT/addtask ./SUPPORT/DFT
	./SUPPORT/addtask ./SUPPORT/FFT
	./SUPPORT/addtask ./SUPPORT/FIT
	./SUPPORT/addtask ./SUPPORT/KALMAN
	./SUPPORT/addtask ./SUPPORT/DBSMOOTH
	./SUPPORT/addtask ./SUPPORT/DBFIT
	./SUPPORT/addtask ./SUPPORT/DBBUILD
	./SUPPORT/addtask ./SUPPORT/TAFFY
	./SUPPORT/addtask ./SUPPORT/CANDY
	cp TISAN.IDX ..
#
# To start over from scratch, type 'make clean'.  This
# removes the executable file, as well as old .o object
# files and *~ backup files:
#
clean: 
	$(RM) TISAN.HLP TISAN.IDX ../TISAN.HLP ../TISAN.IDX
	$(RM) help ./SUPPORT/addtask ../tisan
	$(RM) ../DBCALC ../DBMOD ../DBTRANS ../DBX ../DBBLOCK ../PGRAM
	$(RM) ../DCDFT ../DFT ../FFT ../KALMAN ../FIT ../DBSMOOTH ../DBCMB
	$(RM) ../DBSUBSET ../HISTO ../DBCON ../DBLIST ../DBPLOT ../DBSORT
	$(RM) ../DBSCALE ../IMEAN ../DBFIT ../DBBUILD
	$(RM) ../TAFFY ../CANDY
	$(RM) *.o *~
