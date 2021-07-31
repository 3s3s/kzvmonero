#!/bin/bash

DISTR_VERSION=0.17.2.3
SIGNER1=usdx1
SIGNER2=usdx2

DISTR_NAME=linux-x64

FOR_LINUX_RELEASE=$HOME/monero-gui/build/release/bin
FOR_LINUX_OUTPUT=$HOME/monero-gui

cd $FOR_LINUX_RELEASE
tar -cvjSf $FOR_LINUX_OUTPUT/monero-gui-$DISTR_NAME-v$DISTR_VERSION.tar.bz2 *
cd $FOR_LINUX_OUTPUT

rm -f hashes.txt
rm -f hashes.txt.sig

echo "##GUI" >> hashes_unsigned.txt
sha256sum monero-gui-$DISTR_NAME-v$DISTR_VERSION.tar.bz2 >> hashes_unsigned.txt
echo "# signed by $SIGNER1" >> hashes_unsigned.txt

#sign hashes to text file (first signer)
gpg -u $SIGNER1 --clearsign --digest-algo SHA256 --output hashes.txt hashes_unsigned.txt
rm -f hashes_unsigned.txt

#sign signed hashes to binary file (second signer)
gpg -u $SIGNER2 --detach-sign --digest-algo SHA256 --output hashes.txt.sig hashes.txt

