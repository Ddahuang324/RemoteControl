// FileController.h: 头文件
//

#pragma once
#include <string>
#include <memory>
#include "../model/Interface.h"

class FileController {
public:
    explicit FileController(std::shared_ptr<IFileSystemModel> model = nullptr);
    void uploadFile(const std::string& filePath);
    void downloadFile(const std::string& filePath);
    void deleteFile(const std::string& filePath);
    void runFile(const std::string& filePath);
    void setModel(std::shared_ptr<IFileSystemModel> model);

private:
    std::shared_ptr<IFileSystemModel> model_;
};