#include <iostream>
#include <fstream>
#include <filesystem>
#include <regex>
#include <vector>
#include <set>
#include <memory>
#include <string>

namespace fs = std::filesystem;

bool fileExistsInFolder(const std::string &folderPath, const std::string &filename) {
    return fs::exists(fs::path(folderPath) / filename);
}

bool fileWithExtensionExists(const std::string &folderPath, const std::string &extension) {
    for (const auto &entry : fs::recursive_directory_iterator(folderPath)) {
        if (entry.is_regular_file() && entry.path().extension() == extension)
            return true;
    }
    return false;
}

class LanguageHandler {
public:
    virtual bool detect(const std::string &folderPath) = 0;
    virtual std::set<std::string> extractDependencies(const std::string &folderPath) = 0;
    virtual std::string generateDockerfile(const std::string &folderPath,
                                             const std::set<std::string> &deps) = 0;
    virtual std::string getName() const = 0;
    virtual ~LanguageHandler() {}
};

class PythonHandler : public LanguageHandler {
public:
    std::string getName() const override { return "Python"; }
    
    bool detect(const std::string &folderPath) override {
        if (fileExistsInFolder(folderPath, "requirements.txt"))
            return true;
        if (fileWithExtensionExists(folderPath, ".py"))
            return true;
        return false;
    }
    
    std::set<std::string> extractDependencies(const std::string &folderPath) override {
        std::set<std::string> deps;
        std::regex importRegex("^\\s*(import|from)\\s+([a-zA-Z0-9_]+)");
        for (const auto &entry : fs::recursive_directory_iterator(folderPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".py") {
                std::ifstream file(entry.path());
                std::string line;
                while (std::getline(file, line)) {
                    std::smatch match;
                    if (std::regex_search(line, match, importRegex))
                        deps.insert(match[2]);
                }
            }
        }
        return deps;
    }
    
    std::string generateDockerfile(const std::string &folderPath, const std::set<std::string> &deps) override {
        std::string docker;
        docker += "FROM python:3.9\n";
        docker += "WORKDIR /app\n";
        docker += "COPY . /app\n";
        if (fileExistsInFolder(folderPath, "requirements.txt"))
            docker += "RUN pip install --upgrade pip && pip install -r requirements.txt\n";
        else if (!deps.empty()) {
            docker += "RUN pip install --upgrade pip && pip install";
            for (const auto &dep : deps)
                docker += " " + dep;
            docker += "\n";
        }
        docker += "CMD [\"python\", \"main.py\"]\n";
        return docker;
    }
};

class NodeHandler : public LanguageHandler {
public:
    std::string getName() const override { return "Node.js"; }
    
    bool detect(const std::string &folderPath) override {
        if (fileExistsInFolder(folderPath, "package.json"))
            return true;
        if (fileWithExtensionExists(folderPath, ".js") || fileWithExtensionExists(folderPath, ".ts"))
            return true;
        return false;
    }
    
    std::set<std::string> extractDependencies(const std::string &folderPath) override {
        std::set<std::string> deps;
        std::regex requireRegex("require\\(['\"]([^\\.][^'\"]*)['\"]\\)");
        std::regex importRegex("import\\s+.*?['\"]([^\\.][^'\"]*)['\"]");
        for (const auto &entry : fs::recursive_directory_iterator(folderPath)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                if (ext == ".js" || ext == ".ts") {
                    std::ifstream file(entry.path());
                    std::string line;
                    while (std::getline(file, line)) {
                        std::smatch match;
                        if (std::regex_search(line, match, requireRegex))
                            deps.insert(match[1]);
                        else if (std::regex_search(line, match, importRegex))
                            deps.insert(match[1]);
                    }
                }
            }
        }
        return deps;
    }
    
    std::string generateDockerfile(const std::string &folderPath, const std::set<std::string> &deps) override {
        std::string docker;
        docker += "FROM node:14\n";
        docker += "WORKDIR /app\n";
        docker += "COPY . /app\n";
        if (fileExistsInFolder(folderPath, "package.json"))
            docker += "RUN npm install\n";
        else if (!deps.empty()) {
            docker += "RUN npm install";
            for (const auto &dep : deps)
                docker += " " + dep;
            docker += "\n";
        }
        docker += "CMD [\"npm\", \"start\"]\n";
        return docker;
    }
};

