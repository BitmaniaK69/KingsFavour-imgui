#pragma once

//------------------------------------------------------------------------------------
void ensureDirectoryExists(const std::string& directoryPath) {
    try {
        if (!std::filesystem::exists(directoryPath)) {
            std::filesystem::create_directories(directoryPath);
            std::cout << "Directory created: " << directoryPath << std::endl;
        }
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error ensuring directory exists: " << e.what() << std::endl;
    }
}

std::vector<std::string> getConfigFiles(const std::string& directoryPath) {
    std::vector<std::string> files;

    try {
        if (!std::filesystem::exists(directoryPath)) {
            throw std::runtime_error("Directory does not exist: " + directoryPath);
        }

        for (const auto& entry : std::filesystem::directory_iterator(directoryPath)) {
            if (entry.is_regular_file()) {
                files.push_back(entry.path().filename().string());
            }
        }
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return files;
}
