#include"FileTransfer.h"
#include <array>

FileHandler::FileHandler() = default;
FileHandler:: ~FileHandler() {
	close(); 
}
bool FileHandler::openRead(const std::string& path) {
	close();
	input_.open(path , std::ios::binary);
	return input_.is_open();
}

bool FileHandler::openWrite(const std::string& path) {
	close();
	output_.open(path, std::ios::binary);
	return output_.is_open();
}
uint64_t FileHandler::fileSize(const std::string& path) {
	if (!input_.is_open()) {
		return 0;
	}
	try {
		return std::filesystem::file_size(path);
	}
	catch (const std::filesystem::filesystem_error&)
	{
		return 0;
	}
}
std::size_t FileHandler::readChunk(std::array<char, CHUNK_SIZE>& buffer) {
	input_.read(buffer.data() , CHUNK_SIZE);
	return input_.gcount();
}
void FileHandler::writeChunk(const char* data, std::size_t size){
	output_.write(data , size);
}
void FileHandler::close()
{
	if (input_.is_open())
		input_.close();

	if (output_.is_open())
		output_.close();
}