class JavaHandler : public LanguageHandler {
public:
    std::string getName() const override { return "Java"; }
    
    bool detect(const std::string &folderPath) override {
        if (fileExistsInFolder(folderPath, "pom.xml") || fileExistsInFolder(folderPath, "build.gradle"))
            return true;
        if (fileWithExtensionExists(folderPath, ".java"))
            return true;
        return false;
    }
    
    std::set<std::string> extractDependencies(const std::string &folderPath) override {
        std::set<std::string> deps;
        std::regex importRegex("^\\s*import\\s+([a-zA-Z0-9_\\.]+)");
        for (const auto &entry : fs::recursive_directory_iterator(folderPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".java") {
                std::ifstream file(entry.path());
                std::string line;
                while (std::getline(file, line)) {
                    std::smatch match;
                    if (std::regex_search(line, match, importRegex))
                        deps.insert(match[1]);
                }
            }
        }
        return deps;
    }
    
    std::string generateDockerfile(const std::string &folderPath, const std::set<std::string> &deps) override {
        std::string docker;
        docker += "FROM openjdk:11\n";
        docker += "WORKDIR /app\n";
        docker += "COPY . /app\n";
        if (fileExistsInFolder(folderPath, "pom.xml")) {
            docker += "RUN mvn install\n";
            docker += "CMD [\"java\", \"-jar\", \"target/app.jar\"]\n";
        } else if (fileExistsInFolder(folderPath, "build.gradle")) {
            docker += "RUN gradle build\n";
            docker += "CMD [\"java\", \"-jar\", \"build/libs/app.jar\"]\n";
        } else {
            docker += "# TODO: Java 빌드 명령어 추가\n";
        }
        return docker;
    }
};

class RubyHandler : public LanguageHandler {
public:
    std::string getName() const override { return "Ruby"; }
    
    bool detect(const std::string &folderPath) override {
        if (fileExistsInFolder(folderPath, "Gemfile"))
            return true;
        if (fileWithExtensionExists(folderPath, ".rb"))
            return true;
        return false;
    }
    
    std::set<std::string> extractDependencies(const std::string &folderPath) override {
        std::set<std::string> deps;
        std::regex requireRegex("require\\s+['\"]([^'\"]+)['\"]");
        for (const auto &entry : fs::recursive_directory_iterator(folderPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".rb") {
                std::ifstream file(entry.path());
                std::string line;
                while (std::getline(file, line)) {
                    std::smatch match;
                    if (std::regex_search(line, match, requireRegex)) {
                        std::string dep = match[1];
                        if (!dep.empty() && dep[0] != '.')
                            deps.insert(dep);
                    }
                }
            }
        }
        return deps;
    }
    
    std::string generateDockerfile(const std::string &folderPath, const std::set<std::string> &deps) override {
        std::string docker;
        docker += "FROM ruby:2.7\n";
        docker += "WORKDIR /app\n";
        docker += "COPY . /app\n";
        if (fileExistsInFolder(folderPath, "Gemfile"))
            docker += "RUN bundle install\n";
        else if (!deps.empty()) {
            docker += "RUN gem install";
            for (const auto &dep : deps)
                docker += " " + dep;
            docker += "\n";
        }
        docker += "CMD [\"ruby\", \"main.rb\"]\n";
        return docker;
    }
};

class PHPHandler : public LanguageHandler {
public:
    std::string getName() const override { return "PHP"; }
    
