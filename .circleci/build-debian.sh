#!/usr/bin/env bash

#
# Build for Debian in a docker container
#

# bailout on errors and echo commands.
set -xe

CIRCLE_STAGE=$1
CPU_PLATF=${CIRCLE_STAGE:(-5)}
DOCKER_SOCK="unix:///var/run/docker.sock"

if [ "arm32" = "$CPU_PLATF" ]; then
  echo "DOCKER_OPTS=\"-H tcp://127.0.0.1:2375 -H $DOCKER_SOCK --experimental --storage-driver vfs\"" | sudo tee /etc/default/docker > /dev/null
else
  echo "DOCKER_OPTS=\"-H tcp://127.0.0.1:2375 -H $DOCKER_SOCK -s overlay2\"" | sudo tee /etc/default/docker > /dev/null
fi
sudo service docker restart
sleep 5;

if [ "$EMU" = "on" ]; then
  if [ "$CONTAINER_DISTRO" = "raspbian" ]; then
      docker run --rm --privileged --cap-add=ALL multiarch/qemu-user-static:register --reset
  else
      docker run --rm --privileged --cap-add=ALL --security-opt="seccomp=unconfined" multiarch/qemu-user-static --reset --credential yes --persistent yes
  fi
fi

WORK_DIR=$(pwd):/ci-source

docker run --privileged --cap-add=ALL --security-opt="seccomp=unconfined" -d -ti -e "container=docker" -v $WORK_DIR:rw $DOCKER_IMAGE /bin/bash
DOCKER_CONTAINER_ID=$(docker ps --last 4 | grep $CONTAINER_DISTRO | awk '{print $1}')

docker exec --privileged -ti $DOCKER_CONTAINER_ID apt-get update
docker exec --privileged -ti $DOCKER_CONTAINER_ID apt-get -y install apt-transport-https wget curl gnupg2

docker exec --privileged -ti $DOCKER_CONTAINER_ID /bin/bash -xec \
  "wget -q 'https://dl.cloudsmith.io/public/bbn-projects/bbn-repo/cfg/gpg/gpg.070C975769B2A67A.key' -O- | apt-key add -"
docker exec --privileged -ti $DOCKER_CONTAINER_ID /bin/bash -xec \
  "wget -q 'https://dl.cloudsmith.io/public/bbn-projects/bbn-repo/cfg/setup/config.deb.txt?distro=${PKG_DISTRO}&codename=${PKG_RELEASE}' -O- | tee -a /etc/apt/sources.list"

docker exec --privileged -ti $DOCKER_CONTAINER_ID apt-get update

docker exec --privileged -ti $DOCKER_CONTAINER_ID apt-get -y install build-essential libglib2.0-0 libglib2.0-bin libglib2.0-dev-bin \
   dpkg-dev debhelper devscripts equivs pkg-config apt-utils fakeroot

docker exec --privileged -ti $DOCKER_CONTAINER_ID apt-get -y install \
    dh-exec                           \
    meson                             \
    cmake                             \
    bc                                \
    bison                             \
    flex                              \
    at-spi2-core                      \
    libglib2.0-doc                    \
    libatk-bridge2.0-dev              \
    libatk1.0-dev                     \
    libcairo2-dev                     \
    libegl1-mesa-dev                  \
    libepoxy-dev                      \
    libfontconfig1-dev                \
    libfribidi-dev                    \
    libharfbuzz-dev                   \
    libpango1.0-dev                   \
    libwayland-dev                    \
    libxcomposite-dev                 \
    libxcursor-dev                    \
    libxdamage-dev                    \
    libxext-dev                       \
    libxfixes-dev                     \
    libxi-dev                         \
    libxinerama-dev                   \
    libxkbcommon-dev                  \
    libxml2-utils                     \
    libxrandr-dev                     \
    wayland-protocols                 \
    libatk1.0-doc                     \
    libpango1.0-doc                   \
    adwaita-icon-theme                \
    dh-sequence-gir                   \
    fonts-cantarell                   \
    gnome-pkg-tools                   \
    gobject-introspection             \
    gsettings-desktop-schemas         \
    gtk-doc-tools                     \
    libcolord-dev                     \
    libcups2-dev                      \
    libgdk-pixbuf2.0-dev              \
    libgirepository1.0-dev            \
    libjson-glib-dev                  \
    librest-dev                       \
    librsvg2-common                   \
    libxkbfile-dev                    \
    sassc                             \
    xauth                             \
    xvfb                              \
    docbook-xml                       \
    docbook-xsl                       \
    libcairo2-doc                     \
    libc6                             \
    libjson-glib-1.0-0                \
    libxcomposite1                    \
    xsltproc

docker exec --privileged -ti $DOCKER_CONTAINER_ID ldconfig

#docker exec --privileged -ti $DOCKER_CONTAINER_ID apt-get -y upgrade

docker exec --privileged -ti $DOCKER_CONTAINER_ID /bin/bash -xec \
    "update-alternatives --set fakeroot /usr/bin/fakeroot-tcp; cd ci-source; export DEB_BUILD_OPTIONS=\"noopts nodocs nocheck notest\"; DEB_CXXFLAGS_SET=\"-g -O0\" DEB_CPPFLAGS_SET=\"-g -O0\" DEB_CFLAGS_SET=\"-g -O0\" dpkg-buildpackage -b -uc -us -j2; mkdir dist; mv ../*.deb dist; chmod -R a+rw dist"

find dist -name \*.$EXT

echo "Stopping"
docker ps -a
docker stop $DOCKER_CONTAINER_ID
docker rm -v $DOCKER_CONTAINER_ID
