/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <jsi/jsi.h>

#include <ATen/NativeFunctions.h>
#include <torch/csrc/jit/mobile/import.h>
#include <torch/csrc/jit/mobile/module.h>
#include <torch/script.h>
#include <string>
#include <vector>

#include "../torch/TensorHostObject.h"
#include "../torch/utils/helpers.h"
#include "AbstractScriptModule.h"
#include "CenterCropHostObject.h"
#include "NormalizeModule.h"
#include "ResizeHostObject.h"
#include "VisionTransformHostObject.h"

// Namespace alias for torch to avoid namespace conflicts with torchlive::torch
namespace torch_ = torch;

namespace torchlive {
namespace torchvision {
namespace transforms {

using namespace facebook;

// TransformsHostObject Method Name
static const std::string CENTER_CROP = "centerCrop";
static const std::string RESIZE = "resize";
static const std::string NORMALIZE = "normalize";

// TransformsHostObject Property Names
// empty

// TransformsHostObject Properties
static const std::vector<std::string> PROPERTIES = {};

// TransformsHostObject Methods
const std::vector<std::string> METHODS = {CENTER_CROP, RESIZE, NORMALIZE};

/*
 * This function returns a lambda factory function (i.e. transformFactoryFunc).
 * When the factory function is called, it loads the target operator's script
 * module, parses the parameters, and returns a exec function(i.e.
 * transformExecFunc). When the exec function is called, it parses the input
 * (tensor), and returns a transformed tensor.
 */
template <class T>
static jsi::Function createJITScriptModuleFactory(jsi::Runtime& runtime) {
  auto transformFactoryFunc = [](jsi::Runtime& runtimeFactory,
                                 const jsi::Value& thisValueFactory,
                                 const jsi::Value* argumentsFactory,
                                 size_t countFactory) -> jsi::Value {
    // ScriptModule would be loaded when transformFactoryFunc is called
    AbstractScriptModule* scriptModule = T::getInstance();
    auto params = scriptModule->parseParameters(
        runtimeFactory, thisValueFactory, argumentsFactory, countFactory);
    auto transformExecFunc = [params, scriptModule](
                                 jsi::Runtime& runtimeExec,
                                 const jsi::Value& thisValueExec,
                                 const jsi::Value* argumentsExec,
                                 size_t countExec) -> jsi::Value {
      auto inputs = scriptModule->parseInput(
          runtimeExec, thisValueExec, argumentsExec, countExec);
      inputs.insert(inputs.end(), params.begin(), params.end());
      auto transformed = scriptModule->forward(inputs).toTensor();
      auto transformedTensorHostObject =
          std::make_shared<torchlive::torch::TensorHostObject>(
              runtimeExec, transformed);
      return jsi::Object::createFromHostObject(
          runtimeExec, transformedTensorHostObject);
    };
    auto transform = jsi::Function::createFromHostFunction(
        runtimeFactory,
        jsi::PropNameID::forUtf8(runtimeFactory, T::moduleName),
        T::inputCount,
        transformExecFunc);
    // operator can be called with "op.forward(tensor)" or "op(tensor)"
    transform.setProperty(runtimeFactory, "forward", transform);
    return transform;
  };

  return jsi::Function::createFromHostFunction(
      runtime,
      jsi::PropNameID::forUtf8(runtime, T::moduleName),
      T::parameterCount,
      transformFactoryFunc);
}

VisionTransformHostObject::VisionTransformHostObject(jsi::Runtime& runtime)
    : centerCrop_(createCenterCrop(runtime)),
      resize_(createResize(runtime)),
      normalize_(createJITScriptModuleFactory<NormalizeModule>(runtime)) {}

std::vector<jsi::PropNameID> VisionTransformHostObject::getPropertyNames(
    jsi::Runtime& rt) {
  std::vector<jsi::PropNameID> result;
  for (std::string property : PROPERTIES) {
    result.push_back(jsi::PropNameID::forUtf8(rt, property));
  }
  for (std::string method : METHODS) {
    result.push_back(jsi::PropNameID::forUtf8(rt, method));
  }
  return result;
}

jsi::Value VisionTransformHostObject::get(
    jsi::Runtime& runtime,
    const jsi::PropNameID& propName) {
  auto name = propName.utf8(runtime);

  if (name == CENTER_CROP) {
    return jsi::Value(runtime, centerCrop_);
  } else if (name == NORMALIZE) {
    return jsi::Value(runtime, normalize_);
  } else if (name == RESIZE) {
    return jsi::Value(runtime, resize_);
  }

  return jsi::Value::undefined();
}

jsi::Function VisionTransformHostObject::createCenterCrop(
    jsi::Runtime& runtime) {
  auto centerCropFactoryFunc = [](jsi::Runtime& runtime,
                                  const jsi::Value& thisValue,
                                  const jsi::Value* arguments,
                                  size_t count) -> jsi::Value {
    if (count != 1) {
      throw jsi::JSError(
          runtime,
          "Factory function centerCrop expects 1 argument but " +
              std::to_string(count) + " are given.");
    }
    auto centerCropHostObject =
        std::make_shared<CenterCropHostObject>(runtime, arguments[0]);
    return jsi::Object::createFromHostObject(runtime, centerCropHostObject);
  };

  return jsi::Function::createFromHostFunction(
      runtime,
      jsi::PropNameID::forUtf8(runtime, CENTER_CROP),
      1,
      centerCropFactoryFunc);
}

jsi::Function VisionTransformHostObject::createResize(jsi::Runtime& runtime) {
  auto resizeFactoryFunc = [](jsi::Runtime& runtime,
                              const jsi::Value& thisValue,
                              const jsi::Value* arguments,
                              size_t count) -> jsi::Value {
    if (count != 1) {
      throw jsi::JSError(
          runtime,
          "Factory function resize expects 1 argument but " +
              std::to_string(count) + " are given.");
    }
    auto resizeHostObject =
        std::make_shared<ResizeHostObject>(runtime, arguments[0]);
    return jsi::Object::createFromHostObject(runtime, resizeHostObject);
  };

  return jsi::Function::createFromHostFunction(
      runtime, jsi::PropNameID::forUtf8(runtime, RESIZE), 1, resizeFactoryFunc);
}

} // namespace transforms
} // namespace torchvision
} // namespace torchlive