    bool detect(const std::string &folderPath) override {
        if (fileExistsInFolder(folderPath, "composer.json"))
            return true;
        if (fileWithExtensionExists(folderPath, ".php"))
            return true;
        return false;
    }
    
    std::set<std::string> extractDependencies(const std::string &folderPath) override {
        std::set<std::string> deps;
        std::regex requireRegex("(require|include)\\s*\\(?\\s*['\"]([^'\"]+)['\"]");
        for (const auto &entry : fs::recursive_directory_iterator(folderPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".php") {
                std::ifstream file(entry.path());
                std::string line;
                while (std::getline(file, line)) {
                    std::smatch match;
                    if (std::regex_search(line, match, requireRegex))
                        deps.insert(match[2]);
                }
            }
        }
        return deps;
    }
    
    std::string generateDockerfile(const std::string &folderPath, const std::set<std::string> &deps) override {
        std::string docker;
        docker += "FROM php:7.4-apache\n";
        docker += "WORKDIR /var/www/html\n";
        docker += "COPY . /var/www/html\n";
        if (fileExistsInFolder(folderPath, "composer.json"))
            docker += "RUN composer install\n";
        else if (!deps.empty()) {
            docker += "RUN composer require";
            for (const auto &dep : deps)
                docker += " " + dep;
            docker += "\n";
        }
        docker += "CMD [\"apache2-foreground\"]\n";
        return docker;
    }
};

class GoHandler : public LanguageHandler {
public:
    std::string getName() const override { return "Go"; }
    
    bool detect(const std::string &folderPath) override {
        if (fileExistsInFolder(folderPath, "go.mod"))
            return true;
        if (fileWithExtensionExists(folderPath, ".go"))
            return true;
        return false;
    }
    
    std::set<std::string> extractDependencies(const std::string &folderPath) override {
        std::set<std::string> deps;
        std::regex importRegex("^\\s*import\\s+\"([^\"]+)\"");
        for (const auto &entry : fs::recursive_directory_iterator(folderPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".go") {
                std::ifstream file(entry.path());
                std::string line;
                while (std::getline(file, line)) {
                    std::smatch match;
                    if (std::regex_search(line, match, importRegex))
                        deps.insert(match[1]);
                }
            }
        }
        return deps;
    }
    
    std::string generateDockerfile(const std::string &folderPath, const std::set<std::string> &deps) override {
        std::string docker;
        docker += "FROM golang:1.16\n";
        docker += "WORKDIR /app\n";
        docker += "COPY . /app\n";
        if (fileExistsInFolder(folderPath, "go.mod"))
            docker += "RUN go mod download\n";
        docker += "RUN go build -o main .\n";
        docker += "CMD [\"./main\"]\n";
        return docker;
    }
};

class CSharpHandler : public LanguageHandler {
public:
    std::string getName() const override { return "C# (.NET)"; }
    
    bool detect(const std::string &folderPath) override {
        if (fileWithExtensionExists(folderPath, ".cs"))
            return true;
        for (const auto &entry : fs::directory_iterator(folderPath)) {
            if (entry.is_regular_file()) {
                std::string fname = entry.path().filename().string();
                if (fname.size() >= 6 && fname.substr(fname.size() - 6) == ".csproj")
                    return true;
                if (fname.size() >= 4 && fname.substr(fname.size() - 4) == ".sln")
                    return true;
            }
        }
        return false;
    }
    
    std::set<std::string> extractDependencies(const std::string &folderPath) override {
        return {};
    }
    
    std::string generateDockerfile(const std::string &folderPath, const std::set<std::string> &deps) override {
        std::string docker;
        docker += "FROM mcr.microsoft.com/dotnet/sdk:5.0\n";
        docker += "WORKDIR /app\n";
        docker += "COPY . /app\n";
        docker += "RUN dotnet restore\n";
        docker += "RUN dotnet build\n";
        docker += "CMD [\"dotnet\", \"run\"]\n";
        return docker;
    }
};

