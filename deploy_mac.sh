# Host mode networking doesn't work on Docker Desktop for Mac/ Windows
# This script will work though
FS_DIR="$(pwd)/fs"

# Publish VNC server on port 5900 by default
# if $1 provided we use that for port offset from 5900
if [ -z "$1" ]
then
    VNC_PORT="0"
    echo "Running with default port (5900)"
else
    VNC_PORT=$1
fi

# TCP port is VNC "port" + 5900 (0 -> 5900, 1 -> 5901, etc.)
VNC_PORT_TCP=$((VNC_PORT + 5900))

# Will use password specified by VNC_PASSWORD
if [ -z "$VNC_PASSWORD" ]
then
    # Default password is pwny
    VNC_PASSWORD="pwny"
    echo "Running with default password..."
fi

docker run -it --rm \
    --name "deployment-$1" \
    --volume="$FS_DIR:/fs:ro" \
    -w /fs \
    -e VNC_PASSWORD=$VNC_PASSWORD \
    -e VNC_PORT=$VNC_PORT \
    --publish 127.0.0.1:$VNC_PORT_TCP:$VNC_PORT_TCP/tcp \
    osdev /bin/bash /fs/start_os_vnc.sh
