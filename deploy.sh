SRC_DIR="$(pwd)/src"
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
	--network host \
	--publish 127.0.0.1:5900:5900/tcp \
	--privileged \
	osdev /bin/bash ./start_os_vnc.sh