class CppHandler : public LanguageHandler {
public:
    std::string getName() const override { return "C++"; }
    
    bool detect(const std::string &folderPath) override {
        if (fileWithExtensionExists(folderPath, ".cpp") ||
            fileWithExtensionExists(folderPath, ".cc") ||
            fileWithExtensionExists(folderPath, ".cxx"))
            return true;
        return false;
    }
    
    std::set<std::string> extractDependencies(const std::string &folderPath) override {
        return {};
    }
    
    std::string generateDockerfile(const std::string &folderPath, const std::set<std::string> &deps) override {
        std::string docker;
        docker += "FROM gcc:latest\n";
        docker += "WORKDIR /app\n";
        docker += "COPY . /app\n";
        docker += "RUN g++ -o main *.cpp\n";
        docker += "CMD [\"./main\"]\n";
        return docker;
    }
};

class RustHandler : public LanguageHandler {
public:
    std::string getName() const override { return "Rust"; }
    
    bool detect(const std::string &folderPath) override {
        if (fileExistsInFolder(folderPath, "Cargo.toml"))
            return true;
        if (fileWithExtensionExists(folderPath, ".rs"))
            return true;
        return false;
    }
    
    std::set<std::string> extractDependencies(const std::string &folderPath) override {
        return {};
    }
    
    std::string generateDockerfile(const std::string &folderPath, const std::set<std::string> &deps) override {
        std::string docker;
        docker += "FROM rust:latest\n";
        docker += "WORKDIR /app\n";
        docker += "COPY . /app\n";
        if (fileExistsInFolder(folderPath, "Cargo.toml"))
            docker += "RUN cargo build --release\n";
        else
            docker += "# Cargo.toml 파일을 추가하여 의존성 관리를 해주세요\n";
        docker += "CMD [\"./target/release/<your_binary>\"]\n";
        return docker;
    }
};

void displayBanner() {
    std::cout << R"(               
    ____ ______   ________________ _/  |_  ___________ 
   /  _ \\____ \_/ __ \_  __ \__  \\   __\/  _ \_  __ \
  (  <_> )  |_> >  ___/|  | \// __ \|  | (  <_> )  | \/
   \____/|   __/ \___  >__|  (____  /__|  \____/|__|   
         |__|        \/           \/                   
  
  developed by seoyulh92
  MIT License
  0.0.1
)" << "\n";
}

void initializeLanguageListFile() {
    fs::path langFilePath("langlist.operator");
    if (!fs::exists(langFilePath)) {
        std::ofstream out(langFilePath);
        if (out.is_open()) {
            out << "Python\n";
            out << "Node.js\n";
            out << "Java\n";
            out << "Ruby\n";
            out << "PHP\n";
            out << "Go\n";
            out << "C# (.NET)\n";
            out << "C++\n";
            out << "Rust\n";
            std::cout << "[langlist.operator 파일이 생성되었습니다. 기본 지원 언어가 저장되었습니다.]\n";
            std::cout << "파일 경로: " << fs::absolute(langFilePath) << "\n\n";
        }
        out.close();
    }
}

void addNewLanguage() {
    std::cin.ignore();
    std::cout << "\n추가할 새로운 언어 이름을 입력하세요: ";
    std::string newLang;
    std::getline(std::cin, newLang);
    if (newLang.empty()) {
        std::cout << "입력값이 없습니다.\n";
        return;
    }
    
    fs::path langFilePath("langlist.operator");
    std::ofstream out(langFilePath, std::ios::app);
    if (out.is_open()) {
        out << newLang << "\n";
        out.close();
        std::cout << "새로운 언어가 추가되었습니다: " << newLang << "\n";
    } else {
        std::cerr << "langlist.operator 파일에 접근할 수 없습니다.\n";
    }
}

