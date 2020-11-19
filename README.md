# pwnyOS UIUCTF 2020
Hello! This is a toy OS I am writing for fun. This is the version of pwnyOS used in UIUCTF 2020.

# Environment
## Docker
Provided is a Docker container with all requirements for building the kernel. It is provided for your convenience but the kernel can be built without it on Linux.

To build the docker container, perform the following:
```
cd containers/devel
./build.sh
```

## Running Docker
To run the docker container, use the following:
```
./devel.sh
```

Within the devel container, all project source files will be automatically mapped into `/mac` where they can be editted or compiled. The filesystem directory `/fs` will be mapped to `/fs`.

# Building the Kernel
To build the project from within the Docker, simply do:
```
make
```

To make the project on Linux (not in the Docker) (starting in the project top-level directory), simply do:

```
cd src
make
```

# Running the Kernel
The kernel can be run in `qemu` (specifically, the `i386` or `x86_64` systems) using the following:

```
./start_os.sh
```

# Deployment
The kernel can be deployed through VNC in a Docker container. The existing `osdev` container can be reused for this purpose.

To deploy the OS in Docker as a VNC server, run the following (from top level):
```
./deploy.sh
```
Then, from within the container, run:
```
./start_os_vnc.sh
```

The VNC server will be running on port `5900` by default, with the default password of `pwny`. This password is configured as an environment variable in the `deploy.sh` script.

To connect to the VNC server, use any VNC client to connect to `localhost:5900`. Here's an example using the builtin macOS VNC Viewer:
```
open vnc://localhost:5900
```

When prompted for the password, enter the password as defined by `VNC_PASSWORD` in `deploy.sh`.
