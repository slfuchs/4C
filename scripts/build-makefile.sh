
# set linker to CP if C++ is used
if [ "x$USES_CPP" = "x1" ]; then
  if [ "x$CP" = "x" ] ; then
        echo $0: Error: C++ used, but no C++ compiler available on this platform!!
        exit 1
  else
    LD=$CP
    PLAIN_LD=$PLAIN_CP
  fi
fi

# set linker to cpp if trilinos is used
if grep '^TRILINOS_PACKAGE' "$definefile" 2>&1 > /dev/null ; then
  if [ "x$CP" = "x" ] ; then
        echo $0: Error: C++ used, but no C++ compiler available on this platform!!
        exit 1
  else
    LD=$CP
    LDFLAGS=-Wall
    LIBS="$LIBS -lz  -lc -lm -lg2c"
    PLAIN_LD=$PLAIN_CP
  fi
fi



# build the Makefile
# This is done in three steps

# step 1: store all user supplied variables
#
cat > $makefile <<EOF
#
# Variables
#
SRC=$SRC
DEST=$DEST

PLAIN_CC=$PLAIN_CC
PLAIN_CP=$PLAIN_CP
PLAIN_F77=$PLAIN_F77
PLAIN_LD=$PLAIN_LD

CC=$CC
CP=$CP
F77=$F77
LD=$LD

PROGRAM=$PROGRAMNAME

CFLAGS=$CFLAGS $DEBUGFLAG -D$PLATFORM $DEFINES
CPFLAGS=$CPFLAGS $DEBUGFLAG -D$PLATFORM $DEFINES
FFLAGS=$FFLAGS $DEBUGFLAG -D$PLATFORM $DEFINES

LDFLAGS=$LDFLAGS
INCLUDES=$INCLUDEDIRS
LIBS=$LIBDIRS $LIBS
PLAIN_LIBS=$LIBDIRS $PLAIN_LIBS

VISUAL2_LIB=$VISUAL2_LIB
#VISUAL2_INC=$VISUAL2_INC

VISUAL3_LIB=$VISUAL3_LIB
#VISUAL3_INC=$VISUAL3_INC

#----------------------- binaries -----------------------------------
include \$(SRC)/Makefile.objects
#--------------------------------------------------------------------
#
# The main rule called when no arguments are given
ccarat: \$(PROGRAM)

# Build (nearly) everything.
# Some filters (like the visual ones) are very specific and system dependent
# and thus not included here. This rule is supposed to work everywhere.
all: \$(PROGRAM) post_gid_txt post_out post_monitor post_file_manager

\$(PROGRAM): \\
		$OBJECTS
		@echo "Linking \$(LD) \$(LDFLAGS) \$(LIBS) \$(INCLUDES) -o \$(PROGRAM)"
		@\$(LD) \$(LDFLAGS) \\
		$OBJECTS \\
		\$(LIBS)  -o \$(PROGRAM)
		@echo "\$(PROGRAM) successfully built"
EOF


# step 2: copy Makefile.in
cat $SRC/Makefile.in >> $makefile
#awk '$1 !~ /include/ { print $0 } $1 ~ /include/ { system("cat " $2)}' Makefile.in >> Makefile

# step 3: build dependencies if gcc can be found
#
# you can skip this step by saying
# $ NODEPS=yes ./configure ...
#
if [ x$NODEPS != "xyes" ] ; then
  # hopefully nobody translates which's messages...
  #if which makedepend | grep '^no ' 2>&1 > /dev/null ; then
  #  echo $0: makedepend not found. Use gcc to create dependencies.
    if which gcc | grep '^no ' 2>&1 > /dev/null ; then
       echo $0: gcc not found. No dependencies generated. Use Makefile with care.
    else
      for file in `find $SRC/src -name "*.c"` ; do
        echo "build deps for" $file
        gcc -D$PLATFORM $DEFINES -MM -MT `echo $file|sed -e 's,c$,o,' -e "s,$SRC,$DEST,"` -I`dirname $file|sed -e "s,$SRC,$DEST,"` $INCLUDEDIRS $file >> $makefile
      done
    fi

  if which g++ | grep '^no ' 2>&1 > /dev/null ; then
     echo $0: g++ not found. No dependencies generated. Use Makefile with care.
  else
    for file in `find $SRC/src -name "*.cpp"` ; do
      echo "build deps for" $file
      g++ -D$PLATFORM $DEFINES -MM -MT `echo $file|sed -e 's,cpp$,o,' -e "s,$SRC,$DEST,"` -I`dirname $file|sed -e "s,$SRC,$DEST,"` $INCLUDEDIRS $file >> $makefile
    done
  fi

fi
