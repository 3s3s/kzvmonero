// Copyright (c) 2020, The Monero Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "updater.h"

#include <common/util.h>

#include <openpgp/hash.h>

#include "network.h"
#include "utils.h"

Updater::Updater()
{
    /////KZV (maintainers))////
    /*m_maintainers.emplace_back(fileGetContents(":/monero/utils/gpg_keys/binaryfate.asc").toStdString());
    m_maintainers.emplace_back(fileGetContents(":/monero/utils/gpg_keys/fluffypony.asc").toStdString());
    m_maintainers.emplace_back(fileGetContents(":/monero/utils/gpg_keys/luigi1111.asc").toStdString());*/
    m_maintainers.emplace_back(fileGetContents(":/monero/utils/gpg_keys/usdx1.asc").toStdString());
    m_maintainers.emplace_back(fileGetContents(":/monero/utils/gpg_keys/usdx2.asc").toStdString());
    //////////////////
}

QByteArray Updater::fetchSignedHash(
    const QString &binaryFilename,
    const QByteArray &hashFromDns,
    QPair<QString, QString> &signers) const
{
    ////KZV (hashes)////
    //static constexpr const char hashesTxtUrl[] = "https://web.getmonero.org/downloads/hashes.txt";
    //static constexpr const char hashesTxtSigUrl[] = "https://web.getmonero.org/downloads/hashes.txt.sig";
    static constexpr const char hashesTxtUrl[] = "https://raw.githubusercontent.com/3s3s/kzvmonero/main/hashes.txt";
    static constexpr const char hashesTxtSigUrl[] = "https://raw.githubusercontent.com/3s3s/kzvmonero/main/hashes.txt.sig";
    //////////////////////////////

    const Network network;
    std::string hashesTxt = network.get(hashesTxtUrl);
    std::string hashesTxtSig = network.get(hashesTxtSigUrl);

    const QByteArray signedHash = verifyParseSignedHahes(
        QByteArray(&hashesTxt[0], hashesTxt.size()),
        QByteArray(&hashesTxtSig[0], hashesTxtSig.size()),
        binaryFilename,
        signers);

    if (signedHash != hashFromDns)
    {
        throw std::runtime_error("DNS hash mismatch");
    }

    return signedHash;
}

QByteArray Updater::verifyParseSignedHahes(
    const QByteArray &armoredSignedHashes,
    const QByteArray &secondDetachedSignature,
    const QString &binaryFilename,
    QPair<QString, QString> &signers) const
{
    std::cout << "Updater::Updater::verifyParseSignedHahes step 1\r\n";
    const QString signedMessage = verifySignature(armoredSignedHashes, signers.first);

    std::wcout << "Updater::verifyParseSignedHahes step 2 \r\n";
    signers.second = verifySignature(
        epee::span<const uint8_t>(
            reinterpret_cast<const uint8_t *>(armoredSignedHashes.data()),
            armoredSignedHashes.size()),
        openpgp::signature_rsa::from_buffer(epee::span<const uint8_t>(
            reinterpret_cast<const uint8_t *>(secondDetachedSignature.data()),
            secondDetachedSignature.size())));

    std::cout << "Updater::verifyParseSignedHahes step 3\r\n";
    if (signers.first == signers.second)
    {
        throw std::runtime_error("both signatures were generated by the same person");
    }

    return parseShasumOutput(signedMessage, binaryFilename);
}

QPair<QString, QString> Updater::verifySignaturesAndHashSum(
    const QByteArray &armoredSignedHashes,
    const QByteArray &secondDetachedSignature,
    const QString &binaryFilename,
    const void *binaryData,
    size_t binarySize) const
{
    QPair<QString, QString> signers;
    const QByteArray signedHash =
        verifyParseSignedHahes(armoredSignedHashes, secondDetachedSignature, binaryFilename, signers);
    const QByteArray calculatedHash = getHash(binaryData, binarySize);
    if (signedHash != calculatedHash)
    {
        throw std::runtime_error("hash sum mismatch");
    }

    return signers;
}

QByteArray Updater::getHash(const void *data, size_t size) const
{
    QByteArray hash(sizeof(crypto::hash), 0);
    tools::sha256sum(static_cast<const uint8_t *>(data), size, *reinterpret_cast<crypto::hash *>(hash.data()));
    return hash;
}

QByteArray Updater::parseShasumOutput(const QString &message, const QString &filename) const
{
    for (const auto &line : message.splitRef("\n"))
    {
        const auto trimmed = line.trimmed();
        if (trimmed.endsWith(filename))
        {
            const int pos = trimmed.indexOf(' ');
            if (pos != -1)
            {
                return QByteArray::fromHex(trimmed.left(pos).toUtf8());
            }
        }
        else if (trimmed.startsWith(filename))
        {
            const int pos = trimmed.lastIndexOf(' ');
            if (pos != -1)
            {
                return QByteArray::fromHex(trimmed.right(trimmed.size() - pos).toUtf8());
            }
        }
    }

    throw std::runtime_error("hash not found");
}

QString Updater::verifySignature(const QByteArray &armoredSignedMessage, QString &signer) const
{
    const std::string messageString = armoredSignedMessage.toStdString();
    const openpgp::message_armored signedMessage(messageString);
    signer = verifySignature(signedMessage, openpgp::signature_rsa::from_armored(messageString));

    const epee::span<const uint8_t> message = signedMessage;
    return QString(QByteArray(reinterpret_cast<const char *>(&message[0]), message.size()));
}

QString Updater::verifySignature(const epee::span<const uint8_t> data, const openpgp::signature_rsa &signature) const
{
    for (const auto &maintainer : m_maintainers)
    {
        for (const auto &public_key : maintainer)
        {
            if (signature.verify(data, public_key))
            {
                return QString::fromStdString(maintainer.user_id());
            }
        }
    }

    throw std::runtime_error("not signed by a maintainer");
}