void makeDockerfileOperation() {
    std::cout << "\n==== Operator ====\n";
    std::cout << "프로젝트 폴더 경로를 입력하세요: ";
    std::string folderPath;
    std::cin.ignore();
    std::getline(std::cin, folderPath);
    
    while (!fs::exists(folderPath) || !fs::is_directory(folderPath)) {
        std::cout << "유효하지 않은 폴더입니다. 다시 입력하세요: ";
        std::getline(std::cin, folderPath);
    }
    
    std::vector<std::unique_ptr<LanguageHandler>> handlers;
    handlers.push_back(std::make_unique<PythonHandler>());
    handlers.push_back(std::make_unique<NodeHandler>());
    handlers.push_back(std::make_unique<JavaHandler>());
    handlers.push_back(std::make_unique<RubyHandler>());
    handlers.push_back(std::make_unique<PHPHandler>());
    handlers.push_back(std::make_unique<GoHandler>());
    handlers.push_back(std::make_unique<CSharpHandler>());
    handlers.push_back(std::make_unique<CppHandler>());
    handlers.push_back(std::make_unique<RustHandler>());
    
    std::vector<LanguageHandler*> candidates;
    for (auto &handler : handlers) {
        if (handler->detect(folderPath))
            candidates.push_back(handler.get());
    }
    
    if (candidates.empty()) {
        std::cerr << "지원하는 언어가 감지되지 않았습니다. (Unsupported project)\n";
        return;
    }
    
    std::string dockerContent;
    if (candidates.size() == 1) {
        auto handler = candidates[0];
        std::cout << "감지된 언어: " << handler->getName() << "\n";
        auto dependencies = handler->extractDependencies(folderPath);
        if (!dependencies.empty()) {
            std::cout << "\n=== 감지된 라이브러리 (" << handler->getName() << ") ===\n";
            for (const auto &dep : dependencies)
                std::cout << "  - " << dep << "\n";
            std::cout << "===========================\n\n";
        } else {
            std::cout << "\n자동 감지된 라이브러리가 없습니다 (" << handler->getName() << ").\n\n";
        }
        dockerContent = handler->generateDockerfile(folderPath, dependencies);
    } else {
        std::cout << "여러 언어가 감지되었습니다. 모든 언어에 대한 Dockerfile 내용을 생성합니다.\n";
        dockerContent = "";
        for (auto handler : candidates) {
            auto dependencies = handler->extractDependencies(folderPath);
            std::cout << "\n[" << handler->getName() << "] 감지된 라이브러리:\n";
            if (!dependencies.empty()) {
                for (const auto &dep : dependencies)
                    std::cout << "  - " << dep << "\n";
            } else {
                std::cout << "  없음\n";
            }
            dockerContent += "\n# ===== " + handler->getName() + " Stage =====\n";
            dockerContent += handler->generateDockerfile(folderPath, dependencies);
            dockerContent += "\n";
        }
    }
    
    fs::path dockerfilePath = fs::path(folderPath) / "Dockerfile";
    std::ofstream outFile(dockerfilePath);
    if (!outFile) {
        std::cerr << "Dockerfile을 생성하지 못했습니다.\n";
        return;
    }
    outFile << dockerContent;
    outFile.close();
    
    std::cout << "Dockerfile이 생성되었습니다: " << dockerfilePath.string() << "\n";
    std::cout << "\nOperator 프로세스가 완료되었습니다. 해당 프로젝트는 Docker 컨테이너에서 실행될 준비가 되었습니다!\n";
}

int main() {
    displayBanner();
    initializeLanguageListFile();
    
    std::cout << "1 - make a dockerfile\n";
    std::cout << "2 - add a new language\n";
    std::cout << "선택: ";
    
    int option = 0;
    std::cin >> option;
    
    switch (option) {
        case 1:
            makeDockerfileOperation();
            break;
        case 2:
            addNewLanguage();
            break;
        default:
            std::cerr << "잘못된 선택입니다.\n";
            break;
    }
    
    return 0;
}
