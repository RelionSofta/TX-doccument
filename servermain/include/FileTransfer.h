#pragma once
#include<fstream>
#include<string>
#include<cstdint>
#include<filesystem>
#include<array>
class FileHandler {
private:
	std::ifstream input_;
	std::ofstream output_;
	std::filesystem::path filePath{};

public:
	static constexpr size_t CHUNK_SIZE = 2 * 1024 * 1024; // 2 MiB

	FileHandler();

	~FileHandler();

	bool openRead(const std::string& path);

	bool openWrite(const std::string& path);

	uint64_t fileSize(const std::string& path);

	std::size_t readChunk(std::array<char, CHUNK_SIZE>& buffer);
	void writeChunk(const char* data, std::size_t size);

	void close();
};