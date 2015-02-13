/** @file qpolarsslpki_priv.hpp
  * Qt wrapper class around polarssl functions
  *
  * @copyright (C) 2015, azadkuh
  * @date 2015.02.12
  * @version 1.0.0
  * @author amir zamani <azadkuh@live.com>
  *
 */

#ifndef QPOLARSSL_PKI_PRIV_HPP
#define QPOLARSSL_PKI_PRIV_HPP

#include <QFile>
#include "polarssl/pk.h"
#include "qpolarsslhash_priv.hpp"
#include "qpolarsslrandom_priv.hpp"
///////////////////////////////////////////////////////////////////////////////
namespace qpolarssl {
namespace priv {
///////////////////////////////////////////////////////////////////////////////
class Pki
{
public:
    explicit        Pki(pk_type_t t = POLARSSL_PK_NONE)
        : Pki(pk_info_from_type(t)) {
    }

    explicit        Pki(const pk_info_t* pinfo) {
        if ( pinfo != nullptr ) {
            itype   = pinfo->type;
            pk_init_ctx(context(), pinfo);
        } else {
            pk_init(context());
        }
    }

    virtual        ~Pki() {
        pk_free(context());
    }

    auto            context() -> pk_context* {
        return &ictx;
    }

    auto            context()const -> const pk_context* {
        return &ictx;
    }

    bool            isValid()const {
        return itype != POLARSSL_PK_NONE;
    }

    void            reset() {
        pk_free(context());
    }

    size_t          keySizeBits()const {
        return pk_get_size(context());
    }

    size_t          keySizeBytes()const {
        return pk_get_len(context());
    }

    bool            canDo(pk_type_t type) {
        return pk_can_do(context(), type) == 1;
    }

    auto            type()const -> pk_type_t {
        return pk_get_type(context());
    }

    auto            name()const -> const char* {
        return pk_get_name(context());
    }

    Random&         random() {
        return irandom;
    }

public:
    int             parseKey(const QByteArray& keyData,
                             const QByteArray& password = QByteArray()) {
        auto key = reinterpret_cast<const uint8_t*>(keyData.constData());
        auto pwd = reinterpret_cast<const uint8_t*>(
                       (password.length() > 0 ) ? password.constData() : nullptr
                                                  );

        reset();
        int nRet = pk_parse_key(context(),
                                key, keyData.length(),
                                pwd, password.length()
                                );
        if ( nRet != 0 )
            qDebug("pk_parse_key() failed. error: -0x%X", -nRet);

        return nRet;
    }

    int             parsePublicKey(const QByteArray& keyData) {
        auto key = reinterpret_cast<const uint8_t*>(keyData.constData());

        reset();
        int nRet = pk_parse_public_key(context(),
                                       key, keyData.length()
                                       );
        if ( nRet != 0 )
            qDebug("pk_parse_public_key() failed. error: -0x%X", -nRet);

        return nRet;
    }

    int             parseKeyFrom(const QString& filePath,
                                 const QByteArray& password = QByteArray()) {
        QFile f(filePath);
        QByteArray keyData;
        if ( f.open(QFile::ReadOnly) )
            keyData = f.readAll();

        return parseKey(keyData, password);
    }

    int             parsePublicKeyFrom(const QString& filePath) {
        QFile f(filePath);
        QByteArray keyData;
        if ( f.open(QFile::ReadOnly) )
            keyData = f.readAll();

        return parsePublicKey(keyData);
    }

public:
    auto            sign(const QByteArray& message,
                         md_type_t algorithm) -> QByteArray {
        auto hash = prepare(message, algorithm);
        uint8_t buffer[POLARSSL_MPI_MAX_SIZE] = {0};
        size_t  olen = 0;
        int nRet = pk_sign(context(),
                           algorithm,
                           reinterpret_cast<const uint8_t*>(hash.constData()),
                           0,
                           buffer,
                           &olen,
                           ctr_drbg_random,
                           irandom.context()
                           );

        if ( nRet != 0 ) {
            qDebug("pk_sign() failed. error: -0x%X", -nRet);
            return QByteArray();
        }

        return QByteArray((const char*)buffer, olen);
    }

    int             verify(const QByteArray& message,
                           const QByteArray& signature,
                           md_type_t algorithm) {
        auto hash = prepare(message, algorithm);
        return pk_verify(context(),
                         algorithm,
                         reinterpret_cast<const uint8_t*>(hash.constData()),
                         0,
                         reinterpret_cast<const uint8_t*>(signature.constData()),
                         signature.length()
                         );
    }

    auto            encrypt(const QByteArray& hash) -> QByteArray {
        if ( !checkSize(hash) ) {
            qDebug("invalid hash size for encryption!");
            return QByteArray();
        }

        uint8_t buffer[POLARSSL_MPI_MAX_SIZE] = {0};
        size_t  olen = 0;
        int nRet = pk_encrypt(context(),
                              reinterpret_cast<const uint8_t*>(hash.constData()),
                              hash.length(),
                              buffer,
                              &olen,
                              POLARSSL_MPI_MAX_SIZE,
                              ctr_drbg_random,
                              irandom.context()
                              );
        if ( nRet != 0 ) {
            qDebug("pk_encrypt() failed. error: -0x%X", -nRet);
            return QByteArray();
        }

        return QByteArray((const char*)buffer, olen);
    }

    auto            decrypt(const QByteArray& hash) -> QByteArray {
        if ( !checkSize(hash) ) {
            qDebug("invalid hash size for decryption!");
            return QByteArray();
        }

        uint8_t buffer[POLARSSL_MPI_MAX_SIZE] = {0};
        size_t  olen = 0;
        int nRet = pk_decrypt(context(),
                              reinterpret_cast<const uint8_t*>(hash.constData()),
                              hash.length(),
                              buffer,
                              &olen,
                              POLARSSL_MPI_MAX_SIZE,
                              ctr_drbg_random,
                              irandom.context()
                              );
        if ( nRet != 0 ) {
            qDebug("pk_decrypt() failed. error: -0x%X", -nRet);
            return QByteArray();
        }

        return QByteArray((const char*)buffer, olen);
    }

protected:
    /// checks if the message needs to be converted to a hash.
    auto            prepare(const QByteArray& message,
                            md_type_t algo)const -> QByteArray {
        int   maxLength  = pk_get_len(context());
        return  (message.length() < maxLength   &&  algo == POLARSSL_MD_NONE)
                           ? message : Hash::hash(message, algo);
    }

    /// checks if the size of hash is ok for encryption/decryption.
    bool            checkSize(const QByteArray& hash)const {
        int   maxLength  = pk_get_len(context());
        return maxLength >= hash.length();
    }

protected:
    Q_DISABLE_COPY(Pki)

    Random              irandom;
    pk_type_t           itype = POLARSSL_PK_NONE;
    pk_context          ictx;
};
///////////////////////////////////////////////////////////////////////////////
} // namespace priv
} // namespace qpolarssl
///////////////////////////////////////////////////////////////////////////////
#endif // QPOLARSSL_PKI_PRIV_HPP