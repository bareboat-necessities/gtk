#!/usr/bin/env bash

#
# Build for Debian in a docker container
#

# bailout on errors and echo commands.
set -xe

DOCKER_SOCK="unix:///var/run/docker.sock"

echo "DOCKER_OPTS=\"-H tcp://127.0.0.1:2375 -H $DOCKER_SOCK -s devicemapper\"" | sudo tee /etc/default/docker > /dev/null
sudo service docker restart
sleep 5;

if [ "$EMU" = "on" ]; then
  if [ "$CONTAINER_DISTRO" = "raspbian" ]; then
      docker run --rm --privileged multiarch/qemu-user-static:register --reset
  else
      docker run --rm --privileged multiarch/qemu-user-static --reset -p yes
  fi
fi

WORK_DIR=$(pwd):/ci-source

docker run --privileged -d -ti -e "container=docker"  -v $WORK_DIR:rw $DOCKER_IMAGE /bin/bash
DOCKER_CONTAINER_ID=$(docker ps --last 4 | grep $CONTAINER_DISTRO | awk '{print $1}')

docker exec --privileged -ti $DOCKER_CONTAINER_ID apt-get update
docker exec --privileged -ti $DOCKER_CONTAINER_ID apt-get -y install dh-exec meson cmake \
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
    xsltproc

GDK_PIX_VER="2.40.0+dfsg-5"
PRG_REPO=bbn-projects/bbn-repo/deb/raspbian/pool/buster
PKG_SRC=https://dl.cloudsmith.io/public/${PRG_REPO}/main

docker exec --privileged -ti $DOCKER_CONTAINER_ID /bin/bash -xec \
    eval \`dpkg-architecture\`; \
    wget ${PKG_SRC}/l/li/libgdk-pixbuf2.0-bin_${GDK_PIX_VER}_${DEB_TARGET_ARCH}.deb; \
    wget ${PKG_SRC}/l/li/libgdk-pixbuf2.0-common_${GDK_PIX_VER}_all.deb; \
    wget ${PKG_SRC}/l/li/libgdk-pixbuf2.0-0_${GDK_PIX_VER}_${DEB_TARGET_ARCH}.deb; \
    wget ${PKG_SRC}/g/gi/gir1.2-gdkpixbuf-2.0_${GDK_PIX_VER}_${DEB_TARGET_ARCH}.deb; \
    wget ${PKG_SRC}/l/li/libgdk-pixbuf2.0-dev_${GDK_PIX_VER}_${DEB_TARGET_ARCH}.deb; \
    dpkg -i libgdk-pixbuf2.0-bin_${GDK_PIX_VER}_${DEB_TARGET_ARCH}.deb; \
    dpkg -i libgdk-pixbuf2.0-${GDK_PIX_VER}_all.deb; \
    dpkg -i libgdk-pixbuf2.0-0_${GDK_PIX_VER}_${DEB_TARGET_ARCH}.deb; \
    dpkg -i gir1.2-gdkpixbuf-2.0_${GDK_PIX_VER}_${DEB_TARGET_ARCH}.deb; \
    dpkg -i libgdk-pixbuf2.0-dev_${GDK_PIX_VER}_${DEB_TARGET_ARCH}.deb

docker exec -ti $DOCKER_CONTAINER_ID /bin/bash -xec \
    "cd ci-source; dpkg-buildpackage -b -uc -us; mkdir dist; mv ../*.deb dist; chmod -R a+rw dist"

find dist -name \*.deb

echo "Stopping"
docker ps -a
docker stop $DOCKER_CONTAINER_ID
docker rm -v $DOCKER_CONTAINER_ID
