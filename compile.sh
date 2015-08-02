#!/bin/bash

BAKAPIDIR="$BLAH/bak_api"
SHMDIR="$BLAH/sharedmem"
ICECDIR="$BLAH/icec"

CFLAGS="-Wall -O2 -fPIC -I${BAKAPIDIR} -I${SHMDIR} -I${ICECDIR}"
LFLAGS="-lrt -lpthread"

gcc $CFLAGS icerec.c ${BAKAPIDIR}/getopts.c ${SHMDIR}/shared.c ${ICECDIR}/icelock.c ${ICECDIR}/picInterface.c $LFLAGS -o icerec || exit 3
#gcc $CFLAGS iceplay.c ${BAKAPIDIR}/getopts.c ${SHMDIR}/shared.c ${ICECDIR}/icelock.c ${ICECDIR}/picInterface.c $LFLAGS -o iceplay || exit 4
