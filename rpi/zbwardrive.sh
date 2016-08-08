PCAP_DIR=/tmp/pcaps
RUNDIR=/run/gpsd
NAME=gpsd
DAEMON=/usr/sbin/$NAME
GPSD_SOCKET=/var/run/gpsd.sock
GPSD_OPTIONS="$DEVICE -F $GPSD_SOCKET"

# mkdir -p $PCAP_DIR
mkdir -p $RUNDIR

service gpsd stop
# Run gpsd with correct params
$DAEMON $GPSD_OPTIONS > /tmp/gpsdout

(cd $PCAP_DIR && zbwardrive -s 2 -c 7 -v -g > /tmp/zbout)
