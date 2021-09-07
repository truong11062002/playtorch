/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

import Foundation

@objc(MobileModelModule)
public class MobileModelModule: NSObject {

    enum MobileModelModuleError: Error {
        case DownloadError
        case ModuleCreationError
        case ImageUnwrapError
    }

    private var mModulesAndSpecs: [String: ModuleHolder] = [:]

    @objc(execute:params:resolver:rejecter:)
    public func execute(_ modelPath: NSString, params: NSDictionary, resolver resolve: @escaping RCTPromiseResolveBlock, rejecter reject: @escaping RCTPromiseRejectBlock) {
        let modelKey = getKey(path: modelPath as String)
        if let moduleHolder = mModulesAndSpecs[modelKey] {
            let packer = BaseIValuePacker()
            guard let modelSpec = moduleHolder.modelSpec else { return }
            let startPack = CFAbsoluteTimeGetCurrent()
            do {
                if var tensorWrapper =  try packer.pack(params: params, modelSpec: modelSpec) as? TensorWrapper {
                    let packTime = (CFAbsoluteTimeGetCurrent() - startPack) * 1000
                    let startInference = CFAbsoluteTimeGetCurrent()
                    if let outputs = moduleHolder.module?.predictImage(tensorWrapper, outputType: modelSpec.unpack.dtype) {
                        let inferenceTime = (CFAbsoluteTimeGetCurrent() - startInference) * 1000
                        let startUnpack = CFAbsoluteTimeGetCurrent()
                        let result = try packer.unpack(outputs: outputs, modelSpec: modelSpec)
                        let unpackTime = (CFAbsoluteTimeGetCurrent() - startUnpack) * 1000
                        let metrics = ["totalTime": packTime + inferenceTime + unpackTime, "packTime": packTime, "inferenceTime": inferenceTime, "unpackTime": unpackTime]
                        resolve(["result": result, "metrics": metrics])
                    }
                } else {
                    reject(RCTErrorUnspecified, "Could not run inference on packed inputs", nil)
                }
            } catch {
                reject(RCTErrorUnspecified, "\(error)", error)
            }
        } else {
            let completionHandler: (String?) -> Void  = { error in
                if let error = error {
                    reject(RCTErrorUnspecified, error, nil)
                } else {
                    self.execute(modelPath, params: params, resolver: resolve, rejecter: reject)
                }
            }
            mModulesAndSpecs[modelKey] = ModuleHolder()
            fetchCacheAndLoadModel(modelUri: modelPath as String, completionHandler: completionHandler)
        }
    }

    @objc(preload:resolver:rejecter:)
    public func preload(_ modelUri: NSString, resolver resolve: @escaping RCTPromiseResolveBlock, rejecter reject: @escaping RCTPromiseRejectBlock) {
        let modelKey = getKey(path: modelUri as String)
        mModulesAndSpecs[modelKey] = ModuleHolder()
        let completionHandler: (String?) -> Void  = { error in
            if let error = error {
                reject(RCTErrorUnspecified, error, nil)
            } else {
                resolve(nil)
            }
        }
        fetchCacheAndLoadModel(modelUri: modelUri as String, completionHandler: completionHandler)
    }

    func fetchCacheAndLoadModel(modelUri: String, completionHandler: @escaping (String?) -> Void) {
        let modelKey = getKey(path: modelUri)
        if let modelUrl = URL(string: modelUri) {
            let modelTask = URLSession.shared.downloadTask(with: modelUrl) { urlOrNil, responseOrNil, errorOrNil in
                guard let tempURL = urlOrNil else { completionHandler("Error downloading file"); return }

                // Try to fetch live.spec.json from model file
                let extraFiles = NSMutableDictionary()
                extraFiles.setValue("", forKey: "model/live.spec.json")

                // Note: regardless what initial value is set for the key "model/live.spec.json", the
                // TorchModule.load method will set an empty string if the model file is not bundled inside the
                // model file.
                if let module = TorchModule.load(tempURL.path, extraFiles: extraFiles) {
                    self.mModulesAndSpecs[modelKey]?.setModule(module: module)

                    let modelSpec = extraFiles["model/live.spec.json"] as? String ?? ""
                    if (!modelSpec.isEmpty) {
                        do {
                            let data = Data(modelSpec.utf8)
                            let jsonDecoder = JSONDecoder()
                            let decodedModelSpec = try jsonDecoder.decode(ModelSpecification.self, from: data)
                            self.mModulesAndSpecs[modelKey]?.setSpec(modelSpec: decodedModelSpec)
                            completionHandler(nil)
                        }
                        catch {
                            completionHandler("could not fetch json file: \(error)")
                        }
                    } else {
                        self.fetchModelSpec(modelUri: modelUri, completionHandler: completionHandler)
                    }
                } else {
                    completionHandler("Could not convert downloaded file into Torch Module")
                }
            }
            modelTask.resume()
        } else {
            completionHandler("Could not create URLSession with provided URL")
        }
    }

    func fetchModelSpec(modelUri: String, completionHandler: @escaping (String?) -> Void) {
        let modelKey = getKey(path: modelUri)
        guard var modelUrl = URL(string: modelUri) else { completionHandler("Could not load live spec"); return }
        let newLastComponent = modelUrl.lastPathComponent + ".live.spec.json"
        modelUrl.deleteLastPathComponent()
        modelUrl.appendPathComponent(newLastComponent)
        let specTask = URLSession.shared.downloadTask(with: modelUrl) { urlOrNil, responseOrNil, errorOrNil in
            guard let tempURL = urlOrNil else { completionHandler("Could not load live spec"); return }
            do {
                let jsonString = try String(contentsOfFile: tempURL.path)
                let data = Data(jsonString.utf8)
                let jsonDecoder = JSONDecoder()
                let decodedModelSpec = try jsonDecoder.decode(ModelSpecification.self, from: data)
                self.mModulesAndSpecs[modelKey]?.setSpec(modelSpec: decodedModelSpec)
                completionHandler(nil) //argument represents error, completionHandler(nil) represents success and will resolve promise
            } catch {
                completionHandler("could not fetch json file: \(error)")
            }
        }
        specTask.resume()
    }

    public class ModuleHolder {
        var module: TorchModule?
        var modelSpec: ModelSpecification?

        func setModule(module: TorchModule) {
            self.module = module
        }

        func setSpec(modelSpec: ModelSpecification) {
            self.modelSpec = modelSpec
        }
    }

    func getKey(path: String) -> String {
        var modelKey: String
        if let key = path.components(separatedBy: "/").last {
            modelKey = key
        } else {
            modelKey = path
        }
        return modelKey
    }
}
