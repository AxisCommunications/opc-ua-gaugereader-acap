*Copyright (C) 2022, Axis Communications AB, Lund, Sweden. All Rights Reserved.*

# OPC UA Gauge Reader ACAP

[![Build ACAPs](https://github.com/AxisCommunications/opc-ua-gaugereader-acap/actions/workflows/build.yml/badge.svg)](https://github.com/AxisCommunications/opc-ua-gaugereader-acap/actions/workflows/build.yml)
[![GitHub Super-Linter](https://github.com/AxisCommunications/opc-ua-gaugereader-acap/actions/workflows/super-linter.yml/badge.svg)](https://github.com/AxisCommunications/opc-ua-gaugereader-acap/actions/workflows/super-linter.yml)

This repository contains the source code to build a small ACAP application that
reads an analogue gauge using video analytics and exposes the value through an
[OPC UA](https://en.wikipedia.org/wiki/OPC_Unified_Architecture)
([open62541](https://open62541.org/)) server.

The exposed value is in percent.

## Build

### On developer computer with ACAP SDK installed

```sh
# With the environment initialized, use:
acap-build .
```

### Using ACAP SDK build container and Docker

The handling of this is integrated in the [Makefile](Makefile), so if you have
Docker on your computer all you need to do is:

```sh
make dockerbuild
```

or perhaps build in parallel:

```sh
make -j dockerbuild
```

If you do have Docker but no `make` on your system:

```sh
# 32-bit ARM
DOCKER_BUILDKIT=1 docker build --build-arg ARCH=armv7hf -o type=local,dest=. .
# 64-bit ARM
DOCKER_BUILDKIT=1 docker build --build-arg ARCH=aarch64 -o type=local,dest=. .
```

## Setup

### Manual installation and configuration

Open the ACAP's settings page in the web interface. Simply click in the image
to set up the calibration points in the following order:

1. Center of the gauge
1. Minimum value of the gauge
1. Maximum value of the gauge

*By default, gauges are assumed to be clockwise but the ACAP has a parameter for
this that can be set to `False` for counterclockwise gauges.*

The OPC UA server port is set, just like the clockwise/counterclockwise, as any
regular ACAP parameters. (And the calibration points can be set that way too.)

### Scripted installation and configuration

Use the device's
[param.cgi](https://www.axis.com/vapix-library/subjects/t10175981/section/t10036014/display)
to set the center/min/max points, as well as clockwise/counterclockwise and the
OPC UA server port number.

## Usage

Attach an OPC UA client to the port set in the ACAP. The client will then be
able to read the value (and its timestamp) from the ACAP's OPC UA server.

The ACAP will also log the gauge value in the camera's syslog.

## License

[Apache 2.0](LICENSE)
