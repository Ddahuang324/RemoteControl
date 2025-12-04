#include "pch.h"
#include "FileController.h"
#include <iostream>
#include "FileController.h"
#include <iostream>

FileController::FileController(std::shared_ptr<IFileSystemModel> model)
    : model_(std::move(model)) {}

void FileController::setModel(std::shared_ptr<IFileSystemModel> model) {
    model_ = std::move(model);
}

void FileController::uploadFile(const std::string& filePath) {
    std::cout << "FileController: uploadFile not supported yet: " << filePath << std::endl;
    if (!model_) return;
    // short-term: model returns not supported
    model_->uploadFile(filePath, "", nullptr, [](bool ok, const std::string &err) {
        if (ok) std::cout << "Upload succeeded" << std::endl;
        else std::cout << "Upload failed: " << err << std::endl;
    });
}

void FileController::downloadFile(const std::string& filePath) {
    std::cout << "FileController: starting download: " << filePath << std::endl;
    if (!model_) return;
    auto handle = model_->downloadFile(filePath, std::string("./") + filePath,
        [](int percent) { std::cout << "Download progress: " << percent << "%\n"; },
        [](bool ok, const std::string &err) {
            if (ok) std::cout << "Download finished" << std::endl;
            else std::cout << "Download error: " << err << std::endl;
        });
    if (handle) std::cout << "Download started, id=" << handle->id() << std::endl;
}

void FileController::deleteFile(const std::string& filePath) {
    std::cout << "FileController: deleteFile: " << filePath << std::endl;
    if (!model_) return;
    model_->deleteFile(filePath, [](bool ok, const std::string &err) {
        if (ok) std::cout << "Delete succeeded" << std::endl;
        else std::cout << "Delete failed: " << err << std::endl;
    });
}

void FileController::runFile(const std::string& filePath) {
    std::cout << "FileController: runFile: " << filePath << std::endl;
    if (!model_) return;
    model_->runFile(filePath, [](bool ok, const std::string &err) {
        if (ok) std::cout << "Run succeeded" << std::endl;
        else std::cout << "Run failed: " << err << std::endl;
    });
}