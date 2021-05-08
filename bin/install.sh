#!/bin/bash
#
# chromecastmanager pi install script
#

GIT="https://github.com/trumpton/ChromecastManager.git"
DEST="/tmp/$(whoami)/ChromecastManager"
APP="chromecastmanager"
APPDBG="chromecastmanager-dbg"
CHECK="$DEST/src/chromecastmanager.c"

echo ""
echo "ChromecastManager Install"
echo "-------------------------"
echo ""

# Check dependencies

if [[ "$(sudo which gcc)" == "" ]]; then
  echo "GCC is not installed."
  sudo apt-get install gcc
else
  echo "dependency check ... OK"
fi

# Check user

if [[ "$(grep $APP /etc/passwd)" == "" ]]; then
  echo "Creating $APP user"
  sudo useradd -Md / -s /usr/sbin/nologin $APP
else
  echo "Checking $APP user ... OK"
fi

# Unpack to /tmp

if [ ! -f "$CHECK" ]; then
  echo "Downloading from GIT"
  mkdir -p "$DEST"
  cd "$DEST/.."
  git clone $GIT
fi

if [ ! -f "$CHECK" ]; then
  echo "$APP source not found in $DEST - aborting"
  exit
fi

# Build

echo "Building the executable ..."
(cd "$DEST/src" ; make clean ; make ; make debug )
if [ ! -f "$DEST/src/$APP" ]; then
  echo "FATAL ERROR, building $APP executable failed"
  exit
fi

# Disable Service (if running)
sudo systemctl stop $APP > /dev/null 2>&1

# Transfer Files

echo "Transferring files ... "
echo "   /usr/bin/$APP"
sudo cp "$DEST/src/$APP" "/usr/bin/$APP"
sudo mkdir -p "/etc/$APP"

echo "   /etc/$APP/"
sudo cp $DEST/conf/* "/etc/$APP"

echo "   /lib/systemd/system/$APP.service"
sudo cp "$DEST/system/$APP.service" "/lib/systemd/system/$APP.service"

if [ -f "$DEST/src/$APPDBG" ]; then
  echo "   /usr/bin/$APPDBG"
  sudo cp "$DEST/src/$APPDBG" "/usr/bin/$APPDBG"
fi

# Enable Service

echo "Enabling $APP service ..."
sudo systemctl enable $APP

# Report

echo ""
echo "Installation Complete"
echo "---------------------"
echo ""
echo "Edit the configuration file in /etc/$APP"
echo ""
echo "Start the service with: systemctl start $APP"
echo ""

