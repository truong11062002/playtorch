/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <jni.h>

#include <fbjni/ByteBuffer.h>
#include <fbjni/fbjni.h>

#include <torchlive/media/Blob.h>
#include <torchlive/media/NativeJSRefBridge.h>

namespace torchlive {
namespace media {

using namespace facebook;
using namespace jni;

alias_ref<JClass> getJBlobUtilsClass() {
  static const auto JBlobUtilsClass =
      findClassStatic("org/pytorch/rn/core/media/BlobUtils");
  return JBlobUtilsClass;
}

std::shared_ptr<IImage>
imageFromBlob(const Blob& blob, double width, double height) {
  return nullptr;
}

std::unique_ptr<torchlive::media::Blob> toBlob(const std::string& refId) {
  auto blobUtilsClass = getJBlobUtilsClass();
  static const auto nativeJSRefToByteBufferMethod =
      blobUtilsClass->getStaticMethod<local_ref<JByteBuffer>(std::string)>(
          "nativeJSRefToByteBuffer");
  local_ref<JByteBuffer> buffer =
      nativeJSRefToByteBufferMethod(blobUtilsClass, refId);

  uint8_t* const bytes = buffer->getDirectBytes();
  size_t const size = buffer->getDirectSize();
  auto data = std::make_unique<uint8_t[]>(size);
  std::memcpy(data.get(), bytes, size);
  return std::make_unique<torchlive::media::Blob>(std::move(data), size);
}

} // namespace media
} // namespace torchlive
