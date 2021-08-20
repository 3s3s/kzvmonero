#!/bin/bash

DISTR_VERSION=0.17.2.3
SIGNER1=usdxsec
SIGNER2=usdx2sec

DISTR_NAME=linux-x64

FOR_LINUX_RELEASE=$HOME/kzvmonero/build/release/bin
FOR_LINUX_OUTPUT=$HOME/kzvmonero

if [ -z $1 ]; then
    echo "The first argument is not set. Will try to sign the existion file $FOR_LINUX_OUTPUT/monero-gui-$DISTR_NAME-v$DISTR_VERSION.tar.bz2"
else
    echo "The first argument: $1"
    cd $FOR_LINUX_RELEASE
    tar -cvjSf $FOR_LINUX_OUTPUT/monero-gui-$DISTR_NAME-v$DISTR_VERSION.tar.bz2 *
    cd $FOR_LINUX_OUTPUT
fi

#cd $FOR_LINUX_RELEASE
#tar -cvjSf $FOR_LINUX_OUTPUT/monero-gui-$DISTR_NAME-v$DISTR_VERSION.tar.bz2 *
#cd $FOR_LINUX_OUTPUT

rm -f hashes.txt
rm -f hashes.txt.sig

echo "##GUI" >> hashes_unsigned.txt
SHA256_LINUX64=$(sha256sum -b monero-gui-$DISTR_NAME-v$DISTR_VERSION.tar.bz2)
echo $SHA256_LINUX64 >> hashes_unsigned.txt
echo "# DNS TXT record should by: monero-gui:$DISTR_NAME:$DISTR_VERSION:$SHA256_LINUX64" >> hashes_unsigned.txt
echo "# signed by $SIGNER1" >> hashes_unsigned.txt

#sign hashes to text file (first signer)
gpg -u $SIGNER1 --clearsign --digest-algo SHA256 --output hashes.txt hashes_unsigned.txt
rm -f hashes_unsigned.txt

#sign signed hashes to binary file (second signer)
gpg -u $SIGNER2 --detach-sign --digest-algo SHA256 --output hashes.txt.sig hashes.txt

