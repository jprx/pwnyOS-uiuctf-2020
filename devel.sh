SRC_DIR="$(pwd)/src"
FS_DIR="$(pwd)/fs"

# Get name of current kernel
unameVal="$(uname -s)"

# Docker Desktop for Mac uses a different port name than Linux does for GDB
# Just detect host type and pass appropriate port into devel container
if [[ $unameVal == *"Darwin"* ]]; then
    DEBUG_HOST="host.docker.internal:1234"
elif [[ $unameVal == *"Linux"* ]]; then
    DEBUG_HOST="localhost:1234"
else
    # @TODO: Windows debugging?
    echo "Unsupported OS for debugging, debug might not work"
    DEBUG_HOST="localhost:1234"
fi

# Need to run as --privileged to allow for mounting loopback drives
docker run -it --rm \
	--name "development" \
	--volume="$SRC_DIR:/mac:rw" \
	--volume="$FS_DIR:/fs:rw" \
	--privileged \
	-w /mac \
	--network="host" \
    -e DEBUG_HOST=$DEBUG_HOST \
	osdev /bin/bash
