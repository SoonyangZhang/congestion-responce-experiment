// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_CRYPTO_AES_128_GCM_ENCRYPTER_H_
#define NET_THIRD_PARTY_QUIC_CORE_CRYPTO_AES_128_GCM_ENCRYPTER_H_

#include "base/macros.h"
#include "net/third_party/quic/core/crypto/aead_base_encrypter.h"
#include "net/third_party/quic/platform/api/quic_export.h"

namespace quic {

// An Aes128GcmEncrypter is a QuicEncrypter that implements the
// AEAD_AES_128_GCM algorithm specified in RFC 5116 for use in IETF QUIC.
//
// It uses an authentication tag of 16 bytes (128 bits). It uses a 12 byte IV
// that is XOR'd with the packet number to compute the nonce.
class QUIC_EXPORT_PRIVATE Aes128GcmEncrypter : public AeadBaseEncrypter {
 public:
  enum {
    kAuthTagSize = 16,
  };

  Aes128GcmEncrypter();
  Aes128GcmEncrypter(const Aes128GcmEncrypter&) = delete;
  Aes128GcmEncrypter& operator=(const Aes128GcmEncrypter&) = delete;
  ~Aes128GcmEncrypter() override;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_CRYPTO_AES_128_GCM_ENCRYPTER_H_
