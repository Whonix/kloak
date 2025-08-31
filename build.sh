#!/bin/bash

## For CodeQL autobuild

set -x
set -e

sudo --non-interactive apt-get update --error-on=any
sudo --non-interactive apt-get install --yes libevdev2 libevdev-dev libinput10 libinput-dev libwayland-client0 libwayland-dev libudev1 libudev-dev libxkbcommon0 libxkbcommon-dev pkg-config

make
