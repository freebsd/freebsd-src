#!/bin/sh

BUFFER_ETF_A53=ec802000.etf
BUFFER_ETF_A73=ed002000.etf
BUFFER_ETF_SYS=ec036000.etf
BUFFER_ETR=ec033000.etr

OUT_FILE=perf.data

STROBE=

while :; do
  case $1 in
    --strobe)
      STROBE=y
      WINDOW=$2
      PERIOD=$3
      shift 3
      ;;
    
    *)
      break ;;
  esac
done

case $1 in
  etr)
    BUFFER=$BUFFER_ETR
    ;;

  etf-sys)
    BUFFER=$BUFFER_ETF_SYS
    ;;

  "")
    BUFFER=$BUFFER_ETR
    ;;

  *)
    BUFFER=$1
    ;;
esac

shift 1

case $0 in 
 /*) F=$0 ;;
 *) F=$(pwd)/$0 ;;
esac

SCRIPT_DIR=$(dirname $F)

if [ "$STROBE" ]; then
  for e in /sys/bus/coresight/devices/*.etm/; do
    printf "%x" $WINDOW | sudo tee $e/strobe_window > /dev/null
    printf "%x" $PERIOD | sudo tee $e/strobe_period > /dev/null
  done
fi

PERF=$SCRIPT_DIR/perf

export LD_LIBRARY_PATH=$SCRIPT_DIR:$LD_LIBRARY_PATH

sudo LD_LIBRARY_PATH=$SCRIPT_DIR:$LD_LIBRARY_PATH $PERF record $PERF_ARGS -e cs_etm/@$BUFFER/u --per-thread "$@"

sudo chown $(id -u):$(id -g) $OUT_FILE


